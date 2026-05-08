#include <cstdio>
#include <stdexcept>

#include "leveldb/db.h"
#include "reindexer_version.h"
#include "tools/alloc_ext/je_malloc_extension.h"
#include "tools/alloc_ext/tc_malloc_extension.h"

#ifdef REINDEX_WITH_ROCKSDB
#include "rocksdb/version.h"
#endif

#ifdef REINDEX_WITH_JEMALLOC
#include "jemalloc/jemalloc.h"
#endif

#ifdef REINDEX_WITH_GPERFTOOLS
#include "gperftools/tcmalloc.h"
#endif

#ifdef __GNUC__
__attribute__((constructor)) void init_reindexer_embedded_library(void) {
	std::printf("Reindexer: %s [%s %s]\n", REINDEX_VERSION, __DATE__, __TIME__);

#ifdef REINDEX_WITH_JEMALLOC
	if (!reindexer::alloc_ext::JEMallocIsAvailable()) {
		throw std::runtime_error("Jemalloc could not be loaded properly. Check build/linking.");
	}

	const char* jemallocVersion = nullptr;
	size_t versionSize = sizeof(jemallocVersion);
	reindexer::alloc_ext::mallctl("version", &jemallocVersion, &versionSize, nullptr, 0);
	std::printf("Jemalloc: %s\n", jemallocVersion);
#endif

#ifdef REINDEX_WITH_GPERFTOOLS
	if (!reindexer::alloc_ext::TCMallocIsAvailable()) {
		throw std::runtime_error("Tcmalloc could not be loaded properly. Check build/linking.");
	}

	int major = 0;
	int minor = 0;
	const char* patch = nullptr;
	const char* tcmallocVersion = tc_version(&major, &minor, &patch);
	std::printf("Tcmalloc: %s\n", tcmallocVersion);
#endif

	std::printf("LevelDB: %d.%d\n", leveldb::kMajorVersion, leveldb::kMinorVersion);

#ifdef REINDEX_WITH_ROCKSDB
	std::printf("RocksDB: %d.%d.%d\n", ROCKSDB_MAJOR, ROCKSDB_MINOR, ROCKSDB_PATCH);
#endif
}
#endif
