# damagesplits-rea-20261012

Task: REA spot-check of the damage splits (`0x004050c0`, `0x004051e0`) and side search (`0x00449da0`), per user note to use `npx rea-agents`. Empties the Task-6 trace backlog.

Baseline: `aa11fdc` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, Combat section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/damagesplits-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4050c0-stam.md` (+ `.log`) — success, 1604 lines
- `analysis/oracle/rea-4051e0-bike.md` (+ `.log`) — success, 1334 lines
- `analysis/oracle/rea-449da0-side.md` (+ `.log`) — success, 1150 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004050c0` | 1425 | 1426 | TRUE |
| `0x004051e0` | 992 | 993 | TRUE |
| `0x00449da0` | 703 | 704 | TRUE |

Semantics for the Task-6 porter: stamina split (max floor max/2, current floor 0, HUD hook), bike-condition split (floor 0), side-alternating lane search. No RNG draws in any of the three. The Task-6 trace backlog named in the sibling findings is now EMPTY — every function has exact original text confirmed by two independent Ghidra pipelines.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` Combat section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual ports (sibling `native-port-cc` scope); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 next index refresh and mark the trace prerequisite complete; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
