# ring-terrain-20261016

Task: integrate the live ring terrain RNG so the Napa human replay passes
tick 189 (plan 10 Task 5).

Baseline: 344711b (main). Build directory: `build-agent-ring-terrain-20261016`.
Claims: sessions `ring-terrain-20261016` and `ring-terrain-course-20261016`,
released at commit. No Wine, prefix, Ghidra or capture use; no live processes.

## Finding

The tick-1 walker terrain gap (front point 0 end 132 vs 133) and the
tick-189 RNG gap came from missing **setup draws**, not hill arithmetic.
`0x00453e60` (DZHR pass) calls `0x00453b60` at every split (+1) and merge
(−1): a scratch walker makes 0x51 `0x00453030(±0x100)` steps through both
halves and its terrain iterators draw. Napa has 3 splits and 3 merges. The
fitted `ring.pre_draws` 9,098 had absorbed them; the ring setup really starts
at srand(1) with 0 pre draws and ends 16 draws before the green anchor.

Eliminated on the way (decompiled text and assembly): per-step `0x00451380`
calls (native matches), BORR random gate (native matches; byte +0x14 only
selects the side), the second `0x00454240` (two-road slices only).
Full write-up in `docs/REVERSE_ENGINEERING.md` (Pedestrians / DZHR bullets).

## Changes

- `src/course.hpp/.cpp`: `CourseNode::primary` = split/merge byte +0x19.
- `src/track_ring.hpp/.cpp`: `probe_enter` / `probe_step` (`0x00452e10`
  terrain part, including the literal backward crossing at `0x452f1d`) and
  `probe_window` (`0x00453b60` draws), called from `jitter_events`.
  Unexpected shapes (a branch shorter than 81 slices) set `unsupported()`.
- `tests/oracle_sim_test.cpp`: defaults pre 0 / post 16; closure failure
  reports the draw offset; `--ring-pre-draws=N` diagnostic override.
- `tests/ring_probe.cpp`: walker-terrain scoring when the capture has
  `ring.terrain.schema`; camera from `ring.walk.back.position + 0x3000`.
- `tests/track_ring_test.cpp`: synthetic split/merge window-walk test.
- `tools/sim_known_divergences.json`: output hashes re-pinned for the two
  legacy fixtures; only the closure line changed (+11 → +16).
- Docs: PROGRESS, VERIFICATION, REVERSE_ENGINEERING, SIM_SCENARIOS, plan 10,
  agent-status/CURRENT_STATUS.

## Evidence

- `terrain-human200-20261015`: 200 ticks identical incl. walker trace.
- `sched-human-20261016`: first divergence tick 371 `rider.0.velocity`
  original 40 native 296, RNG in sync (was tick 189).
- sched600 600 identical, sched900 893 RNG (unchanged).
- Full Debug build, ctest 101/101 after re-pinning.

## Remaining

- Tick-371 human velocity gap (next slice).
- The live ring still stops at a split/merge: forward approach to a merge
  restores the `node+0x1c` snapshot (`0x00453030` event 6) that
  `0x00453b60` builds; port that with dual walkers.
- The pedestrian/police draw count in setup assumes `DAT_004642d8 == 0`
  (no police draws) and the native pool size.
