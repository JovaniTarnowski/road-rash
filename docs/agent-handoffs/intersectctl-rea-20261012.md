# intersectctl-rea-20261012

Task: REA spot-check of the intersection controller chain (`0x0041eca0`, `0x0041ef60`, `0x0041f040`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `923992c` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, intersections section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/intersectctl-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-41eca0-ctlplace.md` (+ `.log`) — success, 2776 lines
- `analysis/oracle/rea-41ef60-signal.md` (+ `.log`) — success, 1278 lines
- `analysis/oracle/rea-41f040-think.md` (+ `.log`) — success, 1872 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0041eca0` | 2945 | 2946 | TRUE |
| `0x0041ef60` | 910 | 911 | TRUE |
| `0x0041f040` | 2022 | 2023 | TRUE |

No new semantics: the section already documents the v2-corrected placement/signal/think rules, and REA confirms the exports those rules were traced from. This closes the trace half of plan-24 Task-1 Step-5 acceptance; the `city-intersection` capture + pixel frame remain open (need working captures).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` intersections section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: Step-5 acceptance (plan 24 Task 1 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 1 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
