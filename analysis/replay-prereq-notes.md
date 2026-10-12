# Replay prerequisites (SimReplay slice, plan 10, Oct 12)

Doc deltas for the owner to merge (do NOT edit PROGRESS/VERIFICATION/REVERSE_ENGINEERING directly).
Baseline: prior ReplayFix slice (`ce8e1eb`) + CourseFinish uncommitted edits (opponent count≤14, grid-slot wiring, `finish_q8_` additive API). No tick-behavior changes in this slice.

## What changed (harness + setup/factory regions only; no tick behavior)

`src/race.hpp` / `src/race.cpp` (setup/factory regions ONLY):
- `Opponent::slot` comment corrected to grid slots 1..15 (`0x00464088`/`0x00464108` via `0x00405600`; was stale 0..7).
- New shared `rr::RaceSetupRequest` config struct (course/level/character/solo/road_base_q8/opponents): one explicit config for game + replay, no hidden replay-only setup. The game (`game_main.cpp`, owned by another slice) calls the same resolvers; player bike/rider resolve via career tables (character 1 → rider 8, `0x00415fc0`; `level_bike` at `0x004641c8`; rider row at `0x00463c78`).
- New shared `rr::race_road_base_q8(golden_lateral, route_offset, grid_word)` helper (`0x00405990` lateral math: `golden - route*256 - grid`). The Race ctor comment now reconciles the frames: game passes 0 (relative route frame), replay passes the measured anchor through the same ctor. Napa solo golden: 1792.
- `Race::snapshot()` now appends `TrafficSystem::append_snapshot` rows (every pool slot, `tests/sim_fields.hpp` car_fields) when traffic is attached. Solo captures carry no car rows (pool nulled) — those stay counted-uncovered; traffic captures will require them.
- No tick-path changes: `step/step_rider/random/move/collide/fight/nitro` untouched. `finish_q8_` (CourseFinish) and `road_base_` coexist; factory paths unaffected until wired.

`tests/oracle_sim_test.cpp`:
- Driver axis unchanged (independent `driver=human|demo` from `player.demo_hold` = `DAT_004753c8`, `--driver` override, never from solo/filename/input) + recorded actual `0x00434660` player-think evidence: reports `player.think`/`player.think_count` when probed, plus measured `rider.0.think`/`rider.1.think` every run. Measured: `rider.0.think == 4225680` (`0x00407990` AI-think callee, demo/AI side); player-think entry `0x00434660` (= 4408928) shows as `rider.1.think`.
- Road-base now via shared `rr::race_road_base_q8` (same ctor mechanism as the game).
- Comparator requires the intended field set: every native snapshot field per live rider + every traffic pool slot + rng. Missing golden field = FAIL with `missing required field ...`, never skip-green. Era carve-outs (car.* absent in solo captures; steer/steer_rate/ground/stamina/weapon/slice/ai/anim/nitro offsets no dumped era covers) stay counted-as-uncovered and reported. Car/police/animation fields are in the required set — the proxy must dump them before acceptance.
- Green boundary unchanged: pre-step anchor = preceding post-step rng, proven by constant rng + `rng_calls==0` across all hold frames else exit 1; ticks absolute (animation deadlines accumulate from absolute ticks).

`tests/golden.hpp`: NO magic bump — `# rr-oracle 1` keeps parsing old captures (explicit `kVersions` enumeration from the prior slice, untouched). Parser absence stays absence, not zero.

`tests/sim_fields_test.cpp`: synthetic replay now asserts exactly one rand call at green + one later with expected MSVC stream values AND field states (steal club at green tick 7, chain later, absolute tick 32), plus a stream-sensitivity control (seed 776 diverges — single anchor detects drift, per-tick re-anchor would hide it), plus a `race_road_base_q8(18176, 64, 0) == 1792` unit check.

`docs/SIM_SCENARIOS.md`: replay section rewritten with think-entry evidence, shared-factory contract, road-base reconciliation, comparator REQUIRED-set rule, and pre-step anchor evidence requirements.

## Verification (run Oct 12 under lock protocol)

- `cmake --build build --target oracle-sim-test sim_fields-test golden-test`: clean.
- `ctest -R "sim_fields|golden|race$|traffic|crash"`: 6/6 pass (`sim_fields` incl. synthetic one-draw-at-green + one-later replay with expected stream values, weapon field states, absolute ticks, seed-drift control, and `race_road_base_q8 == 1792` check).
- `build/oracle-sim-test --driver=demo analysis/oracle/sim-napa-solo.tsv ../cdrom/ROADRASH`: exit 1, output `road_base: 1792 (...)`, `[single-anchor] start-hold: 59 frozen frames, green at tick 60 driver=demo ... rider.0.think=4225680 rider.1.think=4408928 (0x00434660=4408928)`, then `first divergence at frame 74 (tick 75): rider.0.lateral original 18173 native 18053` with 8/8 rng-equal history. Same tick-75 diagnostic pin as the prior slice (frozen rider.1 replan target, task-6 work).
- `tools/sim_known_divergences.json` pin: `divergence` + `trace_sha256` still match, but `output_sha256` (`dca3a712...`) does NOT match the new stdout (extended green report line) — owner must re-pin at commit time or hold until the human golden lands. Diagnostic gate reports `unexpected-failure`, not `known-divergence`; harness-output change, not a simulation regression.
- `RR_ORACLE_DIR=/nonexistent`: direct `verify_sim.py --mode diagnostic` exits 77 clean (SKIP-77 clean at script level); `ctest -R oracle-sim` reports Failed because the CTest wraps the script and the skip propagates as failure through CTest (pre-existing wiring, untouched by this slice).

## Suggested VERIFICATION.md lines (owner merges)

- `sim_fields-test`: synthetic one-draw-at-green + one-later replay passes (expected stream values + weapon field states + absolute ticks + seed-drift control).
- `oracle-sim-test --driver=demo analysis/oracle/sim-napa-solo.tsv`: road_base 1792 via shared helper, 59 frozen hold frames, green tick 60, green through tick 74 incl. rng; first divergence tick 75 lateral 18173 vs 18053 (rng equal; frozen-rider.1/target mechanism, task-6 work). Think words: rider.0.think=4225680, rider.1.think=4408928.
- `RR_ORACLE_DIR=/nonexistent ctest -R oracle-sim` SKIPs 77 clean.
- Strict `verify-sim --mode acceptance` still fails (no human-branch golden, no capture receipts) — expected, not a regression.

## Open questions

1. `RR_ORACLE_HUMAN=1` hook (clear `DAT_004753c8` post-setup) vs a real menu flow reaching `0x00421430`? (carried over)
2. Proxy must still record `player.demo_hold`, `player.think`/`player.think_count`, `demo.*`, `grid.bias`, `DAT_00467a6c`, pre-step RNG, and car/police/animation fields per frame. (extended: car/police/anim now REQUIRED)
3. Unify the game's relative route frame with the absolute sub-slice frame (road_base 0 vs 1792) in plan 17? (carried over)
4. Diagnostic pin `output_sha256` will mismatch if the extended green report line changes stdout — re-pin or hold until the human golden lands?
5. `sim-napa-solo-human-replay.tsv` scratch + `sim-*-probe*.tsv` in `analysis/oracle/`: keep or delete at commit time? (carried over)
6. `RaceSetupRequest` is declared but the game-side call (game_main.cpp, live session owned) still constructs `Race` directly — wiring the game to the struct is follow-up work for the game owner, not this slice.
