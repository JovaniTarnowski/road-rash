# crashseq-rea-20261012

Task: REA spot-check of the plan-11 Task-4 crash sequence (`0x00403d60`, `0x0040a600`, `0x0043a610`), per user note to use `npx rea-agents`.

Baseline: `dc15fd3` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, actor section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/crashseq-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-403d60-crashentry.md` (+ `.log`) — success, 5006 lines
- `analysis/oracle/rea-40a600-runback.md` (+ `.log`) — success, 3938 lines
- `analysis/oracle/rea-43a610-wall.md` (+ `.log`) — success, 6298 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00403d60` | 4716 | 4717 | TRUE |
| `0x0040a600` | 4422 | 4423 | TRUE |
| `0x0043a610` | 9756 | 9757 | TRUE |

This supersedes the older `analysis/oracle/rea-crash-entry.json` reference for the crash entry and adds the first REA confirmations of the run-back and wall-collision functions. `0x0043a610` at 9.7KB is the largest single-function REA confirmation to date. Port status unchanged: crash entry/recovery partially ported offline (`recover_crash_actor`, `remount_crash_actor`), wall collision still a stand-in, live tick integration + the four `napa-crash-*` scenario goldens still open (need working captures).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` actor section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual wall-crash/remount ports (plan 11 Task 4 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 11 Task 4 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `docs/WEB.md`, `cmake/`, status docs — none touched. `oracle/`, prefix/install untouched.
