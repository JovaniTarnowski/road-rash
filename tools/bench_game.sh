#!/bin/sh
# Race frame-time benchmark (docs/PERFORMANCE.md).
#
#   tools/bench_game.sh [options] [preset ...]
#
# Presets: user (your settings.txt), original (all enhancements off, 4:3),
# enhanced (every enhancement on, widescreen match), or a path to a
# settings.txt. Each preset runs `rr-game --bench` in an isolated preference
# directory (copy of the chosen settings) so your real settings and saves are
# never written. Reports land in analysis/bench/<stamp>/<preset>.json and a
# summary table is printed.
#
# Options:
#   --build DIR        build directory (default build-bench-release, Release)
#   --debug            use a Debug build (default build-bench-debug)
#   --no-build         do not configure/build first
#   --frames N         measured frames (default 600)
#   --warmup N         frames skipped first (default 90)
#   --course N         quick-race course 0..4 (default 3)
#   --render-scale N   force race scale 1..4 (default: from settings)
#   --vsync            keep vsync on (default off: measures throughput)
#   --assets DIR       disc root (default ../cdrom/ROADRASH)
#   --out DIR          report directory
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=""
TYPE=Release
DO_BUILD=1
FRAMES=600
WARMUP=90
COURSE=3
SCALE=""
VSYNC=""
ASSETS="$ROOT/../cdrom/ROADRASH"
OUT=""
PRESETS=""
while [ $# -gt 0 ]; do
    case "$1" in
        --build) BUILD=$2; shift 2 ;;
        --debug) TYPE=Debug; shift ;;
        --no-build) DO_BUILD=0; shift ;;
        --frames) FRAMES=$2; shift 2 ;;
        --warmup) WARMUP=$2; shift 2 ;;
        --course) COURSE=$2; shift 2 ;;
        --render-scale) SCALE=$2; shift 2 ;;
        --vsync) VSYNC=--bench-vsync; shift ;;
        --assets) ASSETS=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
        *) PRESETS="$PRESETS $1"; shift ;;
    esac
done
[ -n "$PRESETS" ] || PRESETS="user"
if [ -z "$BUILD" ]; then
    if [ "$TYPE" = Debug ]; then BUILD="$ROOT/build-bench-debug"; else BUILD="$ROOT/build-bench-release"; fi
fi
[ -f "$ASSETS/ROADRASH.EXE" ] || { echo "bench: no disc at $ASSETS (use --assets)" >&2; exit 77; }
STAMP=$(date +%Y%m%d-%H%M%S)
[ -n "$OUT" ] || OUT="$ROOT/analysis/bench/$STAMP"
mkdir -p "$OUT"
if [ "$DO_BUILD" = 1 ]; then
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE="$TYPE" > "$OUT/configure.log" 2>&1 || { cat "$OUT/configure.log"; exit 1; }
    cmake --build "$BUILD" --parallel --target rr-game > "$OUT/build.log" 2>&1 || { tail -40 "$OUT/build.log"; exit 1; }
fi
USER_SETTINGS="$HOME/Library/Application Support/RoadRashNative/RoadRash/settings.txt"
[ -f "$USER_SETTINGS" ] || USER_SETTINGS="${XDG_DATA_HOME:-$HOME/.local/share}/RoadRashNative/RoadRash/settings.txt"

write_preset() {   # $1 preset, $2 target settings.txt
    case "$1" in
        user) if [ -f "$USER_SETTINGS" ]; then cp "$USER_SETTINGS" "$2"; else : > "$2"; fi ;;
        original) printf 'widescreen=0\ntexture_detail=0\nenh_interpolate=0\nenh_perspective=0\nenh_full_colour=0\nenh_filter=0\nenh_fog=0\nenh_grade=0\nenh_draw_slices=33\nenh_bloom=0\nenh_vignette=0\nenh_motion_blur=0\n' > "$2" ;;
        enhanced) printf 'widescreen=3\ntexture_detail=1\nenh_interpolate=1\nenh_perspective=1\nenh_full_colour=1\nenh_filter=2\nenh_fog=40\nenh_grade=3\nenh_draw_slices=64\nenh_bloom=1\nenh_vignette=1\nenh_motion_blur=1\n' > "$2" ;;
        *) [ -f "$1" ] || { echo "bench: unknown preset $1" >&2; return 1; }; cp "$1" "$2" ;;
    esac
}

STATUS=0
for P in $PRESETS; do
    NAME=$(basename "$P" .txt)
    PREF="$OUT/pref-$NAME"
    mkdir -p "$PREF"
    write_preset "$P" "$PREF/settings.txt"
    # Keep the generated upscale cache: texture_detail needs it to cost anything.
    REAL_PREF=$(dirname "$USER_SETTINGS")
    [ -d "$REAL_PREF/upscaled" ] && [ ! -e "$PREF/upscaled" ] && ln -s "$REAL_PREF/upscaled" "$PREF/upscaled"
    set -- --assets "$ASSETS" --course "$COURSE" --bench "$FRAMES" --bench-warmup "$WARMUP" \
        --bench-out "$OUT/$NAME.json" --bench-label "$NAME"
    [ -n "$SCALE" ] && set -- "$@" --render-scale "$SCALE"
    [ -n "$VSYNC" ] && set -- "$@" "$VSYNC"
    echo "bench: $NAME ($TYPE) ..."
    if ! RR_PREF_OVERRIDE="$PREF/" "$BUILD/rr-game" "$@" > "$OUT/$NAME.log" 2>&1; then
        echo "bench: $NAME failed, see $OUT/$NAME.log" >&2; STATUS=1; continue
    fi
    [ -f "$OUT/$NAME.json" ] || { echo "bench: $NAME wrote no report" >&2; STATUS=1; }
done
python3 "$ROOT/tools/bench_report.py" "$OUT"/*.json || STATUS=1
echo "bench: reports in $OUT"
exit $STATUS
