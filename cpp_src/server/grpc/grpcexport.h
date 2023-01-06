#pragma once

#include <chrono>
#include <string>
#include "net/ev/ev.h"

namespace reindexer_server {
class DBManager;

}

#if defined(_MSC_VER)
    //  Microsoft 
    #define EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
    //  GCC
    #define EXPORT __attribute__((visibility("default")))
#else
    #pragma warning Unknown dynamic link import/export semantics.
#endif

#ifdef __cplusplus
extern "C" {
#endif
EXPORT void* start_reindexer_grpc(reindexer_server::DBManager& dbMgr, std::chrono::seconds txIdleTimeout, reindexer::net::ev::dynamic_loop& loop,
						  const std::string& address);
EXPORT void stop_reindexer_grpc(void*);
#ifdef __cplusplus
}
#endif

typedef void* (*p_start_reindexer_grpc)(reindexer_server::DBManager& dbMgr, std::chrono::seconds txIdleTimeout,
										reindexer::net::ev::dynamic_loop& loop, const std::string& address);

typedef void (*p_stop_reindexer_grpc)(void*);
