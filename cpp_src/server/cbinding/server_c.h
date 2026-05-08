
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core/cbinding/reindexer_c_export.h"
#include "core/cbinding/reindexer_ctypes.h"

REINDEXER_C_EXPORT uintptr_t init_reindexer_server(void);
REINDEXER_C_EXPORT void destroy_reindexer_server(uintptr_t psvc);
REINDEXER_C_EXPORT reindexer_error start_reindexer_server(uintptr_t psvc, reindexer_string config);
REINDEXER_C_EXPORT reindexer_error stop_reindexer_server(uintptr_t psvc);
REINDEXER_C_EXPORT reindexer_error get_reindexer_instance(uintptr_t psvc, reindexer_string dbname, reindexer_string user,
														  reindexer_string pass, uintptr_t* rx);
REINDEXER_C_EXPORT int check_server_ready(uintptr_t psvc);
REINDEXER_C_EXPORT reindexer_error reopen_log_files(uintptr_t psvc);

#ifdef __cplusplus
}
#endif
