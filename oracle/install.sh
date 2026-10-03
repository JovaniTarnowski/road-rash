#!/bin/sh
# Create a separate oracle install so the user's playable install is never touched.
set -eu
ROOT=/Users/archit/Dev/rr
SRC="$ROOT/prefix/drive_c/ElectronicArts/RoadRash"
DST="$ROOT/prefix/drive_c/ElectronicArts/RoadRashOracle"
[ -d "$DST" ] || cp -R "$SRC" "$DST"
[ -f "$DST/AWEMAN_REAL.DLL" ] || cp "$SRC/AWEMAN32.DLL" "$DST/AWEMAN_REAL.DLL"
cp "$(dirname "$0")/build/AWEMAN32.DLL" "$DST/AWEMAN32.DLL"
shasum -a 256 "$DST/ROADRASH.EXE" | grep -q 73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2
mkdir -p "$DST/Screenshots"
echo "oracle install ready at $DST"
