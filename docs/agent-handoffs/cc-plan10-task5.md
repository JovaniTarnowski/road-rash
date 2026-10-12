# cc-plan10-task5: player physics replay (plan 10 Task 5)

Task: plan 10 Task 5 Step 1–2, first divergences of the solo replay.
Baseline: `49db329`. Worktree `/Users/archit/Dev/rr/native-port-cc`, branch
`cc/plan10-task5`, build directory `native-port-cc/build`. `analysis/oracle`
and `analysis/decompiled` are symlinks to the main checkout (read-only use).
Not merged into `port/playable-napa`: another session had uncommitted edits
to `src/race.cpp`/`src/traffic.*` in the main checkout. The integrator
cherry-picks this branch.

## Change 1: AI lane bias and target frame

Finding (decompile `0x00405600`, `0x00405990`, `0x00407990`, `0x004091f0`):
`0x00405990(..., param_3 = slice, param_4 = grid offset, ...)` places the bike
at `slice+0xcc + param_4` and passes the same `param_4` to `0x00407990`, which
stores it as the brain lane bias `+0x190`. That holds for the player
(`0x00464218 + level*8` word) and every opponent (`0x00464088`/`0x00464108`
words). `0x004091f0` targets `(slice+0xcc + lane*0xfa)*0x100 + curve/8 + bias`,
where `+0xcc` is the absolute centre.

Evidence: `sim-napa-solo-human.tsv` (AI-branch capture with brain fields)
reads `brain_190 = 16384` (level-0 grid word) and `brain_194 = 18176` (the
target equals the player's own lateral) at green. The native build had bias 7
(harness guess) and a relative-frame target (1799), so the demo player steered
left from green.

Fix: `Race` ctor sets `brain.bias = grid lateral` for every rider; both AI
target sites add `road_base_` (0 in the game, the measured absolute anchor in
replay). The harness default bias is now the ctor's value, not 7.

Result: `oracle-sim-test --driver=demo sim-napa-solo.tsv` matches every field
and the RNG through tick 138 (was tick 74). This supersedes the earlier
"frozen rider.1 / replan" explanation of the tick-75 gap.

## Next divergence: tick 139, live terrain RNG (not ported)

Tick 139: original draws 24 values, native none. Bursts recur at every
ring-slice crossing (170: 21, 194: 27, 217: 30, …; plus 1–2 draws for the
traffic timers). The call graph from `0x00454120` (ring advance in
`0x0044b3b0`) reaches these `rand` consumers:

- `0x00453030` → `0x00452e10` → `0x004516e0` → `0x00451220` (`DWDR`/`NTMR`
  walls, 3 draws per profile per new sample) and `0x004515a0` → `0x00451380`
  (`LIHR` hill interval redraws);
- `0x00453030` → `0x00452cb0` → `0x004510c0` → `0x00450fc0` (hill init on a
  new section, 2 draws per point);
- `0x00454c80` → `0x00454240` → `0x00451cb0` (BORR spans), `0x004520d0`
  (DZHR actors).

Native `CourseScene` precomputes terrain at load with its own seeded CRT
stream (`course_scene.cpp`, "full race random-call interleaving is still a
separate integration requirement"). Next action: drive the ring's two terrain
cursors (`0x0049ea20` back, `0x0049ee30` front) inside `Race::step` at the
`0x00454120` point with `Race::random()`, using the existing
`TerrainIterator` (already matches 864 original slices), and make the
renderer/physics read that live terrain. Then the BORR/DZHR builders.

## Tooling

`RR_SIM_TRACE=<prefix>` on `oracle-sim-test` prints each replayed tick's
native fields with that prefix and marks golden mismatches `/orig <value>`.

## Checks

- `cmake --build build --parallel`: pass.
- `ctest --test-dir build --output-on-failure`: 69/69 pass, none skipped.
- `oracle-sim` diagnostic pin updated to the tick-139 output
  (`tools/sim_known_divergences.json`, output hash `daad569c…`).
- `verify-sim` acceptance still fails (no true human-branch golden, no
  capture receipt): expected.

## Files

Changed: `src/race.cpp`, `tests/oracle_sim_test.cpp`,
`tools/sim_known_divergences.json`, this handoff. No processes or locks held.
