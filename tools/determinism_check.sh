#!/bin/sh
# Same race in Debug and Release must hash identically (race-soak prints hash=...).
set -eu
dbg=${1:?debug build dir}; rel=${2:?release build dir}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/rr-det.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
# Capture exit codes explicitly: a crashing soak must not look like "no hashes".
"$dbg/race_soak-test" > "$tmp/dbg.log" || { rc=$?; [ $rc = 77 ] && exit 77; echo "debug soak failed ($rc)"; exit 1; }
"$rel/race_soak-test" > "$tmp/rel.log" || { rc=$?; [ $rc = 77 ] && exit 77; echo "release soak failed ($rc)"; exit 1; }
grep -o 'hash=[0-9a-f]*' "$tmp/dbg.log" | sort > "$tmp/a"
grep -o 'hash=[0-9a-f]*' "$tmp/rel.log" | sort > "$tmp/b"
[ -s "$tmp/a" ] || { echo "no hashes printed"; exit 1; }
if ! cmp -s "$tmp/a" "$tmp/b"; then echo "Debug/Release simulation differs"; diff "$tmp/a" "$tmp/b"; exit 1; fi
echo "determinism OK ($(wc -l < "$tmp/a") hashes)"
