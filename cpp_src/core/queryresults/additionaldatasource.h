#pragma once

#include "core/cjson/baseencoder.h"

namespace reindexer {

class AdditionalDatasource final : public IAdditionalDatasource<JsonBuilder> {
public:
	AdditionalDatasource(double r, IEncoderDatasourceWithJoins* jds) noexcept : joinsDs_(jds), withRank_(true), rank_(r) {}
	AdditionalDatasource(IEncoderDatasourceWithJoins* jds) noexcept : joinsDs_(jds) {}
	void PutAdditionalFields(JsonBuilder& builder) const override {
		if (withRank_) {
			builder.Put("rank()", rank_);
		}
	}
	IEncoderDatasourceWithJoins* GetJoinsDatasource() noexcept override { return joinsDs_; }

private:
	IEncoderDatasourceWithJoins* joinsDs_ = nullptr;
	bool withRank_ = false;
	double rank_ = 0.0;
};

class AdditionalDatasourceShardId final : public IAdditionalDatasource<JsonBuilder> {
public:
	AdditionalDatasourceShardId(int shardId) noexcept : shardId_(shardId) {}
	void PutAdditionalFields(JsonBuilder& builder) const override { builder.Put("#shard_id", shardId_); }
	IEncoderDatasourceWithJoins* GetJoinsDatasource() noexcept override { return nullptr; }

private:
	int shardId_;
};

class AdditionalDatasourceCSV final : public IAdditionalDatasource<CsvBuilder> {
public:
	AdditionalDatasourceCSV(IEncoderDatasourceWithJoins* jds) noexcept : joinsDs_(jds) {}
	void PutAdditionalFields(CsvBuilder&) const override {}
	IEncoderDatasourceWithJoins* GetJoinsDatasource() noexcept override { return joinsDs_; }

private:
	IEncoderDatasourceWithJoins* joinsDs_;
};

}  // namespace reindexer