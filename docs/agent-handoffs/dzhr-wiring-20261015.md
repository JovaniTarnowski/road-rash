# Handoff: DZHR wiring disc coverage (plan 16 Task 2 follow-up) — October 15, 2026

Baseline: `1f6293c` (plus the SIM worker's uncommitted files, untouched).

## Change

The DZHR slice (`1f6293c`) wired factory tables but only synthetic tests
covered the helper and tick rule — the scene→table glue ran only inside
`race-soak`/replay without asserting anything. Added:

- `Race::roadside_solids(int road)` (`src/race.hpp`, header-inline,
  additive): total solid spans on a road, for factory tests.
- `tests/race_setup_test.cpp`: NAPA level-1 solo built twice via
  `make_race_setup`; asserts both roads' solids total > 0 and rebuilds
  agree exactly (course-load seed is fixed: `CourseScene` takes seed 1 in
  `load_race_assets`, CRT recurrence, so placement is deterministic).

## Checks

- `race_setup-test`: `race_setup OK`, exit 0 (disc present).
- Sanitizer tree (`build-goal-asan`, ASan+UBSan):
  `race/car_contact/frozen_state/sim_fields/traffic/pedestrian` 6/6 pass.
- Android `assembleDebug`: BUILD SUCCESSFUL (22 s).
- `git diff --check` clean; `src/race.cpp`, `src/race_setup.cpp` untouched
  by this follow-up.

## Next

Commit `src/race.hpp tests/race_setup_test.cpp` + this handoff with
explicit paths and push. Remaining plan-16-Task-2 work: `napa-crash-scenery`
capture golden; BORR live wiring (blocked on AI avoidance).
