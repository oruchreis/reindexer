#include "joinedselector.h"

#include "core/namespace/namespaceimpl.h"
#include "core/queryresults/joinresults.h"
#include "estl/restricted.h"
#include "nsselecter.h"
#include "vendor/sparse-map/sparse_set.h"

constexpr size_t kMaxIterationsScaleForInnerJoinOptimization = 100;

namespace reindexer {

void JoinedSelector::selectFromRightNs(QueryResults& joinItemR, const Query& query, bool& found, bool& matchedAtLeastOnce) {
	assertrx_dbg(rightNs_);

	JoinCacheRes joinResLong;
	rightNs_->getFromJoinCache(query, joinQuery_, joinResLong);

	rightNs_->getInsideFromJoinCache(joinRes_);
	if (joinRes_.needPut) {
		rightNs_->putToJoinCache(joinRes_, preSelectCtx_.ResultPtr());
	}
	if (joinResLong.haveData) {
		found = joinResLong.it.val.ids->size();
		matchedAtLeastOnce = joinResLong.it.val.matchedAtLeastOnce;
		rightNs_->FillResult(joinItemR, *joinResLong.it.val.ids);
	} else {
		SelectCtxWithJoinPreSelect<JoinPreResultExecuteCtx> ctx(query, nullptr, preSelectCtx_);
		ctx.matchedAtLeastOnce = false;
		ctx.reqMatchedOnceFlag = true;
		ctx.skipIndexesLookup = true;
		ctx.functions = &selectFunctions_;
		rightNs_->Select(joinItemR, ctx, rdxCtx_);
		if (query.NeedExplain()) {
			explainOneSelect_ = std::move(joinItemR.explainResults);
			joinItemR.explainResults = {};
		}

		found = joinItemR.Count();
		matchedAtLeastOnce = ctx.matchedAtLeastOnce;
	}
	if (joinResLong.needPut) {
		JoinCacheVal val;
		val.ids = make_intrusive<intrusive_atomic_rc_wrapper<IdSet>>();
		val.matchedAtLeastOnce = matchedAtLeastOnce;
		for (auto& r : joinItemR.Items()) {
			val.ids->Add(r.Id(), IdSet::Unordered, 0);
		}
		rightNs_->putToJoinCache(joinResLong, std::move(val));
	}
}

void JoinedSelector::selectFromPreResultValues(QueryResults& joinItemR, const Query& query, bool& found, bool& matchedAtLeastOnce) const {
	size_t matched = 0;
	const auto& entries = query.Entries();
	const JoinPreResult::Values& values = std::get<JoinPreResult::Values>(PreResult().payload);
	const auto& pt = values.payloadType;
	for (const ItemRef& item : values) {
		auto& v = item.Value();
		assertrx_throw(!v.IsFree());
		if (entries.CheckIfSatisfyConditions({pt, v})) {
			if (++matched > query.Limit()) {
				break;
			}
			found = true;
			joinItemR.Add(item);
		}
	}
	matchedAtLeastOnce = matched;
}

bool JoinedSelector::Process(IdType rowId, int nsId, ConstPayload payload, bool match) {
	++called_;
	if (optimized_ && !match) {
		matched_++;
		return true;
	}

	const auto startTime = ExplainCalc::Clock::now();
	// Put values to join conditions
	size_t i = 0;
	if (itemQuery_.NeedExplain() && !explainOneSelect_.empty()) {
		itemQuery_.Explain(false);
	}
	std::unique_ptr<Query> itemQueryCopy;
	Query* itemQueryPtr = &itemQuery_;
	for (auto& je : joinQuery_.joinEntries_) {
		QueryEntry& qentry = itemQueryPtr->GetUpdatableEntry<QueryEntry>(i);
		{
			auto keyValues = qentry.UpdatableValues(QueryEntry::IgnoreEmptyValues{});
			payload.GetByFieldsSet(je.LeftFields(), keyValues, je.LeftFieldType(), je.LeftCompositeFieldsTypes());
		}
		if (qentry.Values().empty()) {
			if (itemQueryPtr == &itemQuery_) {
				itemQueryCopy = std::make_unique<Query>(itemQuery_);
				itemQueryPtr = itemQueryCopy.get();
			}
			itemQueryPtr->SetEntry<AlwaysFalse>(i);
		}
		++i;
	}
	itemQueryPtr->Limit(match ? joinQuery_.Limit() : 0);

	bool found = false;
	bool matchedAtLeastOnce = false;
	QueryResults joinItemR;
	std::visit(
		overloaded{[&](const JoinPreResult::Values&) { selectFromPreResultValues(joinItemR, *itemQueryPtr, found, matchedAtLeastOnce); },
				   Restricted<IdSet, SelectIteratorContainer>{}(
					   [&](const auto&) { selectFromRightNs(joinItemR, *itemQueryPtr, found, matchedAtLeastOnce); })},
		PreResult().payload);
	if (match && found) {
		assertrx_throw(nsId < static_cast<int>(result_.joined_.size()));
		joins::NamespaceResults& nsJoinRes = result_.joined_[nsId];
		assertrx_dbg(nsJoinRes.GetJoinedSelectorsCount());
		nsJoinRes.Insert(rowId, joinedFieldIdx_, std::move(joinItemR));
	}
	if (matchedAtLeastOnce) {
		++matched_;
	}
	selectTime_ += (ExplainCalc::Clock::now() - startTime);
	return matchedAtLeastOnce;
}

template <typename Cont, typename Fn>
VariantArray JoinedSelector::readValuesOfRightNsFrom(const Cont& data, const Fn& createPayload, const QueryJoinEntry& entry,
													 const PayloadType& pt) const {
	const auto rightFieldType = entry.RightFieldType();
	const auto leftFieldType = entry.LeftFieldType();
	VariantArray res;
	if (rightFieldType.Is<KeyValueType::Composite>()) {
		unordered_payload_ref_set set(data.size(), hash_composite_ref(pt, entry.RightFields()),
									  equal_composite_ref(pt, entry.RightFields()));
		for (const auto& v : data) {
			const auto pl = createPayload(v);
			if (!pl.Value()->IsFree()) {
				set.insert(*pl.Value());
			}
		}
		res.reserve(set.size());
		for (auto& s : set) {
			res.emplace_back(std::move(s));
		}
	} else {
		tsl::sparse_set<Variant> set(data.size());
		for (const auto& val : data) {
			const auto pl = createPayload(val);
			if (pl.Value()->IsFree()) {
				continue;
			}
			pl.GetByFieldsSet(entry.RightFields(), res, entry.RightFieldType(), entry.RightCompositeFieldsTypes());
			if (!leftFieldType.Is<KeyValueType::Undefined>() && !leftFieldType.Is<KeyValueType::Composite>()) {
				for (Variant& v : res) {
					set.insert(std::move(v.convert(leftFieldType)));
				}
			} else {
				for (Variant& v : res) {
					set.insert(std::move(v));
				}
			}
		}
		res.clear<false>();
		for (auto& s : set) {
			res.emplace_back(std::move(s));
		}
	}
	return res;
}

VariantArray JoinedSelector::readValuesFromPreResult(const QueryJoinEntry& entry) const {
	const JoinPreResult::Values& values = std::get<JoinPreResult::Values>(PreResult().payload);
	return readValuesOfRightNsFrom(
		values, [&values](const ItemRef& item) noexcept { return ConstPayload{values.payloadType, item.Value()}; }, entry,
		values.payloadType);
}

void JoinedSelector::AppendSelectIteratorOfJoinIndexData(SelectIteratorContainer& iterators, int* maxIterations, unsigned sortId,
														 const SelectFunction::Ptr& selectFnc, const RdxContext& rdxCtx) {
	const auto& preresult = PreResult();
	if (joinType_ != JoinType::InnerJoin || preSelectCtx_.Mode() != JoinPreSelectMode::Execute ||
		std::visit(overloaded{[](const SelectIteratorContainer&) noexcept { return true; },
							  Restricted<IdSet, JoinPreResult::Values>{}([maxIterations](const auto& v) noexcept {
								  return v.size() > *maxIterations * kMaxIterationsScaleForInnerJoinOptimization;
							  })},
				   preresult.payload)) {
		return;
	}
	unsigned optimized = 0;
	assertrx_throw(!std::holds_alternative<JoinPreResult::Values>(preresult.payload) ||
				   itemQuery_.Entries().Size() == joinQuery_.joinEntries_.size());
	for (size_t i = 0; i < joinQuery_.joinEntries_.size(); ++i) {
		const QueryJoinEntry& joinEntry = joinQuery_.joinEntries_[i];
		if (!joinEntry.IsLeftFieldIndexed() || joinEntry.Operation() != OpAnd ||
			(joinEntry.Condition() != CondEq && joinEntry.Condition() != CondSet) ||
			(i + 1 < joinQuery_.joinEntries_.size() && joinQuery_.joinEntries_[i + 1].Operation() == OpOr)) {
			continue;
		}
		const auto& leftIndex = leftNs_->indexes_[joinEntry.LeftIdxNo()];
		assertrx_throw(!IsFullText(leftIndex->Type()));

		// Avoiding to use 'GetByJsonPath' during values extraction
		// TODO: Sometimes this substitution may be effective even with 'GetByJsonPath', so we should allow user to hint this optimization.
		bool hasSparse = false;
		for (int field : joinEntry.RightFields()) {
			if (field == SetByJsonPath) {
				hasSparse = true;
				break;
			}
		}
		if (hasSparse) {
			continue;
		}

		const VariantArray values =
			std::visit(overloaded{[&](const IdSet& preselected) {
									  const std::vector<IdType>* sortOrders = nullptr;
									  if (preresult.sortOrder.index) {
										  sortOrders = &(preresult.sortOrder.index->SortOrders());
									  }
									  return readValuesOfRightNsFrom(
										  preselected,
										  [this, sortOrders](IdType rowId) noexcept {
											  const auto properRowId = sortOrders ? (*sortOrders)[rowId] : rowId;
											  return ConstPayload{rightNs_->payloadType_, rightNs_->items_[properRowId]};
										  },
										  joinEntry, rightNs_->payloadType_);
								  },
								  [&](const JoinPreResult::Values&) { return readValuesFromPreResult(joinEntry); },
								  [](const SelectIteratorContainer&) -> VariantArray { throw_as_assert; }},
					   preresult.payload);
		auto ctx = selectFnc ? selectFnc->CreateCtx(joinEntry.LeftIdxNo()) : BaseFunctionCtx::Ptr{};
		assertrx_throw(!ctx || ctx->type != BaseFunctionCtx::CtxType::kFtCtx);

		if (leftIndex->Opts().GetCollateMode() == CollateUTF8) {
			for (auto& key : values) {
				key.EnsureUTF8();
			}
		}
		Index::SelectOpts opts;
		opts.maxIterations = iterators.GetMaxIterations();
		opts.indexesNotOptimized = !leftNs_->SortOrdersBuilt();
		opts.inTransaction = inTransaction_;

		auto selectResults = leftIndex->SelectKey(values, CondSet, sortId, opts, ctx, rdxCtx);
		if (auto* selRes = std::get_if<SelectKeyResultsVector>(&selectResults)) {
			bool wasAppended = false;
			for (SelectKeyResult& res : *selRes) {
				SelectIterator selIter{std::move(res), false, std::string(joinEntry.LeftFieldName()),
									   (joinEntry.LeftIdxNo() < 0 ? IteratorFieldKind::NonIndexed : IteratorFieldKind::Indexed), false};
				const int curIterations = selIter.GetMaxIterations();
				if (curIterations && curIterations < *maxIterations) {
					*maxIterations = curIterations;
				}
				iterators.Append(OpAnd, std::move(selIter));
				wasAppended = true;
			}
			if (wasAppended) {
				++optimized;
			}
		}
	}
	optimized_ = optimized == joinQuery_.joinEntries_.size();
}

}  // namespace reindexer
