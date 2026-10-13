# aifinal-leaves-rea-20261012

Task: REA spot-check of the final AI/combat leaves (`0x004052c0`, `0x00449c80`, `0x0040ac70`), per user note to use `npx rea-agents`. Closes the plan-10 Task-6 trace.

Baseline: `b611e87` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, Combat section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/aifinal-leaves-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4052c0-knockoff.md` (+ `.log`) — success, 1834 lines
- `analysis/oracle/rea-449c80-contactscan.md` (+ `.log`) — success, 908 lines
- `analysis/oracle/rea-40ac70-animset.md` (+ `.log`) — success, 896 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004052c0` | 1824 | 1825 | TRUE |
| `0x00449c80` | 450 | 451 | TRUE |
| `0x0040ac70` | 240 | 241 | TRUE |

Semantics for the Task-6 porter: contact scan picks nearer side; knock-off scales by reason, decides crash via stamina/bike splits, applies steer impulse; anim set tries code then `0x00464430` fallback. No RNG draws in any leaf. The Task-6 trace is COMPLETE at every level (placement → overtake/scan → step/reset → state arms → tail → predicates → callees → leaves); the porter owns `0x00449ce0`, `0x004050c0`/`0x004051e0`, and wired-up `0x0043ae10`.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` Combat section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual ports (sibling `native-port-cc` scope); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `docs/WEB.md`, `cmake/`, status docs — none touched. `oracle/`, prefix/install untouched.
