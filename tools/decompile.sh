#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EXE=${1:-"$ROOT/../cdrom/ROADRASH/ROADRASH.EXE"}
GHIDRA_HOME=${GHIDRA_HOME:-/opt/homebrew/opt/ghidra/libexec}
export JAVA_HOME=${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}
mkdir -p "$ROOT/analysis/ghidra-project" "$ROOT/analysis/decompiled"
exec "$GHIDRA_HOME/support/analyzeHeadless" "$ROOT/analysis/ghidra-project" RoadRash \
  -import "$EXE" -overwrite -analysisTimeoutPerFile 600 \
  -scriptPath "$ROOT/tools/ghidra" -postScript ExportRoadRash.java "$ROOT/analysis/decompiled" \
  -log "$ROOT/analysis/ghidra.log"
