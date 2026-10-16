# Free-file survey 20261016c — Plan 32 D/E/F/G × Plans 13/16/17/18/19/22 vs live claims

Baseline: HEAD `aae00a1`, read-only, no builds/REA/captures. Claims from
`python3 tools/agent_workspace.py list` at survey time (3 live-tree sessions):

- `countdown-timers-20261015`: `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`,
  `docs/VERIFICATION.md`, `docs/plans/10-simulation-oracle.md`, `src/race.cpp`,
  `src/traffic.cpp/.hpp`, `tests/oracle_sim_test.cpp`, `tests/traffic_test.cpp`
- `terrain-integrate-20261015`: `oracle/aweman_proxy.c`, `src/terrain.cpp/.hpp`,
  `tests/terrain_test.cpp`
- `codex-plan32-20261015`: `CMakeLists.txt`, `docs/plans/32-completion-review-next-steps.md`,
  `src/race_setup.cpp`, `tests/race_setup_test.cpp`
- `codex-upscale-loading-*` and `codex-human-terrain-setup-*`: private-worktree
  paths only (`.codex-*/native-port/...`) — **no live-tree claims**, but high
  collision risk on `src/upscale_*`, `src/course_scene.*`, `src/race_screen.*`
  if that work merges back. Treat live-tree edits there as free-but-fragile.

Rule used below: a slice is FREE only if its files are all outside the claim
lists above. `src/race.hpp` is unclaimed but tightly coupled to claimed
`src/race.cpp` — flagged per slice, not counted as cleanly free.

## Plan 32 slice D — live race systems (plans 11/16/17/24)

- Task: wire police spawn/pursuit/arrest into live factory + result path;
  replace `Race::walls` stand-in; recover per-level start/finish bounds;
  25-race (5 courses × 5 levels) acceptance matrix.
- Acceptance: all 25 races finish at original bounds; real arrest reaches career
  penalties + busted cutscene; no offline-test substitution for live behavior.
- Files it would touch: `src/police.*` (free), `src/race.cpp` (**CLAIMED**),
  `src/crash.*` (free), `src/course.*` (free), `tests/course_test.cpp` (free),
  `tests/race_soak_test.cpp` (free), `src/race_setup.*` (**CLAIMED**),
  `tests/oracle_sim_test.cpp` (**CLAIMED**).
- Verdict: **mostly CLAIMED** — the live-wiring core (`race.cpp`,
  `race_setup.*`, scenario registration) is owned. Blocked until those release.
- Smallest safe increment (free files only): synthetic threshold tests in
  `tests/race_test.cpp` (free) + knock-off decision scaffolding in
  `src/crash.*` (free) that does NOT change any `race.cpp` call site;
  per-level bounds trace as static code reading written into a **new handoff**
  (no shared-doc edits). No live behavior change.

## Plan 32 slice E — complete-frame performance (R5; plans 19–21/29) — PRIORITY, FREE

- Task: benchmark full scene+actors+dashboard+horizon+upload at 640×480, 2×,
  4×, 4:3/16:9, Original/Enhanced (p50/p95/p99, peak memory, allocs/frame);
  move first-enable upscale attach into cancellable loading.
- Acceptance: no stale/black margins, no per-frame horizon alloc, no
  multi-second first-enable block, sim hashes unchanged.
- Files it would touch: `tests/bench_render.cpp`, `tests/bench_scene.hpp`,
  `tests/dash_bench.cpp`, `tests/hires_render_test.cpp` (all **FREE**, exist),
  `src/upscale_*` (live-tree `upscale_cache/import` exist, free-but-fragile —
  private worktree owns `upscale_load_job` + `race_caches.hpp` variants),
  `docs/VERIFICATION.md` (**CLAIMED** — record numbers in handoff instead).
- Verdict: **FREE for measurement**. Benchmark-harness edits + runs touch only
  free files; results go in a new handoff, not VERIFICATION.md.
- Smallest safe increment: extend the existing bench harness with a
  whole-frame (scene+actors+dashboard+horizon+upload) timing + allocation
  counter at s=1/2/4, run, and publish the table in a fresh handoff. Purely
  additive to free test files; no `game_main`/raster production changes. Do NOT
  touch first-enable loading (collides with private upscale worktree).

## Plan 32 slice F — device acceptance (plan 14)

- Task: adb serial, asset-free install/import, launch, gamepad/touch,
  renderer recovery, sustained-race thermals, storage/interrupt/save/upgrade.
- Acceptance: observed hardware evidence for the whole plan-14 checklist.
- Files it would touch: none in-tree (device + `analysis/` receipts).
- Verdict: **FREE but device-blocked**. No code increment available.
- Smallest safe increment (read-only): audit plan-14 checklist vs existing
  receipts and write the gap list into a new handoff. No Mulch/build needed.

## Plan 32 slice G — release evidence + status (plans 13/15/18/20) — PRIORITY, HYGIENE-FREE

- Task: save round-trip, career/shop/jukebox/settings/movie-music coverage;
  integrator refreshes canonical status matrix; clean-checkout gates; signing
  decision.
- Acceptance: every plan-00 DoD item has fresh source-identified evidence or an
  approved scope change.
- Files it would touch: `docs/PROGRESS.md`, `docs/VERIFICATION.md`
  (**both CLAIMED**), `CMakeLists.txt` (**CLAIMED**), `docs/CURRENT_STATUS.md`
  (free — generated from `agent-status.json`, so edit the JSON/generator, not
  the .md by hand), `docs/SIM_SCENARIOS.md` (free).
- Verdict: **shared-doc edits BLOCKED; hygiene via fresh handoff is FREE**.
- Smallest safe increment: read-only DoD-vs-receipt audit (`CURRENT_STATUS.md`
  gate receipts + `docs/plans/00-INDEX.md` DoD) published as a new
  `docs/agent-handoffs/*.md` (always free). Do not touch PROGRESS/VERIFICATION/
  CMakeLists until claims release; do not hand-edit CURRENT_STATUS.md.

## Plan 13 — music + frontend completion

- Remaining work: Task 6 (original-save round trip + menu pixel comparison);
  Tasks 1/2/4/5 essentially landed (race music lives as `src/race_music.*`,
  not the plan's `src/music.*` names — `src/sbk|midi|synth.*` absent, so the
  plan's filenames are stale; read code, not the plan, before scoping).
- Files: `oracle/aweman_proxy.c` (**CLAIMED**), `tests/career_test.cpp` (free),
  `tests/frontend_test.cpp` (free).
- Verdict: **split** — oracle menu/save dumps blocked; save-parse side free.
- Safe increment: synthetic-only `career_test` save-format edge cases
  (truncated/garbage inputs, hand-made bytes) touching only free test files.

## Plan 16 — gameplay systems (Tasks 1–7)

| Task | Touches | Verdict |
| --- | --- | --- |
| 1 knock-off + rider–cop contact | `race.cpp` CLAIMED, `race.hpp` coupled, `race_test.cpp` free, `SIM_SCENARIOS.md` free | Blocked core; synthetic threshold tests in `race_test.cpp` free |
| 2 roadside objects | `scenery.*` free, `race.cpp` CLAIMED | Step 1 trace write-up only (new handoff); port blocked |
| 3 weapon lifecycle | `race.cpp` CLAIMED, `crash.cpp` free | Conservation-logic scaffolding in `crash.*` free; wiring blocked |
| 4 off-road/reverse/finish | `bike.cpp`+`bike_test.cpp` free, `race.cpp` CLAIMED | Bike-side limits/wrap tests free; finish detection blocked |
| 5 per-tick animation fields | proxy CLAIMED, `oracle_sim_test.cpp` CLAIMED, `sim_fields.hpp` free | Blocked; field-table design note only |
| 6 needle pen + opponent list | `hud.*`, `hud_test.cpp` — **all free** | **FULLY FREE — best buildable slice** |
| 7 determinism/soak gate | `race_soak_test.cpp` free (exists; no CMake change needed) | **FREE** (read-only extension design or additive test body) |

## Plan 17 — all courses

- Task 1 bounds census/tracker: `src/course.*`, `tests/course_test.cpp` free;
  output doc `docs/COURSES_IMPLEMENTATION.md` **MISSING** (free to create).
  **FREE — read code + write tracker, no oracle needed for the scaffolding.**
- Task 2 one race setup: `src/race_setup.*`, `CMakeLists.txt`, `game_main.cpp`
  — **CLAIMED/BLOCKED** (owned by codex-plan32).
- Tasks 3–9: need Task 2 + captures — blocked; sweep-extension design note only.

## Plan 18 — settings/options

- Tasks 2/3/5/6/7/8/9 marked done in-plan; open: Task 4 low/inset views +
  LOBOB (`hud.*`/`view.*`/`hud_test`/`view_test` free; `game_main.cpp`
  shared-but-unclaimed — coordinate, keep diff tiny; oracle pixel goldens need
  captures, so acceptance blocked).
- Safe increment: synthetic low-dashboard compose tests in `tests/hud_test.cpp`
  (free) without changing production call sites.

## Plan 19 — high-resolution rendering

- Tasks 1–8 marked done in-plan; remaining Task 9 Step 2 is an **owner
  decision** (widescreen Hor+), not implementation. Trace (Step 1) already
  recorded in REVERSE_ENGINEERING.md (**CLAIMED** doc — do not touch).
- Verdict: **no free implementation work**; read-only posture. Any widescreen
  prototype would touch `course_scene`/`actor_render` (upscale-worktree
  collision risk) — defer.

## Plan 22 — toolchain hardening

- Task 1 C++20: landed (`byte_reader.hpp` + test exist; before/after hashes in
  `analysis/cpp20-*/`). Task 2/3 fuzz: landed in-tree (`fuzz/*.cpp`,
  `tools/fuzz.sh` exist). Task 4 TSan: `tools/tsan.sh` exists.
- Gap: Task 5 ratchet — `tools/check_warnings.py` **MISSING** (confirm
  `tools/ci.sh` wiring read-only); `.clang-tidy` exists, confirm `tools/tidy.sh`
  + baselines read-only.
- Safe increment: add the warnings-ratchet script + baselines (new files,
  free) and wire nothing shared until CMakeLists claim releases; or purely
  document the gap in a handoff.

## Top 3 free-file opportunities

1. **Slice E measurement** — whole-frame benchmark (s=1/2/4 + allocs/frame) in
   `tests/bench_render.cpp`/`dash_bench.cpp`/`hires_render_test.cpp`; results
   to a new handoff. Touches only free files; unblocks E acceptance data.
2. **Plan 16 Task 6 HUD needle pen + opponent list** — `src/hud.*` +
   `tests/hud_test.cpp`, all free, zero sim-header coupling. Only fully-free
   production slice found.
3. **Slice G status hygiene without shared docs** — read-only plan-00 DoD vs
   CURRENT_STATUS receipts audit published as a new handoff; plus Plan 17
   Task 1 bounds code (`src/course.*`, `tests/course_test.cpp`) and the
   missing `docs/COURSES_IMPLEMENTATION.md` tracker as the follow-on build.
