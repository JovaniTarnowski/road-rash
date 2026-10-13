# spatialhelpers-rea-20261012

Task: REA spot-check of spatial helpers (`0x00435840`, `0x00434db0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `e3e3305` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, engines section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/spatialhelpers-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-435840-spatial.md` (+ `.log`) — success, 1746 lines
- `analysis/oracle/rea-434db0-engsel.md` (+ `.log`) — success, 1906 lines

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), both exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00435840` | 1805 | 1806 | TRUE |
| `0x00434db0` | 2043 | 2044 | TRUE |

The siren/engine distance/volume/Doppler/pan formulas are now double-confirmed at source level — the plan-13 Task-1 porter has exact text for the full spatial path (range gate → distance → volume → Doppler → pan → buffer route). No RNG in either helper.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Engines via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: siren/engine ports (plan 13 Task 1 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 13 Task 1 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
