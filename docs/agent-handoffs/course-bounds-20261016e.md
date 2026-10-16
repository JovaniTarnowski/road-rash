# Handoff: Plan 17 Task 1 — course/level start-finish bounds (course-bounds-20261016e)

- Plan/task: Plan 17 (`docs/plans/17-all-courses-port.md`) Task 1, Steps 1/3/4
  (trace bounds, failing test, implement). Steps 2 (census) and 5 (tracker)
  NOT done — left for the integrator.
- Baseline: HEAD `1b79351` (tree dirty with other agents' work; preserved).
- Owner files (claimed `cb0f3c7c`, session `course-bounds-20261016e`):
  `src/course.cpp`, `src/course.hpp`, `tests/course_test.cpp`, this handoff.
  Build dir: `build-agent-course-bounds-20261016e` (workspace-assigned; parent
  asked for `build-agent-course-20261016e` — used the assigned one instead so
  the claim tracks it; no other worker's dir touched).
- `src/race_setup.cpp` is claimed by `codex-plan32-20261015` — NOT touched.
  The route-A-end fallback there (lines 99–107) is still live; swap-in is
  flagged as remaining work for that owner (see below).
- No commits/pushes. No REA calls (existing exports + direct table reads
  sufficed; the one-call budget was not needed).

## What changed

- `src/course.hpp`: forward-declares `struct RaceBounds` (defined in
  `src/race_setup.hpp`, which includes this header — defining it here would
  collide) and declares `original_course_index(int)` +
  `race_bounds(exe, course, original_course, level)`.
- `src/course.cpp`: implements both. Start = player-grid position word from
  EXE `0x00464218[level]` (levels 0–3: `0x3200`, level 4: `0x3300`).
  Finish = `finish_event_sample(course, routeA, level) << 8` on route A
  (branch bits enumerated explicitly as all-false via `make_route_choices`,
  not the shorthand). Throws on out-of-range course/level, unreadable EXE
  tables, missing finish, or finish off route A. `route_hint = 0`.
- `tests/course_test.cpp`: CLI mapping checks (incl. throw on -1/5/6);
  all 25 (course × level) bounds checks on explicitly enumerated route A:
  exact start, exact finish line, `route_hint == 0`, `start < finish`,
  finish on route; throw checks for bad course/level; menu-mileage report
  (read, printed, NOT asserted — see evidence note).

## Baseline + evidence (EXE `73d7f974…f18a2`, verified by hash)

- Player start chain: `0x00405600` player build (project export
  `analysis/decompiled/00405600.c:65-71`) selects its slot from
  `PTR_DAT_00464298 → 0x00464218` by `DAT_004b8a10` (level); `0x00405990`
  → `0x004387a0` → `0x00438930` resolves the sub-slice. Direct EXE read
  (this session): `0x00464218` rows L0–L3 `(lateral, 0x3200)`, L4
  `(16384, 0x3300)`; `0x00464298[0] = 0x00464218`. Ring origin in
  `0x004552e0` is the constant `0x3200` (REA re-confirmed Oct 12).
- Finish chain: `0x004525c0` first raw-type-6 CESR event with
  `signed param[0]-level == 1` → `0x00453e60` section+0x2c → `0x00454240`
  `DAT_004b6e58 = section_base + finish*0x100` (first-write-wins gate),
  crossing in `0x00406890` (see `analysis/course-finish-notes.md`,
  `analysis/terrain-interleave-20261016b/rea-00454240.md:239-241`).
- Course order `0x0046df58`: City, Hiway, Medly, Napa, Canyn (= original
  1..5; CLI NAPA 0→4, CITY 1→1, HIWAY 2→2, CANYN 3→5, MEDLY 4→3).
- Mileage table `0x00468f58` (25 u32 tenths, read this session):
  CITY 53/84/115/135/163, HIWAY 53/79/98/129/171, MEDLY 53/80/110/150/168,
  NAPA 50/76/106/128/185, CANYN 55/81/110/126/161.
- Plan-vs-evidence discrepancy (Step 3 premise refuted by Step 1 trace):
  the table is menu display text (`analysis/course-finish-notes.md:32`),
  NOT race bounds. Measured samples-per-tenth over all 25 route-A bounds
  ranges 32.2–33.4 with no exact law, so the test reports the table
  without asserting equality. Owner/integrator to adjudicate if plan text
  needs amending.

## Checks (owned build dir only)

- `cmake -S . -B build-agent-course-bounds-20261016e -DCMAKE_BUILD_TYPE=Debug` → ok.
- `cmake --build … --target course-test course_scene-test` → ok.
- `ctest -R course` → 2/2 pass (`course` 0.36 s incl. 25 new bounds lines
  + mileage report, `course_scene` 66.54 s; 0 failures; prior census,
  route-sample, and Napa-event assertions still green).
- `tools/agent_lsp.py --check` on both edited sources: AST builds, no code
  diagnostics (only IDE tweak-probe noise); fell back to shared
  `build-clangd` CDB read-only (owned dir has no compile commands).

## Remaining work (not mine — do not duplicate)

1. `codex-plan32` owner: replace the `race_setup.cpp:99-107` fallback
   (`start 0x3200`, `finish route_a end`) with
   `race_bounds(exe, *out.course, original_course_index(...), level)`.
   Needs the CLI→file-name mapping for the course number; note L4 start
   becomes `0x3300` (one sample ahead of the current hardcoded `0x3200`).
2. Integrator: plan 17 Task 1 Steps 2 (census) + 5 (`COURSES_IMPLEMENTATION.md`
   tracker), `REVERSE_ENGINEERING.md`/`PROGRESS.md` merge, and the eventual
   move of `struct RaceBounds` into `course.hpp` (my forward declaration
   stays valid through that move).
3. Per-route finishes differ by branch (HIWAY/MEDLY branch tables in
   `course_test.cpp`); only route A is pinned here. The driven route's line
   is a `race_setup`/game concern (`DAT_004b6e58` first-write-wins).
