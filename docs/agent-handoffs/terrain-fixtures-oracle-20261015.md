# Seeded terrain original-function fixture mode

Plan10 Task5 follow-up to c38bcfb. Baseline isolated checkout `c452be2` at
`/Users/archit/Dev/rr/.codex-terrain-20261015/native-port`.
Owner terrain_review: ONLY `oracle/aweman_proxy.c` and this handoff.
Sibling timer_tests owns native terrain diagnostics/tests; their edits preserved.
Main OMP checkout, prefix/install/Wine and shared docs untouched.

## Change

New opt-in `RR_ORACLE_TERRAIN_SEEDED=1` defers component capture to the first
existing FRAME_HOOK entry, before orig_frame. It calls actual reference
`0x004510c0` init and `0x004516e0` step with byte-identical synthetic hill
fixture(2), snapshots original iterator memory and live CRT RNG, writes one
frame0 plus end, flushes/closes and calls ExitProcess(0). Failures in state
lookup/file writing exit1 and cannot publish as success. The exit occurs from
a normal frame callback, after DllMain returned, avoiding loader-lock exit.
This is pre-race component evidence, not human simulation evidence.

Existing default, RASTER and fixed-value terrain capture paths remain intact.
Run with RASTER=0 and SIM=0; combining the new mode with RASTER=2 would close
the output in DllMain before this new first-frame probe and must not be used.
No new function/RNG detours. Save/restore live CRT seed, random mode
`0x004753b0` and fixed value `0x0049bce8`. Mode0 delegates to original CRT
through `0x00441750`. No rendering input changes or golden edits.

Schema agreed directly with timer_tests:

- `terrain.seeded.schema = 1`.
- Seeds indices0..5 map to {0,1,123,32767,0x12345678,0xdeadbeef}.
- Path0: initial0, 48 pre-step snapshots, +256 each step.
- Path1: initial48*256, 48 pre-step snapshots, -256 each step.
- Path2: initial0, 64 pre-step snapshots, first32 +256 then32 -256.
- `terrain.seeded.<seed_index>.<path>.<tick>.<field>` fields:
  rng (full unsigned32), position,last_sample,record;
  point.<0..7>.<start,end,x,height,dx,dh> (signed int32).
- `terrain.seeded.<seed_index>.<path>.final_rng` records state after last step,
  so the final step's draws cannot escape comparison.

Original offsets: position+c,last_sample+10,record+14;
point starts+44 stride4,ends+64 stride4,x/h+84/+88 stride8,
dx/dh+c4/+c8 stride8. Buffer zeroed per case, all80 words initialized.
Offsets traced against 450fc0/451380/4515a0 assembly; prior read-only REA
4515a0 matches export and EXE reference hash was verified there.

## Validation

`./oracle/build.sh > /tmp/terrain-fixtures-oracle-20261015-build.log 2>&1`
completed exit0; empty build log, no compiler warnings. The wrapper acquired
and released only this checkout's oracle/build resource.
Private proxy: `oracle/build/AWEMAN32.DLL`, SHA256
`4160cb58480c137bd31d3486008b80a8a654c8e4e258f6b0c55c7dad7d5c383b`.
`git diff --check` exit0. No Wine/install/capture was run. Native suite and
runtime original comparison are integrator/sibling gates, pending this worker.
C++ LSP not applicable to this Windows-only C file; target MinGW compilation
is the relevant syntax/type check. Existing tooling fallback was exercised
on terrain.cpp during earlier read-only research and documented separately.

## Integrator capture recipe (not run here)

Install the reviewed built proxy through the coordinated install workflow,
then use unique output; do not overwrite raster-fixtures.tsv. Main wrapper
and prefix must be used, because isolated checkout's ../cdrom is absent.

```sh
env -u RR_ORACLE_INPUT -u RR_ORACLE_RECORD -u RR_ORACLE_RANDCOUNT \
  -u RR_ORACLE_RANDTRACE -u RR_ORACLE_RANDLOG -u RR_ORACLE_COURSE \
  RR_ORACLE_TERRAIN_SEEDED=1 RR_ORACLE_RASTER=0 RR_ORACLE_SIM=0 \
  RR_ORACLE_FRAMES=1 RR_ORACLE_DRIVE=0 RR_ORACLE_PIXELS=0 \
  RR_ORACLE_GROUPS=0 RR_ORACLE_POLICE=0 RR_ORACLE_SOLO=0 \
  RR_ORACLE_HUMAN=0 \
  python3 /Users/archit/Dev/rr/native-port/tools/oracle_capture.py capture terrain-seeded-20261015 \
  --workspace /Users/archit/Dev/rr \
  --prefix /Users/archit/Dev/rr/prefix \
  --install /Users/archit/Dev/rr/prefix/drive_c/ElectronicArts/RoadRashOracle \
  --output /Users/archit/Dev/rr/native-port/analysis/oracle/terrain-seeded-20261015.tsv \
  --session terrain-seeded-20261015 --timeout 600
```

Require wrapper exit0, one-frame complete receipt with correct proxy/reference
hashes, then run sibling terrain-test against the separate golden. On mismatch
report first differing hidden window/record/RNG field; do not weaken acceptance.

Edits frozen after build. No commits. No held resource or live worker process.
