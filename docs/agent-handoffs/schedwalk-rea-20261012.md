# schedwalk-rea-20261012

Task: REA spot-check of the scheduler walk leaves (`0x00449ce0`, `0x00449c20`, `0x00449e60`), per user note to use `npx rea-agents`. Sibling sessions still active in tree (src/tools/docs mtimes 16:48–17:24); unowned slice only.

Baseline: `ef48093` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, Combat section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/schedwalk-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-449ce0-walk.md` (+ `.log`) — success, 1388 lines
- `analysis/oracle/rea-449c20-dist.md` (+ `.log`) — success, 736 lines
- `analysis/oracle/rea-449e60-next.md` (+ `.log`) — success, 788 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00449ce0` | 1088 | 1089 | TRUE |
| `0x00449c20` | 104 | 105 | TRUE |
| `0x00449e60` | 160 | 161 | TRUE |

Semantics for the Task-6 porter: bidirectional ring walk with mask + road-half filters; absolute distance helper; side-bit dispatcher. Remaining porter-owned: `0x00449da0`, `0x004050c0`/`0x004051e0`.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` Combat section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual ports (sibling `native-port-cc` scope); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
