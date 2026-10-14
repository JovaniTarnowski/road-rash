# Handoff review-31-1: facts for the PROGRESS/index refresh (plan 31 Task 1)

Sources: `rg -n CMAKE_CXX_STANDARD CMakeLists.txt`, `git log --oneline -60`,
plan checkboxes, `docs/CURRENT_STATUS.md`, `rg -n "police|cops" docs/PROGRESS.md`.
No verification claimed beyond these.

- Language standard: C++20 (`set(CMAKE_CXX_STANDARD 20)`, CMakeLists.txt:18).
  PROGRESS header says C++17 — stale.
- Plan checkboxes done since Oct 11: 25 (Tasks 1–3, 5–7), 26 (all + 2 follow-up
  fixes), 27 Task 1, 28 (all 4), 29 (all 4), 30 (all 5). Plan 21 gained Task 10
  (wide menus, open). Plan 12 superseded by 17 (already marked).
- Index rows 25–31 exist (commit 4f9eafc) but rows 28/29/30 say "in progress" /
  "Not started" — stale after `7ca92ba` (RaceScreen), plan-29 commits, plan-30
  commits. Row 26's follow-up note is current.
- CURRENT_STATUS.md: generated from agent-status.json; check `make agent-status`
  output before editing (do not hand-edit per plan constraints).
- Police/pedestrians as last recorded: traffic spawning/placement ported (not
  oracle-verified); collisions, DZHR scripted vehicles and police not started
  (PROGRESS "Status by area" row still current; plan 24 owns the rest).
- Courses: all five use the original scene pipeline via the shared factory
  (plan 26); "other four use placeholder" is stale.
- Tests: 94 CTests (labels slow/sim/disc/unit, plan 30); strict warnings clean
  (`RR_STRICT_WARNINGS`); full gate last measured 93-test serial 554 s at 99%
  (1 fail, since fixed); fast set 86/86 in 19 s.

## Draft: PROGRESS.md opening paragraph

Replace "The port is C++17" with "The port is C++20". Replace "Napa draws with
the original scene pipeline ... The other four courses still use the
placeholder renderer…" with: "All five courses draw with the original scene
pipeline through the shared race factory (plan 26); the 640×480 output stays
bit-identical at render scale 1. Races use the ported bike physics, AI driving,
rider contact, combat and free traffic; none of these has yet been compared
with the original tick by tick (plan 10's oracle replays with a known
divergence at frame 207)."

## Draft: "Status by area" rows to change

- "Playable race (`rr-game`)": "Single course, solo ..." → "All five courses,
  quick-race and career, keyboard/gamepad/touch, countdown, HUD,
  finish/restart; race loop as `RaceScreen` (plan 28)".
- "World rendering ...": drop "Other courses and the viewer: placeholder
  shading" for courses (viewer still placeholder-shaded — keep that half).
- "Traffic, police": append "Career races keep the purchased bike; bad `.CAR`
  degrades to no-traffic (October 13 review follow-ups)".
- "Front end ...": append "Native Display/Sound/Pause/Enhancements screens,
  visual-style presets, single widescreen setting (October 13)".
- Add row: "Test/build infrastructure | CTest labels (slow/sim/disc/unit) +
  `test-fast`/`test-sim`, shared `Checks`, `RR_STRICT_WARNINGS` clean,
  `ByteReader`/`parse_iso_record`, claim-aware build pruner | plan 30".

## Draft: 00-INDEX.md "Where things stand" header

New date October 14, 2026, same structure: playable game on macOS + Android
APK; front end as before; all courses on the scene pipeline; physics/AI/contact/
combat/traffic uncompared (oracle known divergence); audio/video as before;
tests 94 CTests with labels, strict clean.

## Draft: index status-table cells

- 19–24: leave (no change this cycle).
- 25: Done (no change).
- 26: Done (no change; follow-up note current).
- 27: Task 1 done; Tasks 2–3 decision-gated (no change).
- 28: "Tasks 1–4 done (`b50dac8`, `a3c1c68`, `5632e71`, `7ca92ba`); plan 23
  Task 3 note open (orchestrator scope)".
- 29: "Done (Tasks 1–4): benchmark + counter, cached RGBA levels, band-threaded
  RGB (bit-identical), persistent band scratch".
- 30: "Done (Tasks 1–5): labels/fast targets/guard, Checks, strict clean,
  ByteReader/ISO parser, build pruner".
- 31: "In progress (this plan)".
