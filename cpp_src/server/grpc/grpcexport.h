#pragma once

#include <chrono>
#include <string>
#include "core/cbinding/reindexer_c_export.h"
#include "net/ev/ev.h"

namespace reindexer_server {
class DBManager;

}

extern "C" {
REINDEXER_C_EXPORT void* start_reindexer_grpc(reindexer_server::DBManager& dbMgr, std::chrono::seconds txIdleTimeout,
											  reindexer::net::ev::dynamic_loop& loop, const std::string& address);
REINDEXER_C_EXPORT void stop_reindexer_grpc(void*);
}
typedef void* (*p_start_reindexer_grpc)(reindexer_server::DBManager& dbMgr, std::chrono::seconds txIdleTimeout,
										reindexer::net::ev::dynamic_loop& loop, const std::string& address);

typedef void (*p_stop_reindexer_grpc)(void*);
