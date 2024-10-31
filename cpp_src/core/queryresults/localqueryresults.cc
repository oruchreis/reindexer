#include "localqueryresults.h"
#include "additionaldatasource.h"
#include "cluster/sharding/sharding.h"
#include "core/cbinding/resultserializer.h"
#include "core/cjson/csvbuilder.h"
#include "core/cjson/msgpackbuilder.h"
#include "core/cjson/protobufbuilder.h"
#include "core/itemimpl.h"
#include "core/namespace/namespace.h"
#include "joinresults.h"
#include "server/outputparameters.h"
#include "tools/catch_and_return.h"

namespace reindexer {

void LocalQueryResults::AddNamespace(NamespaceImplPtr ns, [[maybe_unused]] bool noLock) {
	assertrx(noLock);
	const NamespaceImpl* nsPtr = ns.get();
	auto strHolder = ns->strHolder();
	const auto it = std::find_if(nsData_.cbegin(), nsData_.cend(), [nsPtr](const NsDataHolder& nsData) { return nsData.ns == nsPtr; });
	if (it != nsData_.cend()) {
		assertrx(it->strHolder.get() == strHolder.get());
		return;
	}
	nsData_.emplace_back(std::move(ns), std::move(strHolder));
}

void LocalQueryResults::AddNamespace(NamespaceImpl* ns, [[maybe_unused]] bool noLock) {
	assertrx(noLock);
	auto strHolder = ns->strHolder();
	const auto it = std::find_if(nsData_.cbegin(), nsData_.cend(), [ns](const NsDataHolder& nsData) { return nsData.ns == ns; });
	if (it != nsData_.cend()) {
		assertrx(it->strHolder.get() == strHolder.get());
		return;
	}
	nsData_.emplace_back(ns, std::move(strHolder));
}

void LocalQueryResults::RemoveNamespace(const NamespaceImpl* ns) {
	const auto it = std::find_if(nsData_.begin(), nsData_.end(), [ns](const NsDataHolder& nsData) { return nsData.ns == ns; });
	assertrx(it != nsData_.end());
	nsData_.erase(it);
}

struct LocalQueryResults::Context {
	Context() = default;
	Context(PayloadType type, TagsMatcher tagsMatcher, const FieldsSet& fieldsFilter, std::shared_ptr<const Schema> schema,
			lsn_t nsIncarnationTag)
		: type_(std::move(type)),
		  tagsMatcher_(std::move(tagsMatcher)),
		  fieldsFilter_(fieldsFilter),
		  schema_(std::move(schema)),
		  nsIncarnationTag_(nsIncarnationTag) {}

	PayloadType type_;
	TagsMatcher tagsMatcher_;
	FieldsSet fieldsFilter_;
	std::shared_ptr<const Schema> schema_;
	lsn_t nsIncarnationTag_;
};

static_assert(LocalQueryResults::kSizeofContext >= sizeof(LocalQueryResults::Context),
			  "LocalQueryResults::kSizeofContext should >= sizeof(LocalQueryResults::Context)");

LocalQueryResults::LocalQueryResults(std::initializer_list<ItemRef> l) : items_(l) {}
LocalQueryResults::LocalQueryResults() = default;
LocalQueryResults::LocalQueryResults(LocalQueryResults&& obj) noexcept = default;

LocalQueryResults::LocalQueryResults(const ItemRefVector::const_iterator& begin, const ItemRefVector::const_iterator& end)
	: items_(begin, end) {}

LocalQueryResults& LocalQueryResults::operator=(LocalQueryResults&& obj) noexcept = default;

LocalQueryResults::~LocalQueryResults() = default;

void LocalQueryResults::Clear() { *this = LocalQueryResults(); }

void LocalQueryResults::Erase(ItemRefVector::iterator start, ItemRefVector::iterator finish) { items_.erase(start, finish); }

void LocalQueryResults::Add(const ItemRef& i) { items_.push_back(i); }

// Used to save strings when converting the client result to the server.
// The server item is created, inserted into the result and deleted
// so that the rows are not deleted, they are saved in the results.
void LocalQueryResults::SaveRawData(ItemImplRawData&& rawData) { rawDataHolder_.emplace_back(std::move(rawData)); }

std::string LocalQueryResults::Dump() const {
	std::string buf;
	for (size_t i = 0; i < items_.size(); ++i) {
		if (&items_[i] != &*items_.begin()) {
			buf += ",";
		}
		buf += std::to_string(items_[i].Id());
		if (joined_.empty()) {
			continue;
		}
		Iterator itemIt{this, int(i), Error(), {}};
		auto joinIt = itemIt.GetJoined();
		if (joinIt.getJoinedItemsCount() > 0) {
			buf += "[";
			for (auto fieldIt = joinIt.begin(); fieldIt != joinIt.end(); ++fieldIt) {
				if (fieldIt != joinIt.begin()) {
					buf += ";";
				}
				for (int j = 0; j < fieldIt.ItemsCount(); ++j) {
					if (j != 0) {
						buf += ",";
					}
					buf += std::to_string(fieldIt[j].Id());
				}
			}
			buf += "]";
		}
	}
	return buf;
}

h_vector<std::string_view, 1> LocalQueryResults::GetNamespaces() const {
	h_vector<std::string_view, 1> ret;
	ret.reserve(ctxs.size());
	for (auto& ctx : ctxs) {
		ret.push_back(ctx.type_.Name());
	}
	return ret;
}

NsShardsIncarnationTags LocalQueryResults::GetIncarnationTags() const {
	NsShardsIncarnationTags ret;
	auto& shardTags = ret.emplace_back();
	shardTags.tags.reserve(ctxs.size());
	for (auto& ctx : ctxs) {
		shardTags.tags.emplace_back(ctx.nsIncarnationTag_);
	}
	return ret;
}

int LocalQueryResults::GetJoinedNsCtxIndex(int nsid) const noexcept {
	int ctxIndex = joined_.size();
	for (int ns = 0; ns < nsid; ++ns) {
		ctxIndex += joined_[ns].GetJoinedSelectorsCount();
	}
	return ctxIndex;
}

class LocalQueryResults::EncoderDatasourceWithJoins final : public IEncoderDatasourceWithJoins {
public:
	EncoderDatasourceWithJoins(const joins::ItemIterator& joinedItemIt, const ContextsVector& ctxs, Iterator::NsNamesCache& nsNamesCache,
							   int ctxIdx, size_t nsid, size_t joinedCount) noexcept
		: joinedItemIt_(joinedItemIt), ctxs_(ctxs), nsNamesCache_(nsNamesCache), ctxId_(ctxIdx), nsid_{nsid} {
		if (nsNamesCache.size() <= nsid_) {
			nsNamesCache.resize(nsid_ + 1);
		}
		if (nsNamesCache[nsid_].size() < joinedCount) {
			nsNamesCache[nsid_].clear();
			nsNamesCache[nsid_].reserve(joinedCount);
			fast_hash_map<std::string_view, int> namesCounters;
			assertrx_dbg(ctxs_.size() >= ctxId_ + joinedCount);
			for (size_t i = ctxId_, end = ctxId_ + joinedCount; i < end; ++i) {
				const std::string& n = ctxs_[i].type_.Name();
				if (auto [it, emplaced] = namesCounters.emplace(n, -1); !emplaced) {
					--it->second;
				}
			}
			for (size_t i = ctxId_, end = ctxId_ + joinedCount; i < end; ++i) {
				const std::string& n = ctxs_[i].type_.Name();
				int& count = namesCounters[n];
				if (count < 0) {
					if (count == -1) {
						nsNamesCache[nsid_].emplace_back(n);
					} else {
						count = 1;
						nsNamesCache[nsid_].emplace_back("1_" + n);
					}
				} else {
					nsNamesCache[nsid_].emplace_back(std::to_string(++count) + '_' + n);
				}
			}
		}
	}

	size_t GetJoinedRowsCount() const noexcept override { return joinedItemIt_.getJoinedFieldsCount(); }
	size_t GetJoinedRowItemsCount(size_t rowId) const override final {
		auto fieldIt = joinedItemIt_.at(rowId);
		return fieldIt.ItemsCount();
	}
	ConstPayload GetJoinedItemPayload(size_t rowid, size_t plIndex) override {
		auto fieldIt = joinedItemIt_.at(rowid);
		const ItemRef& itemRef = fieldIt[plIndex];
		const Context& ctx = ctxs_[ctxId_ + rowid];
		return ConstPayload(ctx.type_, itemRef.Value());
	}
	const TagsMatcher& GetJoinedItemTagsMatcher(size_t rowid) noexcept override {
		const Context& ctx = ctxs_[ctxId_ + rowid];
		return ctx.tagsMatcher_;
	}
	const FieldsSet& GetJoinedItemFieldsFilter(size_t rowid) noexcept override {
		const Context& ctx = ctxs_[ctxId_ + rowid];
		return ctx.fieldsFilter_;
	}
	const std::string& GetJoinedItemNamespace(size_t rowid) noexcept override { return nsNamesCache_[nsid_][rowid]; }

private:
	const joins::ItemIterator& joinedItemIt_;
	const ContextsVector& ctxs_;
	const Iterator::NsNamesCache& nsNamesCache_;
	const int ctxId_;
	const size_t nsid_;
};

void LocalQueryResults::encodeJSON(int idx, WrSerializer& ser, Iterator::NsNamesCache& nsNamesCache) const {
	auto& itemRef = items_[idx];
	assertrx(ctxs.size() > itemRef.Nsid());
	auto& ctx = ctxs[itemRef.Nsid()];

	if (itemRef.Value().IsFree()) {
		ser << "{}";
		return;
	}
	ConstPayload pl(ctx.type_, itemRef.Value());
	JsonEncoder encoder(&ctx.tagsMatcher_, &ctx.fieldsFilter_);
	JsonBuilder builder(ser, ObjType::TypePlain);
	if (!joined_.empty()) {
		joins::ItemIterator itemIt = (begin() + idx).GetJoined();
		if (itemIt.getJoinedItemsCount() > 0) {
			EncoderDatasourceWithJoins joinsDs(itemIt, ctxs, nsNamesCache, GetJoinedNsCtxIndex(itemRef.Nsid()), itemRef.Nsid(),
											   joined_[itemRef.Nsid()].GetJoinedSelectorsCount());
			h_vector<IAdditionalDatasource<JsonBuilder>*, 2> dss;
			AdditionalDatasource ds = needOutputRank ? AdditionalDatasource(itemRef.Proc(), &joinsDs) : AdditionalDatasource(&joinsDs);
			dss.push_back(&ds);
			AdditionalDatasourceShardId dsShardId(outputShardId);
			if (outputShardId >= 0) {
				dss.push_back(&dsShardId);
			}
			encoder.Encode(pl, builder, dss);

			return;
		}
	}

	h_vector<IAdditionalDatasource<JsonBuilder>*, 2> dss;

	AdditionalDatasource ds(itemRef.Proc(), nullptr);
	if (needOutputRank) {
		dss.push_back(&ds);
	}
	AdditionalDatasourceShardId dsShardId(outputShardId);
	if (outputShardId >= 0) {
		dss.push_back(&dsShardId);
	}

	encoder.Encode(pl, builder, dss);
}

joins::ItemIterator LocalQueryResults::Iterator::GetJoined() { return reindexer::joins::ItemIterator::CreateFrom(*this); }

Error LocalQueryResults::Iterator::GetMsgPack(WrSerializer& wrser, bool withHdrLen) {
	auto& itemRef = qr_->items_[idx_];
	assertrx(qr_->ctxs.size() > itemRef.Nsid());
	auto& ctx = qr_->ctxs[itemRef.Nsid()];

	if (itemRef.Value().IsFree()) {
		return Error(errNotFound, "Item not found");
	}

	int startTag = 0;
	ConstPayload pl(ctx.type_, itemRef.Value());
	MsgPackEncoder msgpackEncoder(&ctx.tagsMatcher_);
	const TagsLengths& tagsLengths = msgpackEncoder.GetTagsMeasures(pl);
	MsgPackBuilder msgpackBuilder(wrser, &tagsLengths, &startTag, ObjType::TypePlain, const_cast<TagsMatcher*>(&ctx.tagsMatcher_));
	if (withHdrLen) {
		auto slicePosSaver = wrser.StartSlice();
		msgpackEncoder.Encode(pl, msgpackBuilder);
	} else {
		msgpackEncoder.Encode(pl, msgpackBuilder);
	}
	return errOK;
}

Error LocalQueryResults::Iterator::GetProtobuf(WrSerializer& wrser, bool withHdrLen) {
	auto& itemRef = qr_->items_[idx_];
	assertrx(qr_->ctxs.size() > itemRef.Nsid());
	auto& ctx = qr_->ctxs[itemRef.Nsid()];
	if (!ctx.schema_) {
		return Error(errParams, "The schema was not found for Protobuf builder");
	}

	if (itemRef.Value().IsFree()) {
		return Error(errNotFound, "Item not found");
	}

	ConstPayload pl(ctx.type_, itemRef.Value());
	ProtobufEncoder encoder(&ctx.tagsMatcher_);
	ProtobufBuilder builder(&wrser, ObjType::TypePlain, ctx.schema_.get(), const_cast<TagsMatcher*>(&ctx.tagsMatcher_));

	auto item = builder.Object(kProtoQueryResultsFields.at(kParamItems));
	auto ItemImpl = item.Object(ctx.schema_->GetProtobufNsNumber() + 1);

	if (withHdrLen) {
		auto slicePosSaver = wrser.StartSlice();
		encoder.Encode(pl, builder);
	} else {
		encoder.Encode(pl, builder);
	}

	return {};
}

Error LocalQueryResults::Iterator::GetJSON(WrSerializer& ser, bool withHdrLen) {
	try {
		if (withHdrLen) {
			auto slicePosSaver = ser.StartSlice();
			qr_->encodeJSON(idx_, ser, nsNamesCache);
		} else {
			qr_->encodeJSON(idx_, ser, nsNamesCache);
		}
	} catch (const Error& err) {
		err_ = err;
		return err;
	}
	return errOK;
}

CsvOrdering LocalQueryResults::MakeCSVTagOrdering(unsigned limit, unsigned offset) const {
	if (!ctxs[0].fieldsFilter_.empty()) {
		std::vector<int> ordering;
		ordering.reserve(ctxs[0].fieldsFilter_.size());
		for (const auto& tag : ctxs[0].fieldsFilter_) {
			ordering.emplace_back(tag);
		}
		return ordering;
	}

	std::vector<int> ordering;
	ordering.reserve(128);
	fast_hash_set<int> fieldsTmIds;
	WrSerializer ser;
	const auto& tm = getTagsMatcher(0);
	Iterator::NsNamesCache nsNamesCache;
	for (size_t i = offset; i < items_.size() && i < offset + limit; ++i) {
		ser.Reset();
		encodeJSON(i, ser, nsNamesCache);

		gason::JsonParser parser;
		auto jsonNode = parser.Parse(giftStr(ser.Slice()));

		for (const auto& child : jsonNode) {
			auto [it, inserted] = fieldsTmIds.insert(tm.name2tag(child.key));
			if (inserted && *it > 0) {
				ordering.emplace_back(*it);
			}
		}
	}
	return ordering;
}

[[nodiscard]] Error LocalQueryResults::Iterator::GetCSV(WrSerializer& ser, CsvOrdering& ordering) noexcept {
	try {
		auto& itemRef = qr_->items_[idx_];
		assertrx(qr_->ctxs.size() > itemRef.Nsid());
		auto& ctx = qr_->ctxs[itemRef.Nsid()];

		if (itemRef.Value().IsFree()) {
			return Error(errNotFound, "Item not found");
		}

		ConstPayload pl(ctx.type_, itemRef.Value());
		CsvBuilder builder(ser, ordering);
		CsvEncoder encoder(&ctx.tagsMatcher_, &ctx.fieldsFilter_);

		if (!qr_->joined_.empty()) {
			joins::ItemIterator itemIt = (qr_->begin() + idx_).GetJoined();
			if (itemIt.getJoinedItemsCount() > 0) {
				EncoderDatasourceWithJoins joinsDs(itemIt, qr_->ctxs, nsNamesCache, qr_->GetJoinedNsCtxIndex(itemRef.Nsid()),
												   itemRef.Nsid(), qr_->joined_[itemRef.Nsid()].GetJoinedSelectorsCount());
				h_vector<IAdditionalDatasource<CsvBuilder>*, 2> dss;
				AdditionalDatasourceCSV ds(&joinsDs);
				dss.push_back(&ds);
				encoder.Encode(pl, builder, dss);
				return errOK;
			}
		}

		encoder.Encode(pl, builder);
	}
	CATCH_AND_RETURN
	return errOK;
}

Error LocalQueryResults::Iterator::GetCJSON(WrSerializer& ser, bool withHdrLen) {
	try {
		auto& itemRef = qr_->items_[idx_];
		assertrx(qr_->ctxs.size() > itemRef.Nsid());
		auto& ctx = qr_->ctxs[itemRef.Nsid()];

		if (itemRef.Value().IsFree()) {
			return Error(errNotFound, "Item not found");
		}

		ConstPayload pl(ctx.type_, itemRef.Value());
		CJsonBuilder builder(ser, ObjType::TypePlain);
		CJsonEncoder cjsonEncoder(&ctx.tagsMatcher_, &ctx.fieldsFilter_);

		if (withHdrLen) {
			auto slicePosSaver = ser.StartSlice();
			cjsonEncoder.Encode(pl, builder);
		} else {
			cjsonEncoder.Encode(pl, builder);
		}
	} catch (const Error& err) {
		err_ = err;
		return err;
	}
	return errOK;
}

Item LocalQueryResults::Iterator::GetItem(bool enableHold) {
	auto& itemRef = qr_->items_[idx_];

	assertrx(qr_->ctxs.size() > itemRef.Nsid());
	auto& ctx = qr_->ctxs[itemRef.Nsid()];

	if (itemRef.Value().IsFree()) {
		return Item(Error(errNotFound, "Item not found"));
	}

	auto item = Item(new ItemImpl(ctx.type_, itemRef.Value(), ctx.tagsMatcher_, ctx.schema_));
	item.impl_->payloadValue_.Clone();
	if (enableHold) {
		if (!item.impl_->keyStringsHolder_) {
			item.impl_->keyStringsHolder_.reset(new std::vector<key_string>);
		}
		Payload{ctx.type_, item.impl_->payloadValue_}.CopyStrings(*(item.impl_->keyStringsHolder_));
	}

	item.setID(itemRef.Id());
	return item;
}

void LocalQueryResults::AddItem(Item& item, bool withData, bool enableHold) {
	auto ritem = item.impl_;
	if (item.GetID() != -1) {
		auto ns = ritem->GetNamespace();
		if (ctxs.empty()) {
			ctxs.emplace_back(ritem->Type(), ritem->tagsMatcher(), FieldsSet(), ritem->GetSchema(),
							  ns ? ns->ns_->incarnationTag_ : lsn_t());
		}
		Add(ItemRef(item.GetID(), withData ? (ritem->RealValue().IsFree() ? ritem->Value() : ritem->RealValue()) : PayloadValue()));
		if (withData && enableHold) {
			if (ns) {
				Payload{ns->ns_->payloadType_, items_.back().Value()}.CopyStrings(stringsHolder_);
			} else {
				assertrx(ctxs.size() == 1);
				Payload{ctxs.back().type_, items_.back().Value()}.CopyStrings(stringsHolder_);
			}
		}
	}
}

const TagsMatcher& LocalQueryResults::getTagsMatcher(int nsid) const noexcept { return ctxs[nsid].tagsMatcher_; }

const PayloadType& LocalQueryResults::getPayloadType(int nsid) const noexcept { return ctxs[nsid].type_; }

const FieldsSet& LocalQueryResults::getFieldsFilter(int nsid) const noexcept { return ctxs[nsid].fieldsFilter_; }

TagsMatcher& LocalQueryResults::getTagsMatcher(int nsid) noexcept { return ctxs[nsid].tagsMatcher_; }

PayloadType& LocalQueryResults::getPayloadType(int nsid) noexcept { return ctxs[nsid].type_; }

std::shared_ptr<const Schema> LocalQueryResults::getSchema(int nsid) const noexcept { return ctxs[nsid].schema_; }

int LocalQueryResults::getNsNumber(int nsid) const noexcept {
	assertrx(ctxs[nsid].schema_);
	return ctxs[nsid].schema_->GetProtobufNsNumber();
}

void LocalQueryResults::addNSContext(const PayloadType& type, const TagsMatcher& tagsMatcher, const FieldsSet& filter,
									 std::shared_ptr<const Schema> schema, lsn_t nsIncarnationTag) {
	nonCacheableData = nonCacheableData || filter.getTagsPathsLength();

	ctxs.emplace_back(type, tagsMatcher, filter, std::move(schema), std::move(nsIncarnationTag));
}

void LocalQueryResults::addNSContext(const QueryResults& baseQr, size_t nsid, lsn_t nsIncarnationTag) {
	addNSContext(baseQr.GetPayloadType(nsid), baseQr.GetTagsMatcher(nsid), baseQr.GetFieldsFilter(nsid), baseQr.GetSchema(nsid),
				 std::move(nsIncarnationTag));
}

LocalQueryResults::NsDataHolder::NsDataHolder(LocalQueryResults::NamespaceImplPtr&& _ns, StringsHolderPtr&& strHldr) noexcept
	: nsPtr_{std::move(_ns)}, ns(nsPtr_.get()), strHolder{std::move(strHldr)} {}

LocalQueryResults::NsDataHolder::NsDataHolder(NamespaceImpl* _ns, StringsHolderPtr&& strHldr) noexcept
	: ns(_ns), strHolder(std::move(strHldr)) {}

}  // namespace reindexer