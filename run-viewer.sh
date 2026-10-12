#!/bin/sh
# Build and launch the asset research viewer (developer tool; the game is run-macos.sh).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$ROOT/build" --parallel
exec "$ROOT/build/rr-viewer" --assets "${1:-$ROOT/../cdrom/ROADRASH}"
