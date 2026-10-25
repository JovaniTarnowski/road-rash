#!/bin/sh
# Logged native + Android debug; strict fidelity/release/packaging opt in.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
build_dir=${RR_BUILD_DIR:-"$root/build-agent-ci"}
python3 tools/cpp_quality.py format-check
python3 tools/agent_gate.py native --build-dir "$build_dir"
python3 tools/agent_gate.py android-debug
if [ "${RR_ANDROID_RELEASE:-0}" = 1 ]; then
    python3 tools/agent_gate.py android-release
fi
if [ "${RR_SIM_ACCEPTANCE:-0}" = 1 ]; then
    python3 tools/agent_gate.py oracle --build-dir "$build_dir"
fi
if [ "${RR_CODEC_ORACLE:-0}" = 1 ]; then
    python3 tools/verify_video_audio.py ../cdrom/ROADRASH/VIDEO --video-decoder "$build_dir/cinepak-test"
fi
if [ "${RR_TSAN:-0}" = 1 ]; then
    sh tools/tsan.sh
fi
if [ "${RR_WEB:-0}" = 1 ]; then
    web_dir=${RR_WEB_DIR:-"$root/build-web"}
    # shellcheck disable=SC1091
    . "$root/.tools/emsdk/emsdk_env.sh" >/dev/null 2>&1 || true
    "$root/.tools/emsdk/upstream/emscripten/emcmake" cmake -S "$root" -B "$web_dir" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$web_dir" --parallel
    ctest --test-dir "$web_dir" --output-on-failure
fi
if [ "${RR_STANDALONE_APP:-0}" = 1 ]; then
    python3 tools/agent_gate.py package --build-dir "$root/build-agent-ci-package"
elif [ -d "build-release/Road Rash Native.app" ]; then
    python3 tools/verify_macos_app.py "build-release/Road Rash Native.app"
fi
printf '%s\n' 'CI OK (see receipts; simulation parity and hardware are separate coverage)'
