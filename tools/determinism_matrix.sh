#!/bin/sh
# Plan 34 Task 3 Step 5: cross-platform determinism matrix.
# Runs rr-determinism (a fixed 8-human input log; see tools/determinism.cpp)
# on every platform available here and compares the per-60-tick state
# hashes with the committed synthetic reference
# tests/data/determinism_synthetic.txt (and, with the disc, with the macOS
# disc run). Exit 1 on any difference.
#   tools/determinism_matrix.sh [macos] [linux] [wasm] [android]   (default: all available)
# Linux x86-64 builds in Docker (debian:trixie, SDL3 from the distribution);
# wasm32 needs an activated Emscripten (emcc) and runs under Node; Android
# needs a connected device or emulator (adb) and a built rr-determinism for
# arm64-v8a in build-android-det/. Windows joins with plan 33.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${RR_DET_OUT:-$ROOT/analysis/determinism}"
REF="$ROOT/tests/data/determinism_synthetic.txt"
DISC="${RR_DISC:-$ROOT/../cdrom/ROADRASH}"
mkdir -p "$OUT"
WANT="${*:-macos linux wasm android}"
FAIL=0
check() { # check <name> <file> <reference>
    if diff -q "$2" "$3" >/dev/null; then echo "determinism: $1 matches $(basename "$3")"
    else echo "determinism: $1 DIFFERS from $(basename "$3") (first line: $(diff "$2" "$3" | sed -n 2p))" >&2; FAIL=1; fi
}
for p in $WANT; do
    case "$p" in
    macos)
        [ "$(uname -s)" = Darwin ] || { echo "determinism: macos skipped (not on macOS)"; continue; }
        cmake -S "$ROOT" -B "$ROOT/build-det-macos" -DCMAKE_BUILD_TYPE=Release -DRR_NET_TRANSPORT=OFF >"$OUT/macos-build.log" 2>&1
        cmake --build "$ROOT/build-det-macos" --target rr-determinism --parallel >>"$OUT/macos-build.log" 2>&1
        "$ROOT/build-det-macos/rr-determinism" --synthetic >"$OUT/macos-synthetic.txt"
        check macos-synthetic "$OUT/macos-synthetic.txt" "$REF"
        if [ -f "$DISC/ROADRASH.EXE" ]; then
            "$ROOT/build-det-macos/rr-determinism" --disc "$DISC" >"$OUT/macos-disc.txt"
        fi ;;
    linux)
        command -v docker >/dev/null && docker info >/dev/null 2>&1 || { echo "determinism: linux skipped (no Docker daemon)"; continue; }
        docker run --rm --platform linux/amd64 -v "$ROOT:/src:ro" -v rr-det-linux-build:/build -v "$OUT:/out" \
            ${RR_DISC_MOUNT:+-v "$DISC:/disc:ro"} debian:trixie sh -ec '
            apt-get update -qq >/dev/null && apt-get install -y -qq g++ cmake make libsdl3-dev python3 >/dev/null
            cmake -S /src -B /build -DCMAKE_BUILD_TYPE=Release -DRR_NET_TRANSPORT=OFF >/out/linux-build.log 2>&1
            cmake --build /build --target rr-determinism --parallel >>/out/linux-build.log 2>&1
            /build/rr-determinism --synthetic >/out/linux-synthetic.txt
            if [ -f /disc/ROADRASH.EXE ]; then /build/rr-determinism --disc /disc >/out/linux-disc.txt; fi'
        check linux-x86_64-synthetic "$OUT/linux-synthetic.txt" "$REF"
        [ -f "$OUT/linux-disc.txt" ] && [ -f "$OUT/macos-disc.txt" ] && check linux-x86_64-disc "$OUT/linux-disc.txt" "$OUT/macos-disc.txt" ;;
    wasm)
        command -v emcc >/dev/null || { echo "determinism: wasm skipped (no emcc; source emsdk_env.sh)"; continue; }
        emcmake cmake -S "$ROOT" -B "$ROOT/build-det-wasm" -DCMAKE_BUILD_TYPE=Release >"$OUT/wasm-build.log" 2>&1
        cmake --build "$ROOT/build-det-wasm" --target rr-determinism --parallel >>"$OUT/wasm-build.log" 2>&1
        node "$ROOT/build-det-wasm/rr-determinism.js" --synthetic >"$OUT/wasm-synthetic.txt"
        check wasm32-synthetic "$OUT/wasm-synthetic.txt" "$REF" ;;
    android)
        ADB="${ADB:-$ROOT/.tools/android-sdk/platform-tools/adb}"
        [ -x "$ADB" ] && "$ADB" get-state >/dev/null 2>&1 || { echo "determinism: android skipped (no device)"; continue; }
        BIN="$ROOT/build-android-det/rr-determinism"
        [ -x "$BIN" ] || { echo "determinism: android skipped (build $BIN for arm64-v8a first)"; continue; }
        "$ADB" push "$BIN" /data/local/tmp/rr-determinism >/dev/null
        "$ADB" shell /data/local/tmp/rr-determinism --synthetic >"$OUT/android-synthetic.txt"
        check android-arm64-synthetic "$OUT/android-synthetic.txt" "$REF" ;;
    *) echo "determinism: unknown platform $p" >&2; FAIL=1 ;;
    esac
done
exit $FAIL
