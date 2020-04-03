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

#include <stdint.h>
#include "core/type_consts.h"
#include "reindexer_ctypes.h"

EXPORT uintptr_t init_reindexer(void);
EXPORT uintptr_t init_reindexer_with_config(reindexer_config config);

EXPORT void destroy_reindexer(uintptr_t rx);

EXPORT reindexer_error reindexer_connect(uintptr_t rx, reindexer_string dsn, ConnectOpts opts, reindexer_string client_vers);
EXPORT reindexer_error reindexer_ping(uintptr_t rx);

EXPORT reindexer_error reindexer_enable_storage(uintptr_t rx, reindexer_string path, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_init_system_namespaces(uintptr_t rx);

EXPORT reindexer_error reindexer_open_namespace(uintptr_t rx, reindexer_string nsName, StorageOpts opts, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_drop_namespace(uintptr_t rx, reindexer_string nsName, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_truncate_namespace(uintptr_t rx, reindexer_string nsName, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_rename_namespace(uintptr_t rx, reindexer_string srcNsName, reindexer_string dstNsName,
										   reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_close_namespace(uintptr_t rx, reindexer_string nsName, reindexer_ctx_info ctx_info);

EXPORT reindexer_error reindexer_add_index(uintptr_t rx, reindexer_string nsName, reindexer_string indexDefJson, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_update_index(uintptr_t rx, reindexer_string nsName, reindexer_string indexDefJson, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_drop_index(uintptr_t rx, reindexer_string nsName, reindexer_string index, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_set_schema(uintptr_t rx, reindexer_string nsName, reindexer_string schemaJson, reindexer_ctx_info ctx_info);

EXPORT reindexer_tx_ret reindexer_start_transaction(uintptr_t rx, reindexer_string nsName);

EXPORT reindexer_error reindexer_modify_item_packed_tx(uintptr_t rx, uintptr_t tr, reindexer_buffer args, reindexer_buffer data);
EXPORT reindexer_error reindexer_update_query_tx(uintptr_t rx, uintptr_t tr, reindexer_buffer in);
EXPORT reindexer_error reindexer_delete_query_tx(uintptr_t rx, uintptr_t tr, reindexer_buffer in);
EXPORT reindexer_ret reindexer_commit_transaction(uintptr_t rx, uintptr_t tr, reindexer_ctx_info ctx_info);

EXPORT reindexer_error reindexer_rollback_transaction(uintptr_t rx, uintptr_t tr);

EXPORT reindexer_ret reindexer_modify_item_packed(uintptr_t rx, reindexer_buffer args, reindexer_buffer data, reindexer_ctx_info ctx_info);
EXPORT reindexer_ret reindexer_select(uintptr_t rx, reindexer_string query, int as_json, int32_t* pt_versions, int pt_versions_count,
							   reindexer_ctx_info ctx_info);

EXPORT reindexer_ret reindexer_select_query(uintptr_t rx, reindexer_buffer in, int as_json, int32_t* pt_versions, int pt_versions_count,
									 reindexer_ctx_info ctx_info);
EXPORT reindexer_ret reindexer_delete_query(uintptr_t rx, reindexer_buffer in, reindexer_ctx_info ctx_info);
EXPORT reindexer_ret reindexer_update_query(uintptr_t rx, reindexer_buffer in, reindexer_ctx_info ctx_info);

EXPORT reindexer_buffer reindexer_cptr2cjson(uintptr_t results_ptr, uintptr_t cptr, int ns_id);
EXPORT void reindexer_free_cjson(reindexer_buffer b);

EXPORT reindexer_error reindexer_free_buffer(reindexer_resbuffer in);
EXPORT reindexer_error reindexer_free_buffers(reindexer_resbuffer* in, int count);

EXPORT reindexer_error reindexer_commit(uintptr_t rx, reindexer_string nsName);

EXPORT reindexer_ret reindexer_enum_meta(uintptr_t rx, reindexer_string ns, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_put_meta(uintptr_t rx, reindexer_string ns, reindexer_string key, reindexer_string data,
								   reindexer_ctx_info ctx_info);
EXPORT reindexer_ret reindexer_get_meta(uintptr_t rx, reindexer_string ns, reindexer_string key, reindexer_ctx_info ctx_info);
EXPORT reindexer_error reindexer_delete_meta(uintptr_t rx, reindexer_string ns, reindexer_string key, reindexer_ctx_info ctx_info);

EXPORT reindexer_error reindexer_cancel_context(reindexer_ctx_info ctx_info, ctx_cancel_type how);

EXPORT void reindexer_enable_logger(void (*logWriter)(int level, char* msg));
EXPORT void reindexer_disable_logger(void);

EXPORT void reindexer_init_locale(void);

#ifdef __cplusplus
}
#endif
