# pedchain-rea-20261012

Task: REA spot-check of the pedestrian chain (`0x00440dd0`, `0x00440990`, `0x00440680`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `9b21c92` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, pedestrians section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/pedchain-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-440dd0-pedthink.md` (+ `.log`) — success, 5712 lines
- `analysis/oracle/rea-440990-pedmove.md` (+ `.log`) — success, 862 lines
- `analysis/oracle/rea-440680-peddraw.md` (+ `.log`) — success, 1104 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00440dd0` | 7173 | 7174 | TRUE |
| `0x00440990` | 384 | 385 | TRUE |
| `0x00440680` | 831 | 832 | TRUE |

No new semantics: the section already documents the corrected think/move/draw rules, and REA confirms the exports the port was built from. This closes the trace half of plan-24 Task-4 Step-5 acceptance; the `city-pedestrians`/`city-pedestrian-hit` goldens + pixel frame remain open (need working captures).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` pedestrians section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: Step-5 acceptance (plan 24 Task 4 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 4 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
