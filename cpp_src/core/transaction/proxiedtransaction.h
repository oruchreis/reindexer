#pragma once

#include <condition_variable>
#include "client/transaction.h"
#include "core/cjson/tagsmatcher.h"
#include "core/payload/payloadtype.h"

namespace reindexer {

class Item;
class SharedTransactionData;
class RdxContext;
class ReindexerImpl;
class QueryResults;

class ProxiedTransaction {
public:
	ProxiedTransaction(client::Transaction &&_tx, bool _proxiedViaSharding)
		: tx_(std::move(_tx)), proxiedViaSharding_(_proxiedViaSharding), asyncData_(mtx_) {}

	Error Modify(Item &&item, ItemModifyMode mode, lsn_t lsn);
	Error Modify(Query &&query, lsn_t lsn);
	Error PutMeta(std::string_view key, std::string_view value, lsn_t lsn);
	Error SetTagsMatcher(TagsMatcher &&tm, lsn_t lsn);
	void Rollback(int serverId, const RdxContext &ctx);
	Error Commit(int serverId, int shardId, QueryResults &result, const RdxContext &ctx);

private:
	class AsyncData {
	public:
		AsyncData(std::mutex &mtx) noexcept : mtx_(mtx) {}
		void AddNewAsyncRequest();
		void OnAsyncRequestDone(const Error &e) noexcept;
		Error AwaitAsyncRequests() noexcept;
		~AsyncData() { AwaitAsyncRequests(); }

	private:
		std::mutex &mtx_;
		std::condition_variable cv_;
		Error err_;
		unsigned asyncRequests_ = 0;
	};
	struct ItemCache {
		PayloadType pt;
		TagsMatcher tm;
		bool isValid = false;
	};

	client::Transaction tx_;
	const bool proxiedViaSharding_ = false;
	std::mutex mtx_;
	AsyncData asyncData_;
	ItemCache itemCache_;
};

}  // namespace reindexer