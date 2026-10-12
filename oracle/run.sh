#!/bin/sh
# Usage: oracle/run.sh <scenario-name> [timeout-seconds]
# The timeout is a deadline, not permission to accept a killed/partial capture.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec python3 "$root/tools/oracle_capture.py" capture "$1" --timeout "${2:-600}" \
    --workspace "${RR_WORKSPACE:-$root/..}"
