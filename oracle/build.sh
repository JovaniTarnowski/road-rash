#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root/oracle"
mkdir -p build
exec python3 "$root/tools/agent_resource.py" --session oracle-build --resource "$root/oracle/build" -- \
    i686-w64-mingw32-gcc -O2 -Wall -shared -static-libgcc -o build/AWEMAN32.DLL aweman_proxy.c aweman_proxy.def -Wl,--enable-stdcall-fixup
