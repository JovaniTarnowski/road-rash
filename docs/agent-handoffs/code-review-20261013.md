# Handoff: full code review and plans 25–31 (2026-10-13)

- **Task:** review the native-port codebase; propose improvements as plans
  structured for subagent orchestration.
- **Baseline:** `57e6b3b`, plus another session's uncommitted C++20 switch
  (`CMakeLists.txt`, `tests/view_test.cpp`) and untracked `src/byte_reader.hpp`,
  `tests/byte_reader_test.cpp`, `analysis/cpp20-*`. Those changes are not mine
  and were not touched.
- **Files written (docs only):**
  - `docs/agent-review/CODE-REVIEW-2026-10-13.md` (findings and evidence)
  - `docs/plans/25-…` through `31-…`, and `docs/plans/REVIEW-ORCHESTRATION.md`
  - `docs/plans/00-INDEX.md`: seven rows appended to the plan status table
    (no other edits)
  - this handoff
- **Checks run:** a private Debug tree under the session scratchpad (not in the
  repo) with strict warnings: build OK; 105 unique first-party warnings;
  `ctest` 81/81 passed in 566 s. No ASan/TSan/Release/Android run; no oracle
  capture. No source code changed.
- **Confirmed defects for the integrator to prioritise:** P0 in the review
  (the game's `Race` lacks personalities, EXE, ride library, nitro, ring and
  finish), then plan 25 Task 1 (restart dead since `aa96b53`).
- **Held resources / live processes:** none. The scratch build dir is outside
  the repo and needs no cleanup.
- **Next action:** owner review of the plans. The integrator then starts wave
  W1 per `REVIEW-ORCHESTRATION.md`. Plan 27 Tasks 2–3 need owner approval of
  the naming convention first.
