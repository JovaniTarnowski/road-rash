# Handoff: frozen cadence translation + solo traffic isolation (plan 10) — October 15, 2026

## Task / plan

Plan 10 Task 6 fix order #1c (frozen scheduler cadence), translation half.
Follows `sim-sched-dump-20261015.md` (dump + `sched900` capture). Baseline:
`95af953` (pushed).

## What landed (uncommitted at handoff time; commit after gate)

- `src/race.hpp`: `Rider::sched_last/next/interval` (+0x10/+0x0c/+0x14) plus
  `sched_live` gate (false = legacy/live behavior, dt 1). Declared
  `step_frozen_schedule`.
- `src/race.cpp`: `step_frozen_schedule` (0x00437e60 head + LAB_00438108
  gate shape, same as traffic/pedestrian cadence; no RNG, no alloc);
  wired before the frozen early-return in `step_rider` and before the
  frozen skip in `countdown_step` (loop restructured with identical
  non-frozen behavior); countdown comment updated. No contact/collide/move
  changes: `collide` calls `contact` both ways, so the frozen bike's own
  dt-4 push flows through the existing `s.i32(0x18)` read.
- `tests/frozen_state.hpp`: hydrates sched state + bike +0x18 from per-tick
  `sched.<addr>.*` keys when present (strict: partial keys, overflow and
  interval < 1 throw atomically); legacy captures untouched.
- `tests/frozen_state_test.cpp`: sched hydration, legacy default, partial
  failure atomicity, interval rejection.
- `tests/oracle_sim_test.cpp`: solo replay detaches native traffic
  (`set_traffic(nullptr)`, null-safe) to match the oracle-nulled pool.
- Docs: capture spec translation note (`REVERSE_ENGINEERING.md`), replay
  receipts (`VERIFICATION.md`), timeline (`PROGRESS.md`), plan note.

## Evidence

- `sched900-20261015.tsv`: 900 ticks, complete, exit 0, proxy `1b8e2357`.
  All 14 frozen bikes interval 4, lasts 1,5,...,209, dt 4 (dt 5 first
  window from the build `+0x10 = tick - interval` init). Player interval 1.
- Single-anchor `--driver=demo` replay: tick 210 -> 319 (traffic fix) ->
  **tick 376**. Ticks 208/209 take 3840-unit nudges; RNG in sync throughout.
- Old goldens byte-identical (sched_live gate): human tick-210 rng pair,
  solo tick-222 lateral, frozen900/mask300 tick-210.
- Tick-376 cause PROVEN from exports (no new capture needed):
  `0x0044dd90` spawn timers draw one lane pick per expiry with lanes > 0,
  then `0x0044df60` returns first-statement on the nulled pool
  (`DAT_0049e654 == 0`). Timer state/rates/taps = Task 7 work. Spare
  player half (rider.1, interval 1, mirrors bike) drew nothing through 375
  and has no native counterpart (open).
- Temp dladdr draw-attribution instrumentation was used for diagnosis and
  fully reverted (diff contains only the 5 + 4 doc files).

## Checks

- Native Debug build exit 0; focused `frozen_state|sim_fields|bike|
  car_contact|oracle-sim` pass.
- Full native gate: 96/97 (`/tmp/fullgate.log`, 482s). Sole failure is
  `agent-tooling/test_doc_checks_pass` over another worker's untracked
  `docs/plans/32-completion-review-next-steps.md` (no index row) — this
  slice adds no plan files, so the failure is independent and pre-existing.
  Left untouched per shared-tree rules.
- `git diff --check` clean. Stage explicit paths only.

## Remaining work

1. Commit + push (message: `sim: model frozen scheduler cadence; isolate solo traffic (plan 10)`).
2. Task 7: `0x0044dd90` timer state/rates/tap lanes with draws-but-no-spawn
   under the nulled pool; live traffic untouched.
3. Re-capture human solo with the sched dump (human golden lacks sched keys;
   same grid -> same hydration fix expected past 210).
4. Native spare-half object (rider.1 live think) — model or prove benign.

## Held resources

- Gate shell above; owned `build-verify-20261015`; capture shell done.
  New golden `analysis/oracle/sched900-20261015.tsv` is gitignored local
  evidence (register in CTest only via the missing-golden SKIP pattern, if
  at all — integrator call).
