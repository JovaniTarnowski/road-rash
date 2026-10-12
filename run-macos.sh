#!/bin/sh
# Build and launch the playable game (plan 11a Task 4: one game, every path).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$ROOT/build" --parallel
ASSETS="${1:-$ROOT/../cdrom/ROADRASH}"
if [ -f "$ASSETS/ROADRASH.EXE" ]; then
    exec "$ROOT/build/rr-game" --assets "$ASSETS"
else
    # No imported/mounted disc: run the first-run import flow.
    exec "$ROOT/build/rr-game"
fi
