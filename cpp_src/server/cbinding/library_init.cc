#include <mutex>
#include "debug/backtrace.h"
#include "reindexer_version.h"
#include "tools/alloc_ext/je_malloc_extension.h"
#include "tools/alloc_ext/tc_malloc_extension.h"
#ifdef __GNUC__

#ifdef REINDEX_WITH_JEMALLOC
 #include "jemalloc/jemalloc.h"
#endif // REINDEX_WITH_JEMALLOC

#ifdef REINDEX_WITH_GPERFTOOLS
#include "gperftools/tcmalloc.h"
#endif

#include "leveldb/db.h"
#if REINDEX_WITH_ROCKSDB
#include "rocksdb/version.h"
#endif

__attribute__((constructor)) void init_library(void) {
	reindexer::debug::backtrace_init();
	printf("Reindexer: %s [%s %s]\n", REINDEX_VERSION, __DATE__, __TIME__);

#if REINDEX_WITH_JEMALLOC
	if (!reindexer::alloc_ext::JEMallocIsAvailable()) {
		throw std::runtime_error("Jemalloc couldn't be loaded properly. Check build/linking.");
	}

	const char *jemalloc_version;
	size_t s = sizeof(jemalloc_version);
	mallctl("version", &jemalloc_version, &s, NULL, 0);
	printf("Jemalloc: %s\n", jemalloc_version);
#endif

#if REINDEX_WITH_GPERFTOOLS
	if (!reindexer::alloc_ext::TCMallocIsAvailable()) {
		throw std::runtime_error("Tcmalloc couldn't be loaded properly. Check build/linking.");
	}

	int major, minor;
	const char *patch;
	const char *tcmalloc_version = tc_version(&major, &minor, &patch);
	printf("Tcmalloc: %s\n", tcmalloc_version);
#endif

	printf("LevelDb: %d.%d\n", leveldb::kMajorVersion, leveldb::kMinorVersion);

#if REINDEX_WITH_ROCKSDB
	printf("RocksDb: %d.%d.%d\n", ROCKSDB_MAJOR, ROCKSDB_MINOR, ROCKSDB_PATCH);
#endif
}
#endif