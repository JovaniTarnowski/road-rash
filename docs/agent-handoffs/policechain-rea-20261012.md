# policechain-rea-20261012

Task: REA spot-check of the plan-11 Task-6 police chain (`0x00406b80`, `0x00407860`, `0x00407f30`), per user note to use `npx rea-agents`.

Baseline: `cbf1ba8` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, police section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/policechain-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-406b80-copspawn.md` (+ `.log`) — success, 3418 lines
- `analysis/oracle/rea-407860-copfind.md` (+ `.log`) — success, 1050 lines
- `analysis/oracle/rea-407f30-arrest.md` (+ `.log`) — success, 4368 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00406b80` | 3676 | 3677 | TRUE |
| `0x00407860` | 614 | 615 | TRUE |
| `0x00407f30` | 5040 | 5041 | TRUE |

This extends the older single-function `0x00406b80` REA note already in the doc to the full spawn→find→arrest chain. Port status unchanged: arrest decisions + cop setup verified offline, live spawning/chase/sirens + `napa-cops`/`city-cops` goldens open (need working captures).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` police section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual live-spawn port (plan 11 Task 6 / plan 24 Task 2 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 11 Task 6 / plan 24 Task 2 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `docs/WEB.md`, `cmake/`, status docs — none touched. `oracle/`, prefix/install untouched.
