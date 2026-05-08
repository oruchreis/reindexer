#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "core/type_consts.h"
#include "reindexer_c_export.h"
#include "reindexer_ctypes.h"

REINDEXER_C_EXPORT uintptr_t init_reindexer(void);
REINDEXER_C_EXPORT uintptr_t init_reindexer_with_config(reindexer_config config);

REINDEXER_C_EXPORT void destroy_reindexer(uintptr_t rx);

REINDEXER_C_EXPORT reindexer_error reindexer_connect(uintptr_t rx, reindexer_string dsn, ConnectOpts opts,
													 reindexer_string client_vers, BindingCapabilities caps);
REINDEXER_C_EXPORT reindexer_error reindexer_ping(uintptr_t rx);

REINDEXER_C_EXPORT reindexer_error reindexer_open_namespace(uintptr_t rx, reindexer_string nsName, StorageOpts opts,
															reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_drop_namespace(uintptr_t rx, reindexer_string nsName, reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_truncate_namespace(uintptr_t rx, reindexer_string nsName,
																reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_rename_namespace(uintptr_t rx, reindexer_string srcNsName,
															  reindexer_string dstNsName, reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_close_namespace(uintptr_t rx, reindexer_string nsName, reindexer_ctx_info ctx_info);

REINDEXER_C_EXPORT reindexer_error reindexer_add_index(uintptr_t rx, reindexer_string nsName, reindexer_string indexDefJson,
													   reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_update_index(uintptr_t rx, reindexer_string nsName, reindexer_string indexDefJson,
														  reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_drop_index(uintptr_t rx, reindexer_string nsName, reindexer_string index,
														reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_set_schema(uintptr_t rx, reindexer_string nsName, reindexer_string schemaJson,
														reindexer_ctx_info ctx_info);

REINDEXER_C_EXPORT reindexer_tx_ret reindexer_start_transaction(uintptr_t rx, reindexer_string nsName);

REINDEXER_C_EXPORT reindexer_error reindexer_modify_item_packed_tx(uintptr_t rx, uintptr_t tr, reindexer_buffer args,
																   reindexer_buffer data);
REINDEXER_C_EXPORT reindexer_error reindexer_update_query_tx(uintptr_t rx, uintptr_t tr, reindexer_buffer in);
REINDEXER_C_EXPORT reindexer_error reindexer_delete_query_tx(uintptr_t rx, uintptr_t tr, reindexer_buffer in);
REINDEXER_C_EXPORT reindexer_ret reindexer_commit_transaction(uintptr_t rx, uintptr_t tr, reindexer_ctx_info ctx_info);

REINDEXER_C_EXPORT reindexer_error reindexer_rollback_transaction(uintptr_t rx, uintptr_t tr);

REINDEXER_C_EXPORT reindexer_ret reindexer_modify_item_packed(uintptr_t rx, reindexer_buffer args, reindexer_buffer data,
															  reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_ret reindexer_select(uintptr_t rx, reindexer_string query, int as_json, int32_t* tm_versions,
												  int tm_versions_count, reindexer_ctx_info ctx_info);

REINDEXER_C_EXPORT reindexer_ret reindexer_select_query(uintptr_t rx, reindexer_buffer in, int as_json, int32_t* tm_versions,
														int tm_versions_count, reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_ret reindexer_delete_query(uintptr_t rx, reindexer_buffer in, reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_ret reindexer_update_query(uintptr_t rx, reindexer_buffer in, int32_t* tm_versions,
														int tm_versions_count, reindexer_ctx_info ctx_info);

REINDEXER_C_EXPORT reindexer_buffer reindexer_cptr2cjson(uintptr_t results_ptr, uintptr_t cptr, int ns_id);
REINDEXER_C_EXPORT void reindexer_free_cjson(reindexer_buffer b);

REINDEXER_C_EXPORT reindexer_error reindexer_free_buffer(reindexer_resbuffer in);
REINDEXER_C_EXPORT reindexer_error reindexer_free_buffers(reindexer_resbuffer* in, int count);

REINDEXER_C_EXPORT reindexer_ret reindexer_enum_meta(uintptr_t rx, reindexer_string ns, reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_put_meta(uintptr_t rx, reindexer_string ns, reindexer_string key,
													  reindexer_string data, reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_ret reindexer_get_meta(uintptr_t rx, reindexer_string ns, reindexer_string key,
													reindexer_ctx_info ctx_info);
REINDEXER_C_EXPORT reindexer_error reindexer_delete_meta(uintptr_t rx, reindexer_string ns, reindexer_string key,
														 reindexer_ctx_info ctx_info);

REINDEXER_C_EXPORT reindexer_error reindexer_subscribe(uintptr_t rx, reindexer_string optsJSON);
REINDEXER_C_EXPORT reindexer_error reindexer_unsubscribe(uintptr_t rx);
REINDEXER_C_EXPORT reindexer_array_ret reindexer_read_events(uintptr_t rx, reindexer_buffer* out_buffers, uint32_t buffers_count);
REINDEXER_C_EXPORT reindexer_error reindexer_erase_events(uintptr_t rx, uint32_t events_count);

REINDEXER_C_EXPORT reindexer_error reindexer_cancel_context(reindexer_ctx_info ctx_info, ctx_cancel_type how);

REINDEXER_C_EXPORT void reindexer_enable_logger(void (*logWriter)(int level, char* msg));
REINDEXER_C_EXPORT void reindexer_disable_logger(void);

REINDEXER_C_EXPORT void reindexer_init_locale(void);

REINDEXER_C_EXPORT const char* reindexer_version(void);
REINDEXER_C_EXPORT void reindexer_malloc_free(void* ptr);

#ifdef __cplusplus
}
#endif
