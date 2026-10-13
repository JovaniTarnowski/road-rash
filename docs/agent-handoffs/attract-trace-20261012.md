# attract-trace-20261012

Task: Plan 13 Task 5 Step 1 — trace the 60-second main-menu timer + attract mode 0 (course, riders, input exit). Static trace only (disassembly + EXE dumps + prior REA/export evidence); no REA, no captures. Tree clean + index empty at start (verified); unowned slice only.

Baseline: `7e7fa6e` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (rerun at commit time for docs-only change)

## Trace findings (all verified this turn unless noted)

- **Timer arm**: `0x00421df0` case 1 (screen 9 = main menu; dispatch map `0x4223ac` dumped: 9→case 1→`0x421e93`): KillTimer(9) + SetTimer(hwnd `0x4753dc`, id 9, 60000 ms, NULL). Re-armed on every main-menu entry.
- **First fire** (`0x423cb7`, WM_TIMER id 9): KillTimer; toggle byte `0x46afec` 0→1; course counter `0x46aff0` incl, 6→reset 1 (courses cycle 1–5; 6 never runs); `0x00421430(course)` rebuilds standings (thrash `0x421060` vs big-game `0x421000` by current mode — export `00421430.c`) and stores course through screen-8 item 0's target (`0x469ec4` = `0x469ec0`+4; screen 8 table dumped: 1 item at `0x469ec0`); `DAT_004753c8 = 1`; course art `0x0041dbc0(0x1b, …)`; `0x00421df0(8)` → loading → demo race with AI player (think `0x00434660` takes AI branch when demo hold set — confirmed at disasm `0x4346a1`; native `set_demo_hold` documents the same).
- **Second fire** (toggle 1→0): `rand & 1` picks JESSIE (`0x46b1f8`) / DUEL (`0x46b1f0`) via `0x004023e0`, then `0x00421df0(9)` back to menu.
- **Mode 0 routing**: prior confirmed evidence reused (`0x00416120` → main menu in attract; RE doc line 445 + results-chain REA bullet). No re-trace needed.
- **Riders**: demo setup reuses the standings builders + `make_race_setup` equivalent with AI-controlled player (native: `set_demo_hold` seeds the player AI brain; oracle demo player = character 1's rider 8).
- **NOT pinned**: the input-exit mechanism (which message/key ends the demo race) and controller-connect exit — no disassembly site identified within this turn's time box. Recorded as open port detail for Steps 2–3 (behavioral test: key → main menu; the port does not need the exact message ID).

## Doc edits

- `docs/REVERSE_ENGINEERING.md`: attract-trace bullet under Game-mode; timer line updated to point at trace; stale "Jukebox does nothing" line fixed (ported last turn).
- `docs/plans/13-music-and-frontend-completion.md`: Task 5 Step 1 checked.
- This handoff.

No source, test, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- Staged-fileset hygiene: verify `git diff --cached --name-only` == my 3 pathspecs before committing.
- Task 5 Steps 2–3 (failing test + port, owner scope): input-exit + controller-exit behavior pinned by test, not by further trace (recommended; trace time-box reached).
- Integrator: link Task 5 Step 1 from next index refresh.

## Live processes / resources

No live REA/Ghidra/Wine processes. No build locks. No scratch files (python inline only).
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
