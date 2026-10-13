# attract-port-20261012

Task: Plan 13 Task 5 Steps 2–3 — failing test + attract demo port. Tree clean + index empty at start (verified); unowned slice only.

Baseline: `fafbeff` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build`

## Test (failing first)

`tests/frontend_test.cpp`: idle 60 s → demo `start_race` course 1; second window → course 2. Failed first at compile (`demo` member missing), then caught a real semantic error: my initial input-disarms-timer design contradicted the trace.

## Correction from evidence (KillTimer(9) census)

I first implemented input-disarm (nicer UX). The test failed; a census of all 8 `KillTimer` call sites showed timer 9 is killed ONLY at menu-entry re-arm (`0x421e31`) and at fire (`0x423cbd`) — never on input. Removed the disarm: the timer fires 60 s after menu entry even mid-navigation, matching the original. Other IDs (0/7/0xb/0xd) belong to credits/video paths.

## Port

- `src/frontend.hpp`: `attract_at_`/`attract_course_` fields; `FrontendEvent::demo` flag.
- `src/frontend.cpp`: `enter(9)` arms +60 s; `update()` fires demo `start_race` (course cycles 1–5) and returns early.
- `src/game_main.cpp`: `RaceOptions.demo`; `run_race` exits quietly on any key (no repeat) / gamepad button / pad-added / tap / mouse; demo branch in the `start_race` handler reuses career setup + `set_demo_hold(true, 95)`, plays race MIDI, restores menu music, re-enters menu (re-arm). Demo rider seed (base 95) follows the oracle pattern; exact demo-grid detail remains an approximation (recorded).
- Simplification vs original (documented): every idle window starts a demo race; the original alternates demo / JESSIE-DUEL-video on the toggle byte. The videos have jukebox triggers; plan Step 2 needs only the demo race.

## Coverage boundary (honest)

- Scripted: idle→demo event + course cycle (frontend_test, 0 failures); related suites 5/5 pass; full build links.
- NOT scripted: in-demo key/button/pad-added exit (game_main SDL-event scope, no headless harness — same category as the plan's "Listen" checks) and 60 s wall-clock idle. Needs a manual run: idle on menu → demo starts → keypress → menu.
- Controller-connect exit ported (`GAMEPAD_ADDED` in demo_exit) but manually verified only.

## Checks / remaining work

- Staged-fileset hygiene: verify `git diff --cached --name-only` == 6 pathspecs (frontend.hpp/cpp, game_main.cpp, frontend_test.cpp, plan 13, handoff) before committing.
- Full 71-test gate left to CI/integrator.
- Integrator: Task 5 done; link from next index refresh.

## Live processes / resources

No live REA/Ghidra/Wine processes. /tmp scratch (atdbg) outside repo. No build locks.
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
