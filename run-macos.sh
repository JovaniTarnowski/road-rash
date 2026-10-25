#!/bin/sh
# Build and launch the playable game (plan 11a Task 4: one game, every path).
#   ./run-macos.sh [--debug] [ASSETS_DIR] [rr-game args...]
# Default is an optimized Release build in build-play/; --debug builds the
# unoptimized Debug tree in build/ (slow: the CPU renderer runs ~7x slower).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TYPE=Release
BUILD="$ROOT/build-play"
if [ "${1:-}" = "--debug" ]; then
    TYPE=Debug
    BUILD="$ROOT/build"
    shift
fi
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE="$TYPE"
cmake --build "$BUILD" --parallel --target rr-game
ASSETS="$ROOT/../cdrom/ROADRASH"
if [ $# -gt 0 ] && [ "${1#-}" = "$1" ]; then ASSETS=$1; shift; fi
if [ -f "$ASSETS/ROADRASH.EXE" ]; then
    exec "$BUILD/rr-game" --assets "$ASSETS" "$@"
else
    # No imported/mounted disc: run the first-run import flow.
    exec "$BUILD/rr-game" "$@"
fi
