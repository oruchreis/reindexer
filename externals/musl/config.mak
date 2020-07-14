TARGET = x86_64-linux-musl
STAT = -static --static
FLAG = -fPIC -g0 -Os

COMMON_CONFIG += CC="gcc ${STAT}" CXX="g++ ${STAT}"
COMMON_CONFIG += CFLAGS="${FLAG}" CXXFLAGS="${FLAG}" LDFLAGS="-s ${STAT}"
COMMON_CONFIG +=  --disable-shared --enable-static=yes --enable-static --enable-default-pie --disable-host-shared --disable-nls

BINUTILS_CONFIG += --disable-lto
GCC_CONFIG += --enable-default-pie --enable-shared=yes --enable-static --disable-host-shared
GCC_CONFIG += --disable-lto --disable-multilib --enable-languages=c,c++
DL_CMD = wget -q -O