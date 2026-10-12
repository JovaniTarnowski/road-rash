#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$root" -B "$root/build-release" -DCMAKE_BUILD_TYPE=Release -DRR_BUNDLE_SDL=ON
cmake --build "$root/build-release" --parallel
ctest --test-dir "$root/build-release" --output-on-failure
# Stamp the commit the bundle was built from (plan 11a Task 4), so the verifier
# can fail a stale app. The plist is part of the sealed resources, so re-sign.
app="$root/build-release/Road Rash Native.app"
commit=$(git -C "$root" rev-parse --short HEAD)
plutil -replace RRBuildCommit -string "$commit" "$app/Contents/Info.plist"
codesign --force --sign - "$app"
python3 "$root/tools/verify_macos_app.py" "$app"
ditto -c -k --sequesterRsrc --keepParent "$root/build-release/Road Rash Native.app" "$root/build-release/Road Rash Native.zip"
printf '%s\n' "$root/build-release/Road Rash Native.zip"
