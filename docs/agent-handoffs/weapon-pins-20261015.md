# Handoff: weapon lifecycle static pins (plan 16 Task 3) — October 15, 2026

## Task / plan

Plan 16 Task 3, following the read-only `weapon-trace-20261015.md` sketch
items that need no capture: verify the steal-gate conjunction, pin
conservation synthetically, and pin crash/remount preservation. The
attacker flag/frame-2 gate, swish timing and left/right labels stay
capture-gated (plan 11 Task 2) and were NOT touched.

Baseline: `66d629a` (branch `main`) plus the SIM worker's uncommitted
frozen-state integration (preserved untouched).

## Mode

Test-only slice. No source changes (`src/` untouched — the ported
steal/damage/conservation logic verified exact against the trace, see
below). Owned build dir `build-goal-20261015`. This handoff plus
`tests/race_test.cpp` are the only writes.

## What was verified (static, against weapon-trace §(a))

- Steal conjunction (`src/race.cpp:1989-1991`): victim `+0x1cc < tick`
  strict, victim frame < 2, attacker fists, victim armed non-kick, victim
  mid-attack, exactly one `(rand&3)==3` draw — all present and exact.
- Hit deadline (`:1999`): damage skipped while `tick <= +0x1d0` — exact.
- Damage scaling (`:2006-2009`), kick `damage/4+1` reason 5 (`:2009-2010`),
  RNG draw order steal → modulo → conditional &3 (`:1991,2013-2014`) —
  exact per trace §(a).4.
- No-drop/no-pickup/no-carry: `grep` over `src/race.cpp` shows `.weapon`
  writes only at the ctor (`:197`), demo seed (`:258`) and steal
  (`:1992-1995`). `knock_off`/`crash_reason`/`begin_crash`/remount never
  touch `weapon` — the correct port is the absence of such code.

## Tests added (`tests/race_test.cpp`, after the conservation block)

1. Steal occurrence + per-race conservation (1000 seeds): attacker-armed
   outcomes counted (non-vacuous: `successes > 0`) and total weapons == 1
   in all 1000 races.
2. Victim immunity (200 seeds, victim `steal_until = 1<<30`): zero successes.
3. Hit immunity (`hit_until = 1<<30`): 18 punches leave stamina unchanged.
4. Crash/remount preservation: armed rider (chain) takes a lethal BORR
   hazard (reason 1), keeps the weapon through the crash; hazards cleared
   (see gotcha) then remounts within 600 ticks still armed.

## Gotchas found (probe evidence, both fixed in the tests)

1. Parked riders overlap → tick-0 contact knock-off crashes them before any
   swing when `+0x2f0` is the default zero (mirrors the class-1 tests, which
   set `+0x2f0` explicitly). All steal blocks now set attacker/victim
   `+0x2f0` to 1000/10000. Side finding: the pre-existing conservation
   block (victim `attack.timer = 5`, zero condition) only passes vacuously —
   its riders crash at tick 0 and no swing ever resolves. Left untouched
   (another worker's test); the new occurrence block supersedes its
   coverage. Integrator may set its timer to 1 + healthy `+0x2f0`.
2. A crashed rider parked on a hazard span re-triggers every tick
   (`collide_hazards` does not skip crashed riders), resetting
   `crash_timer` forever. The preservation test clears the spans after the
   crash assert; slide → stop → remount follows.
3. Victim `attack.timer` must equal the player's (both 1): the victim would
   otherwise resolve first and zero the player's timer (player steps first
   each tick, so equal timers let the player resolve first while the victim
   is still mid-swing, satisfying the mid-attack gate).

## Checks

- `cmake --build build-goal-20261015 --target race-test`: exit 0, no warnings.
- `./build-goal-20261015/race-test`: 0 failures (2 initially: the vacuous
  gate and the hazard re-trigger trap, both diagnosed via a temporary probe that
  was fully reverted — no probe code remains; `git diff` shows only the
  four test blocks).
- `ctest -R '^race$|^car_contact$|^frozen_state$'`: 3/3 pass.
- `ctest -R 'race-test|car_contact|sim_fields|frozen_state|traffic|pedestrian'`
  (name-substring set): 6/6 pass.
- `git diff --check`: clean. `src/` untouched (`git status` shows only
  `tests/race_test.cpp` modified plus pre-existing SIM-worker files).

## Live resources

None. No background jobs, no locks, no captures. Owned build dir only.

## Next

- Capture-gated remainder for the combat owner: `napa-weapon-steal`,
  `napa-weapon-crash` (trace §(b)), attacker flag gate + swish timing
  (plan 11 Task 2 descriptor word).
- Optional integrator follow-up: make the pre-existing conservation block
  non-vacuous (timer 1 + healthy `+0x2f0`) or delete it as superseded.
- `docs/PROGRESS.md:288` weapon-grab stand-in line is stale relative to
  `resolve_hit` (trace §(a).2) — flagged for the integrator (shared doc,
  not touched here).
