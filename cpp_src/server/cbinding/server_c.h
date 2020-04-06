
#pragma once

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

#include "core/cbinding/reindexer_ctypes.h"

EXPORT uintptr_t init_reindexer_server();
EXPORT void destroy_reindexer_server(uintptr_t psvc);
EXPORT reindexer_error start_reindexer_server(uintptr_t psvc, reindexer_string config);
EXPORT reindexer_error stop_reindexer_server(uintptr_t psvc);
EXPORT reindexer_error get_reindexer_instance(uintptr_t psvc, reindexer_string dbname, reindexer_string user, reindexer_string pass,
                                              uintptr_t* rx);
EXPORT int check_server_ready(uintptr_t psvc);
EXPORT reindexer_error reopen_log_files(uintptr_t psvc);
#ifdef __cplusplus
}
#endif
