#!/bin/sh
# Usage: oracle/run.sh <scenario-name> [seconds]
# Runs the original game with hooks; writes analysis/oracle/<scenario>.tsv.
# With RR_ORACLE_FRAMES set, the DLL exits the game after that many frames and
# the script waits for it; otherwise the game is killed after <seconds>.
set -eu
ROOT=/Users/archit/Dev/rr
NAME="$1"; SECS="${2:-60}"
case "$NAME" in ''|*[!A-Za-z0-9_-]*) echo 'Scenario names must use letters, digits, hyphens or underscores' >&2; exit 2;; esac
WINE="$HOME/Applications/Wine Staging.app/Contents/Resources/wine/bin/wine"
export WINEPREFIX="$ROOT/prefix" WINEDEBUG="${WINEDEBUG:--all}" WINEDLLOVERRIDES="ddraw=n,b;aweman32=n"
export RR_ORACLE_OUT="C:\\oracle-$NAME.tsv" RR_ORACLE_SCENARIO="$NAME"
export RR_ORACLE_FRAMES="${RR_ORACLE_FRAMES:-}" RR_ORACLE_SEED="${RR_ORACLE_SEED:-}"
export RR_ORACLE_INPUT="${RR_ORACLE_INPUT:-}" RR_ORACLE_RECORD="${RR_ORACLE_RECORD:-}"
export RR_ORACLE_RASTER="${RR_ORACLE_RASTER:-}"
export RR_ORACLE_PIXELS="${RR_ORACLE_PIXELS:-}" RR_ORACLE_SHOT_EVERY="${RR_ORACLE_SHOT_EVERY:-}"
[ -d "$ROOT/cdrom/ROADRASH" ] || hdiutil attach -readonly -nobrowse -mountpoint "$ROOT/cdrom" "$ROOT/Road Rash (USA) (En,Fr,De,Es,It) (Rerelease) (19970930).iso" >/dev/null
ln -sfn "$ROOT/cdrom" "$WINEPREFIX/dosdevices/d:"
rm -f "$WINEPREFIX/drive_c/oracle-$NAME.tsv"
for old in "$WINEPREFIX/drive_c/$NAME"-f*.raw "$WINEPREFIX/drive_c/$NAME"-raster*.raw "$WINEPREFIX/drive_c/$NAME"-texture*.raw "$WINEPREFIX/drive_c/$NAME"-cel*.raw "$WINEPREFIX/drive_c/$NAME"-nodes*.raw; do
    [ -f "$old" ] || continue
    rm -f "$old"
done
cd "$WINEPREFIX/drive_c/ElectronicArts/RoadRashOracle"
"$WINE" 'C:\ElectronicArts\RoadRashOracle\ROADRASH.EXE' &
PID=$!
if [ -n "$RR_ORACLE_FRAMES" ]; then
    wait "$PID" || true
else
    sleep "$SECS"
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
fi
mkdir -p "$ROOT/native-port/analysis/oracle"
cp "$WINEPREFIX/drive_c/oracle-$NAME.tsv" "$ROOT/native-port/analysis/oracle/$NAME.tsv"
for frame in "$WINEPREFIX/drive_c/$NAME"-f*.raw "$WINEPREFIX/drive_c/$NAME"-raster*.raw "$WINEPREFIX/drive_c/$NAME"-texture*.raw "$WINEPREFIX/drive_c/$NAME"-cel*.raw "$WINEPREFIX/drive_c/$NAME"-nodes*.raw; do
    [ -f "$frame" ] || continue
    cp "$frame" "$ROOT/native-port/analysis/oracle/"
done
echo "wrote analysis/oracle/$NAME.tsv"
