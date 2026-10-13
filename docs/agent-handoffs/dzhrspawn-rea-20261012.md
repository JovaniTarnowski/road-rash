# dzhrspawn-rea-20261012

Task: REA spot-check of the DZHR spawn path (`0x004520d0`, `0x004409e0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `dd27218` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, pedestrians section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/dzhrspawn-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4520d0-dzhr.md` (+ `.log`) — success, 1632 lines
- `analysis/oracle/rea-4409e0-setup.md` (+ `.log`) — success, 2022 lines

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), both exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004520d0` | 2219 | 2220 | TRUE |
| `0x004409e0` | 1828 | 1829 | TRUE |

No new semantics: the section already documents the corrected spawn formula, and REA confirms the exports it was traced from. Replay-critical detail restated for the porter: exactly one `rand()` per actor record in the iterator, one trigger draw per ped, two draws per cop — in record order. This closes the trace half of plan-24 Task-4 Step-5 acceptance; goldens + pixel frame remain open (need working captures).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` pedestrians section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: Step-5 acceptance (plan 24 Task 4 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 4 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
