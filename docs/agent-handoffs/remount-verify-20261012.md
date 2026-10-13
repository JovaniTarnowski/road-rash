# remount-verify-20261012

Task: Verify remount/reset (`0x00404560`, `0x004067d0`, `0x0040a9d0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `e3a2d50` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, crash section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/remount-verify-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-404560-remount.md` (+ `.log`) — success, 2172 lines
- `analysis/oracle/rea-4067d0-copyback.md` (+ `.log`) — success, 1382 lines
- `analysis/oracle/rea-40a9d0-walkspeed.md` (+ `.log`) — success, 682 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00404560` | 1532 | 1533 | TRUE |
| `0x004067d0` | 1535 | 1536 | TRUE |
| `0x0040a9d0` | 20 | 21 | TRUE |

The `remount_crash_actor` port's three sources now all have second-pipeline confirmation, including the already-documented `+0x104` skip and the 1200 constant.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under remount paragraph via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: live remount integration + `napa-crash-*` goldens (plan 11 Task 4 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 11 Task 4 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
