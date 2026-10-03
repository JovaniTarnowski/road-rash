#!/bin/sh
# Decompile extra functions by address into analysis/decompiled (uses the existing Ghidra project).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
GHIDRA_HOME=${GHIDRA_HOME:-/opt/homebrew/opt/ghidra/libexec}
export JAVA_HOME=${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}
exec "$GHIDRA_HOME/support/analyzeHeadless" "$ROOT/analysis/ghidra-project" RoadRash \
  -process ROADRASH.EXE -noanalysis -scriptPath "$ROOT/tools/ghidra" \
  -postScript DecompileAt.java "$ROOT/analysis/decompiled" "$@" 2>&1 | grep -E "exported|no function|ERROR" || true
