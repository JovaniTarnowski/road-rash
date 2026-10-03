#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$root" -B "$root/build-release" -DCMAKE_BUILD_TYPE=Release -DRR_BUNDLE_SDL=ON
cmake --build "$root/build-release" --parallel
ctest --test-dir "$root/build-release" --output-on-failure
python3 "$root/tools/verify_macos_app.py" "$root/build-release/Road Rash Native.app"
ditto -c -k --sequesterRsrc --keepParent "$root/build-release/Road Rash Native.app" "$root/build-release/Road Rash Native.zip"
printf '%s\n' "$root/build-release/Road Rash Native.zip"
