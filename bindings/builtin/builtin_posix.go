// +build !windows
//go:generate sh -c "cd ../.. && mkdir -p build && cd build && cmake -DGO_BUILTIN_EXPORT_PKG_PATH=\"../bindings/builtin\" .. && make reindexer -j12"

package builtin

// #cgo CXXFLAGS: -std=c++11 -g -O2 -Wall -Wpedantic -Wextra -I../../cpp_src 
// #cgo CFLAGS: -std=c99 -g -O2 -Wall -Wpedantic -Wno-unused-variable -I../../cpp_src 
// #cgo LDFLAGS: -L${SRCDIR}/../../build/cpp_src/  -lreindexer -lleveldb -lpthread -lrt -ldl -lunwind -lstdc++ -g
import "C"
