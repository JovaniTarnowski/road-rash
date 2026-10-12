# sim-notes (SingleAnchorReplay slice, plan 10 task 5 step 2a)

Doc deltas for the owner to merge (do NOT edit PROGRESS/VERIFICATION/REVERSE_ENGINEERING directly).

## Divergence report (informational, not a fix task) — CORRECTED per owner follow-up Oct 12

Default single-anchor replay (`build/oracle-sim-test`, no flags) on all
three existing goldens, AFTER the owner's harness fix (load
`read_personalities` + pass `exe_image` to the `Race` ctor in
`tests/oracle_sim_test.cpp`, uncommitted owner-held):

- `sim-napa-solo.tsv` and `sim-napa-solo-human.tsv`: physics green through
  tick 139. Next divergence is frame 138 (tick 139): RNG original
  2574079260 vs native 1133891140, fields still matching — most likely the
  first post-green `0x0044dd90` traffic-spawn timer draw (the solo hook
  nulls the pool via `0x0049e654=0` but the timers still tick and draw;
  the replay never constructs/steps `TrafficSystem`, probed zero
  `Race::random` draws through 85 steps). This is the documented plan-10
  known limitation (setup/stream consumers not ported), now localized.
- SUPERSEDED (harness artifact, not physics): the tick-120 speed gap
  (original 1479 vs native 1474) first reported by this slice root-caused
  to solo replay building `Race` without personalities — the first
  `ai_replan` (tick 119→120) reset speed from zeroed `spd_a`/`spd_b`.
- `sim-napa-solo-straight.tsv` (full field): green at tick 6 (5 frozen
  frames); first divergence frame 5 (tick 6) `rider.1.position` original
  25088 vs native 12800. Native solo builds zero opponents while the
  golden constructs 7 (frozen); opponent slot mapping is task 6 work.

The required first-divergence report format is implemented: tick, field,
original vs native, then the last-8-ticks RNG history (golden vs native).
RNG is now compared FIRST every tick (plan 10 review focus 1); previously
only fields were compared.

## Trace findings

- `0x00434660` head (`analysis/decompiled/00434660.c`): demo hold
  (`DAT_004753c8 != 0`) OR race-not-started (`DAT_0044c5d7c != 1`) takes
  the `0x00407bd0` AI branch and ignores `DAT_004b7ed0`. Player-control
  branch reads control bits accelerate `0x1`, brake `0x2`, left `0x4`,
  right `0x200`, punch/kick/backhand `0x20/0x40/0x80`, lean `0x8/0x400`,
  nitro `0x10` (lines 85–130 of the decompile).
- `DAT_004753c8=0` is written only by `0x00421430` (race grid builder,
  last line before return). It is set to 1 by the attract-demo entry
  (`0x423d03`, reached from the main-menu timer path `0x423c92` on
  screen 7/9 selection). `hook_menu_enter` in `oracle/aweman_proxy.c`
  calls the real `0x00421df0(course)` directly, bypassing the mode
  selection that would reach `0x00421430` — so `0x4753c8` keeps its
  startup value 1 in every `RR_ORACLE_COURSE` capture. That is why the
  "human" input capture still runs the AI branch: the control words are
  recorded in the dump but never read.
- Game mode `0x004753b0` (0 attract, 1 Thrash, 2 Big Game, 3 mano-a-mano)
  and race outcome `0x004c5d7c` (`0x00416010`: 1 racing, 5 wrecked/lost)
  are unchanged by captures; the solo golden runs mode 0/attract with
  outcome 1.
- To get a TRUE player-control golden, the oracle needs a hook that
  writes `DAT_004753c8=0` (and ensures `DAT_004c5d7c=1`) after race
  setup, or a menu path that reaches `0x00421430`. Suggested: new env
  `RR_ORACLE_HUMAN=1` cleared in `hook_step` before the first step.
  Left as an open question for the owner — the current `napa-solo-human`
  golden still has value (scripted control stream + determinism proof +
  single-anchor physics divergence), but does NOT yet exercise
  `apply_controls`.

## Suggested VERIFICATION.md lines (owner merges)

- `oracle-sim-test` defaults to a single RNG anchor at the green light;
  `--reanchor` keeps per-tick re-anchoring as a diagnostic only.
- `sim-napa-solo-human.tsv`: 1,431 frames, self-check + input replay zero
  differences; single-anchor replay diverges frame 119 (tick 120)
  `rider.0.speed` 1479 vs 1474 (RNG equal).
- `RR_ORACLE_DIR=/nonexistent ctest -R oracle-sim` SKIPs cleanly.

## Suggested PROGRESS.md line (owner merges)

- Plan 10 task 5 step 2a done: `--reanchor` diagnostic flag,
  single-anchor default, human-controls scenario documented, first
  divergence reported (tick-120 launch-ramp speed gap).

## Open questions

1. `RR_ORACLE_HUMAN=1` hook (clear `DAT_004753c8` post-setup) — new
   oracle work, or should the human scenario go through a real menu flow
   instead of `RR_ORACLE_COURSE`?
2. The tick-120 speed gap: owner/another slice fixes (physics, not this
   slice).
3. `sim-napa-solo-human-replay.tsv` left as git-ignored scratch in
   `analysis/oracle/` (sibling probe files `sim-napa-solo-human-probe.tsv`
   ditto) — keep or delete at commit time?
