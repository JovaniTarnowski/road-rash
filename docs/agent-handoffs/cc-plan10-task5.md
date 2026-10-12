# cc-plan10-task5: player physics replay (plan 10 Task 5)

Task: plan 10 Task 5 Step 1–2, first divergences of the solo replay.
Baseline: `49db329`. Worktree `/Users/archit/Dev/rr/native-port-cc`, branch
`cc/plan10-task5`, build directory `native-port-cc/build`. `analysis/oracle`
and `analysis/decompiled` are symlinks to the main checkout (read-only use).
Not merged into `port/playable-napa`: another session had uncommitted edits
to `src/race.cpp`/`src/traffic.*` in the main checkout. The integrator
cherry-picks this branch.

## Change 1: AI lane bias (commit 7695b06, corrected here)

`0x00405990(..., param_3 = slice, param_4 = grid offset, ...)` places the
bike at the slice centre plus `param_4` and passes the same `param_4` to
`0x00407990`, which stores it as brain lane bias `+0x190` for the player
(`0x00464218 + level*8`) and every opponent (`0x00464088`/`0x00464108`).
The `Race` ctor now sets `brain.bias = grid lateral`; the harness default
bias is that value (was a guessed 7). Measured: `sim-napa-solo-human.tsv`
`brain_190 = 16384`.

Correction: 7695b06 also added `road_base_` to the two AI target sites.
The human-era capture disproves it: original `+0x194` is 18688 at tick 177
and 18176 at green, exactly `road_base` (1792) below that build. The AI
centre is in the route's own frame; this commit removes the addition.

## Change 2: track ring RNG (new `src/track_ring.*`)

The original builds and steps a 128-slice ring with two course walkers
(`0x0049ee30` front, `0x0049ea20` back). Their feature iterators draw from
the shared CRT stream. `TrackRing` ports the random-consuming part:

- setup `0x004552e0`: `0x00453e60` DFS over the node graph jitters/sorts
  every section's DZHR records (`0x00451ee0`/`0x00451fc0`); both walkers
  enter the root section (`0x00452f50` -> `0x00452cb0`; LIHR init draws 16
  in `0x00450fc0`); both walk to camera-0x3000 (`0x00453ae0`); the front
  walker builds 128 slices (`0x00454c80` -> `0x00454240`), stepping after
  each;
- walker step `0x00453030` -> `0x00452e10`: terrain `0x004516e0`
  (walls `0x00451220`: 3 draws per side per new sample; hills
  `0x004515a0`/`0x00451380`: draws when an interval expires), BORR
  `0x00451e20`, DZHR index `0x00452230` (index may end one past the last
  record); section change re-enters `0x00452cb0`;
- slice build `0x00454240`: `0x00451cb0` runs twice (one draw per object
  when the record has random spacing, bit 11), then `0x004520d0` draws the
  lateral jitter for each unfired actor record and calls `0x004409e0`
  (pedestrian: one trigger draw when the 15-slot pool allocates; police:
  two draws only once the player exists, `DAT_004642d8`);
- crossing `0x00454120`: back step, slice build at the old front, front step.

Evidence (`build/ring-probe`, `tests/ring_probe.cpp`): stepping the MSVC
LCG back from the green anchor, only K = 2363 reproduces the draw counts of
all 297 slice crossings in `sim-napa-solo.tsv` (traffic timers add 0-2 per
tick). `srand(1)` plus exactly 11,461 draws reaches the anchor, so the ring
setup starts at setup draw 9,098, consumes 2,352 and ends 11 draws before
green. In the replay, native setup from `srand(seed)+9098` closes exactly on
the anchor (checked every run) and the RNG then matches through tick 184.

Open: `0x0044b250` runs the ring before `0x00405600`, whose 15
`0x00405990` lane draws plus its trailing `_rand()` exceed the 11 measured
post-ring draws. Either the bootstrap path orders setup differently or the
model has 4+ value-neutral extra draws late in setup. Ring alignment is
pinned by the crossings either way. The frame-0 dump also lacks the model's
fourth setup pedestrian (sample 63, type 7, lateral +133120).

Integration: `Race::set_track_ring` runs setup with player-absent spawn
semantics and advances the ring at the `0x00454120` point; its DZHR actors
replace the precomputed `build_pedestrian_slices` walk (`spawn_pedestrian`
now takes the jitter draw). Only single-road sections are walked; split,
merge and backward crossings set `unsupported()`. Terrain values are still
precomputed by `CourseScene` (private stream) for physics and rendering. The
game is not wired yet (`game_main.cpp`/`race_setup.cpp` untouched).

## Change 3: frozen solo field

The solo hook freezes every scheduler node but the player's. It still builds
the full field: the frame-0 node dump has 14 bike/half pairs at
13056..31488, three pedestrians, intersection signals. Golden `rider.1` is
the player's own half (think `0x00434660`), not an opponent. The harness now
builds `original_opponents(..., 14)` frozen; `Race::step_rider` and
`advance_ride_animation` skip frozen riders, which stay visible to scans and
contacts. `Race::set_freeze_actors` stops pedestrian thinking.

## Next divergence: tick 184, AI state machine

The human-era capture (`sim-napa-solo-human.tsv`, AI branch, same run)
exposes the brain. The original thinks during the 59 countdown ticks:
`+0x188` counts, replan at tick 59 latches neighbours (13056), sets current
81, and at tick 60 `0x00408ad0` moves to state 2. Native starts at green in
state 1 with no target. Frame 0 also carries stale brain words (`+0x194`
10240, `+0x198` 14016, `+0x1a4` 108, `+0x180` latched) from the global
brain copy's previous use. Later the native AI enters state 4 (not
dispatched natively) while the original stays in 1 and enters state 5
against the frozen bike at 13312 at tick 183 (target x 43264).

Next actions (plan 10 Task 6 / plan 11 Task 5 scope):
1. seed the demo brain from the golden's frame-0 brain window and run
   native countdown AI ticks (think, no movement, no draws) for the hold
   frames;
2. port the state handlers `0x004092d0` (2), `0x00409460` (3), `0x00409ca0`
   (4), `0x0040a560` (6) and drop the native-only "idle" fallback in
   `ai_state_racing`;
3. re-run `RR_SIM_CONTINUE=1 RR_SIM_TRACE=rider.0. oracle-sim-test
   --driver=demo sim-napa-solo-human.tsv`: the brain line prints native vs
   golden state, target, neighbours, steps and speeds per tick.

## Tooling

- `RR_SIM_TRACE=<prefix>` on `oracle-sim-test` prints each replayed tick's
  native fields with that prefix, marks golden mismatches `/orig <value>`,
  and prints the player brain against the golden brain window.
- `RR_SIM_CONTINUE=1` keeps replaying past divergences (diagnostic).
- `build/ring-probe <golden> <disc> [maxK] [police]` locates the ring setup
  in the setup stream.

## Checks

- `cmake --build build --parallel`: pass.
- `ctest --test-dir build --output-on-failure`: 70/70 pass, none skipped
  (new `track_ring` synthetic test).
- Sanitizer tree `build-san` (`-fsanitize=undefined,address
  -fno-sanitize-recover=undefined`): `track_ring`, `race`, `pedestrian`,
  `oracle-sim`, `terrain`, `course` pass.
- `oracle-sim` diagnostic pin updated to the tick-184 output
  (`tools/sim_known_divergences.json`).
- `verify-sim` acceptance still fails (no true human-branch golden, no
  capture receipt): expected.

## Files

Changed: `src/race.cpp`, `src/race.hpp`, `src/race_pedestrians.cpp`,
`src/track_ring.{hpp,cpp}` (new), `tests/oracle_sim_test.cpp`,
`tests/track_ring_test.cpp` (new), `tests/ring_probe.cpp` (new),
`CMakeLists.txt`, `tools/sim_known_divergences.json`, this handoff.
No processes or locks held. Shared docs (PROGRESS, VERIFICATION,
REVERSE_ENGINEERING, SIM_SCENARIOS) are left to the integrator: merge the
sections above.
