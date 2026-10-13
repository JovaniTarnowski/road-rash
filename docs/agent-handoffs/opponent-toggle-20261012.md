# opponent-toggle-20261012

Task: Fix the native OpponentList deviation found in `menu-keys-20261012` (`/` toggles via `sete`; native was ON-only). Tree clean + index empty at start (verified); unowned slice only.

Baseline: `845c4f2` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build`

## Fix

- `src/frontend.cpp`: `enable_opponent_list` (set-true) → `toggle_opponent_list` (negate, return new value). Rename (only callers: test + decl).
- `src/frontend.hpp` + `src/settings.hpp`: comments corrected (`/` toggles, `0x423777`).
- `tests/frontend_test.cpp`: the old "ON-only" check encoded the misread — replaced with on/off toggle checks (behavior correction with disassembly evidence, not re-pinning).

## Checks

- `frontend-test`: 0 failures (incl. 2 new toggle checks); `ctest -R "frontend|settings"`: 2/2 pass; full `cmake --build build --parallel` links.
- Full 71-gate re-run left to the next gate turn (change is 3-line + test).

## Remaining work

- Staged-fileset hygiene: verify `git diff --cached --name-only` == 5 pathspecs (frontend.hpp/cpp, settings.hpp, frontend_test.cpp, handoff) before committing.
- The `/`-key→handler wiring itself (menu key dispatch to `toggle_opponent_list`) belongs to plan 18 Tasks 6–7 with the other in-race/menu key ports.
- Integrator: link from plan 18 Task 6/7 scope.

## Live processes / resources

No live REA/Ghidra/Wine processes. No build locks. No scratch files.
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
