# pause-keys-20261012

Task: Plan 18 Task 1 Step 3 — pause keys, in-race toggles, Escape quit path (exports + disassembly, no REA/capture). Tree clean + index empty at start (verified); unowned slice only.

Baseline: `17461f3` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (rerun at commit time for docs-only change)

## Findings (exports `004431d0.c`, `00443120.c`, disasm `0x423408–0x42359d`)

- Modal pump `0x004431d0(fmt, param_1)`: key-up/syskey-up dispatch — F10 MiniDash cycle, PrintScreen screenshot flag, other keys exit 2; param_1≠0 + Escape exits 1. Timer/joystick-button exit 2 (resume).
- Call sites: A (param 1) → return 1 → quit message + outcome 5; B/C (param 0) plain dismiss variants.
- `0x00443120` = MessageBoxA pause (with menu-redraw guards `0x41f490/0x41f4c0`).
- Native gap identified (not ported): race loop handles P/ESC/pause but NOT F10/PrintScreen; the Frontend helpers exist but are unreachable in-race → plan 18 Task 6/7 scope.
- Still open: Escape race-entry point (which race-loop site invokes the confirm pump).

## Doc edits

- `docs/REVERSE_ENGINEERING.md`: pause-pump bullet (options section tail).
- This handoff. No plan checkbox (Task 1 Steps remain owner scope), no source/test touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- Staged-fileset hygiene: verify `git diff --cached --name-only` == my 2 pathspecs before committing.
- Integrator: fold into plan 18 Task 1 Steps 3–4 commit.

## Live processes / resources

No live REA/Ghidra/Wine processes. No build locks. No scratch files.
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
