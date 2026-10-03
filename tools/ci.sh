#!/bin/sh
# Native + Android checks; codec and standalone packaging checks are opt-in.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
if [ -d ../cdrom/ROADRASH ]; then
    build/rr-native --assets ../cdrom/ROADRASH --smoke
    build/rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE ../cdrom/ROADRASH/DATA/COURSES/*.CRS
fi
if [ "${RR_CODEC_ORACLE:-0}" = 1 ]; then
    python3 tools/verify_video_audio.py ../cdrom/ROADRASH/VIDEO --video-decoder build/cinepak-test
fi
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}"
android/gradlew -p android assembleDebug
if unzip -Z1 android/app/build/outputs/apk/debug/app-debug.apk | rg -q '^assets/game/'; then
    printf '%s\n' 'Original game data found in APK' >&2
    exit 1
fi
if [ "${RR_STANDALONE_APP:-0}" = 1 ]; then
    tools/build_macos_app.sh
fi
printf '%s\n' 'CI OK'
