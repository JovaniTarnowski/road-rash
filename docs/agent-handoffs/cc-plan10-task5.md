# cc-plan10-task5: player physics replay (plan 10 Task 5)

Task: plan 10 Task 5 Step 1–2, first divergences of the solo replay.
Baseline: `49db329`. Changes 1-3 were developed on branch `cc/plan10-task5`
and merged into `port/playable-napa` (`cbd1b54`). Change 4 was finished in the
shared checkout (owner request) with a private build directory `build-cc`,
verified in an isolated HEAD export, and staged without the other session's
uncommitted edits (its `race.cpp` overtake/demo-hold hunks, `race_test.cpp`
cases and `bike.cpp` guards stay unstaged in the working tree).

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

## Change 4: AI state machine, countdown, contact walk (plan 10 Task 6 slice)

All against the decompiled functions (REA-confirmed in the sibling handoffs):

- Replan `0x004082f0`: easing clamps each word to its own target; both
  neighbour scans use cap 0xa00 (0x5000 is the police rider's); the scan
  `0x00408cd0` walks rider halves (mask 8) and accepts the first strictly
  inside its slice's road edges `+0xe0/+0xe4`; the overtake rating's
  near-finish term reads the player's 1-based place (player half `+0x1fc`,
  measured 15 -> 1); equal ratings tie-break as the original (nearer only
  when both present).
- Overtake `0x00408e40`/`0x00449e90`: scans every scheduler object ahead in
  list order (bikes, halves, pedestrians, cars; window 0x960 from the last
  object visited; stops at the first object outside the lateral window);
  latching the replan target or nothing returns false (AL cleared); target
  writes and speed cases ported. Native list order = position, then creation
  (ring-setup pedestrians, then riders bike/half in build order, then later
  spawns), measured from the frame-0 node dump.
- Transition `0x00408ad0` step 2 checks the two lanes independently; lane
  index uses the route centre without `road_base_` (opponent halves sit on
  their exact grid words, so the AI frame is the route frame).
  `0x00408df0` resets `+0x164` and the per-state tick `+0x138 + 4*state`.
- Dispatch `0x00465980`: 1/2 racing, 3/4 follow `0x00409ca0` (state 3's
  attack rolls `0x00409460`/`0x004095d0` not ported: the fight() stand-in
  remains), 5 fight, 6 shadow `0x0040a560`, 0 none. Follow uses
  `0x00449a80` speed matching. The native-only idle fallback is gone.
- Opponent brains: slots 9..14 use row-0 copies (`0x004078f0` at career
  start, speeds jittered by rand()%13-6 then: not reproduced) with `+0x165`
  = slot + 1 (`0x00407990`); native read past the 10-row table before.
- Countdown: `Race::countdown_step()` runs the AI think without movement.
  The player's first scheduled tick has dt 2 (`0x00438930`: last run =
  creation - interval). The harness seeds frozen bike/half laterals and the
  brain window from frame 0 when the capture has them, then compares the
  brain every countdown tick (equal for all 58 ticks on
  `sim-napa-solo-human.tsv`).
- Contact `0x004368f0`: one walk over the position-sorted list. Outer nodes
  are those scheduled this tick (frozen never); partners follow in list order;
  the walk stops at the first partner whose gap exceeds the summed
  half-lengths (template `+0x48`: bike 64, pedestrian 16). A ring-setup
  pedestrian at 13824 therefore delays the first frozen-bike contact to tick
  208, as in the capture. Hazards (`0x004373f0`) run only within the bike's
  half-length of mid-slice. Separation `0x00436a40` is ported line for line
  (`Race::separate`, incl. height stacking and the rule that bikes are never
  pushed sideways when overlapping on every axis). Cars stay in
  `collide_traffic` (11a owner's area; list order approximate there);
  pedestrian pairs are not ported.

Result (single anchor, `--driver=demo`): `sim-napa-solo-human.tsv` matches
every compared field and the brain window through tick 207; tick 208 (first
contact) matches speed, position, lateral, steer rate and lateral velocity,
diverging on `rider.0.steer` (original 2392, native 504) in the contact
handler `0x004049c0`. `sim-napa-solo.tsv` (no node dump) reaches tick 208
too, diverging on position because its frozen bikes keep native grid
laterals (pinned in `tools/sim_known_divergences.json`).

Open:
- frozen bike placement: bike lateral = grid word + (position - 0x2b00)
  for rows 50..54/57; other rows add a lane or more (field not traced);
- `+0x1a0`/`+0x1a4` at frame 0 (70/108) are left over from the global brain's
  previous use;
- the post-ring setup draws (11 measured) are fewer than `0x00405600`'s 16;
- `game_main.cpp` still counts down without `countdown_step()` (shared file:
  coordinate); AI riders there start without their countdown replans.

Next action: trace the steer kick in `0x004049c0` for a bike-bike contact
(tick 208), then state 3 attacks (`0x00409460`, `0x004095d0`, `0x00408d50`).

## Tooling

- `RR_SIM_TRACE=<prefix>` on `oracle-sim-test` prints each replayed tick's
  native fields with that prefix, marks golden mismatches `/orig <value>`,
  and prints the player brain against the golden brain window.
- `RR_SIM_CONTINUE=1` keeps replaying past divergences (diagnostic); the
  countdown lines print as `countdown tick N:` with the brain comparison.
- `build/ring-probe <golden> <disc> [maxK] [police]` locates the ring setup
  in the setup stream.

## Checks

Change 4, isolated HEAD export plus only these changes:
- Debug build: 70/70 pass, none skipped.
- UBSan/ASan tree: `race`, `pedestrian`, `track_ring`, `oracle-sim`,
  `race_setup` pass. `crash` stops on a pre-existing division by zero in
  `apply_spec` (`src/bike.cpp:104`, zeroed spec; unchanged by this work; the
  web-port session's uncommitted `bike.cpp` guards it).
- Shared tree (with the other session's edits): 70/71, `oracle-sim` differs
  because their uncommitted physics edits move the solo divergence.
- `verify-sim` acceptance still fails (no true human-branch golden).

## Files

Changes 1-3: `src/race.cpp`, `src/race.hpp`, `src/race_pedestrians.cpp`,
`src/track_ring.{hpp,cpp}` (new), `tests/oracle_sim_test.cpp`,
`tests/track_ring_test.cpp` (new), `tests/ring_probe.cpp` (new),
`CMakeLists.txt`, `tools/sim_known_divergences.json`, this handoff.
Change 4: `src/race.cpp`, `src/race.hpp`, `src/race_pedestrians.cpp`,
`src/pedestrians.hpp`, `tests/oracle_sim_test.cpp`, `tests/race_test.cpp`
(hazard cases moved mid-slice), `tests/pedestrian_test.cpp` (countdown and
personality tables), `tools/sim_known_divergences.json`, this handoff.
No processes or locks held. Shared docs (PROGRESS, VERIFICATION,
REVERSE_ENGINEERING, SIM_SCENARIOS) are left to the integrator: merge the
sections above.
