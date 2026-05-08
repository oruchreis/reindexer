#pragma once

#if defined(REINDEXER_C_EXPORTS)
#if defined(_WIN32)
#define REINDEXER_C_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define REINDEXER_C_EXPORT __attribute__((visibility("default")))
#else
#define REINDEXER_C_EXPORT
#endif
#else
#define REINDEXER_C_EXPORT
#endif
