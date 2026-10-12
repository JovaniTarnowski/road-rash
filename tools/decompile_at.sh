#!/bin/sh
# Extra address export; keep the full log and preserve headless exit status.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
GHIDRA_HOME=${GHIDRA_HOME:-/opt/homebrew/opt/ghidra/libexec}
export JAVA_HOME=${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}
exec python3 "$ROOT/tools/agent_resource.py" --session ghidra-export \
  --resource "$ROOT/analysis/ghidra-project" --resource "$ROOT/analysis/decompiled" \
  --log "$ROOT/analysis/decompile-at.log" -- \
  "$GHIDRA_HOME/support/analyzeHeadless" "$ROOT/analysis/ghidra-project" RoadRash \
  -process ROADRASH.EXE -noanalysis -scriptPath "$ROOT/tools/ghidra" \
  -postScript DecompileAt.java "$ROOT/analysis/decompiled" "$@"
