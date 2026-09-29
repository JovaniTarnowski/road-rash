#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$ROOT/build" --parallel
exec "$ROOT/build/rr-native" --assets "${1:-$ROOT/../cdrom/ROADRASH}"
