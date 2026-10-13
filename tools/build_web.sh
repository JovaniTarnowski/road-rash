#!/bin/sh
# Build the web deploy bundle into build-web/dist/ (plan 23, Task 9 Steps 1-2,
# early scaffold).
#
# Current state: the playable wasm game target does not exist yet. Plan 23
# Task 1 (toolchain spike) is done; Tasks 2-8 (goldens under Node, SDL
# main-callback rework, import job, asset store, page, perf, smoke tests)
# are open, and CMakeLists.txt skips the game executables for EMSCRIPTEN
# until Task 3. So this script produces a status placeholder plus licences
# and runs the no-game-data bundle check. Once web/index.html lands
# (Task 6), extend this script to build the Emscripten game target into
# dist/ instead of the placeholder.
#
# Usage:
#   tools/build_web.sh              # placeholder bundle (always works)
#   RR_WEB_WASM=1 tools/build_web.sh  # reserved: full wasm build (not yet)
#
# Vercel uses this as buildCommand with outputDirectory build-web/dist
# (see vercel.json). COOP/COEP isolation headers are intentionally NOT set
# here: the baseline single-threaded build works on any static host without
# them (plan 23). Task 7 adds them when the optional threaded build lands.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
dist="$root/build-web/dist"
mkdir -p "$dist"

if [ "${RR_WEB_WASM:-0}" = 1 ]; then
    echo "RR_WEB_WASM=1 is reserved for the Task 3+ game target; it does not exist yet." >&2
    echo "Building the placeholder bundle instead." >&2
fi

cat > "$dist/index.html" <<'HTML'
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Road Rash native port &mdash; web build status</title>
<style>
  body { font-family: system-ui, sans-serif; max-width: 40rem; margin: 3rem auto; padding: 0 1rem; }
</style>
<h1>Road Rash native port &mdash; web build</h1>
<p><strong>Status: not yet playable.</strong> The WebAssembly port is at plan 23
Task 1 (Emscripten toolchain spike: library and tests build, measurements
recorded in <code>docs/WEB.md</code>). The playable in-browser game needs
Tasks 3&ndash;6 first: SDL main-callback rework, pull-based disc import,
browser storage, and the game page.</p>
<p>Bring-your-own-disc rule already applies: this page will never host or
upload game data. Import will happen locally in your browser.</p>
<hr>
<p><small>Unofficial research port. Not affiliated with Electronic Arts.
&ldquo;Road Rash&rdquo; is a third-party trademark.</small></p>
HTML

# Licences (mirrors the macOS bundle rule: third-party notices ship).
{
    echo "THIRD-PARTY LICENCES (web bundle)"
    echo "=================================="
    for f in "$root/third_party/SDL-LICENSE.txt" "$root/third_party/LICENSE"; do
        if [ -f "$f" ]; then
            echo; echo "--- $f ---"; echo; cat "$f"
        fi
    done
} > "$dist/LICENSES.txt"

# Bundle check (plan 23, Task 9 Step 2): the deployed bundle must never
# contain game data. Fail on game extensions or on any file whose SHA-256
# appears in the import reference table.
fail=0
if find "$dist" -type f \( -iname '*.avi' -o -iname '*.rra' -o -iname '*.rsc' \
    -o -iname '*.crs' -o -iname '*.car' -o -iname '*.iso' \) -print -quit \
    | grep -q .; then
    echo "bundle check FAILED: game-data file extension in $dist" >&2
    fail=1
fi
hashes=$(grep -o -E '[0-9a-f]{64}' "$root/src/reference_hashes.inc" | sort -u || true)
if [ -n "$hashes" ]; then
    while IFS= read -r f; do
        h=$(shasum -a 256 "$f" | awk '{print $1}')
        if printf '%s\n' "$hashes" | grep -q -x "$h"; then
            echo "bundle check FAILED: $f matches a reference game hash" >&2
            fail=1
        fi
    done <<EOF
$(find "$dist" -type f)
EOF
fi
if [ "$fail" -ne 0 ]; then exit 1; fi

echo "web bundle OK in $dist:"
ls -la "$dist"
echo "bundle check passed: no game data."
