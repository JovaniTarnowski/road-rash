#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec python3 "$root/tools/oracle_install.py" --workspace "${RR_WORKSPACE:-$root/..}"
