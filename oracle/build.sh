#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
i686-w64-mingw32-gcc -O2 -Wall -shared -static-libgcc -o build/AWEMAN32.DLL aweman_proxy.c aweman_proxy.def -Wl,--enable-stdcall-fixup
echo "built $(pwd)/build/AWEMAN32.DLL"
