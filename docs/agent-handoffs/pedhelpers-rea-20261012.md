# pedhelpers-rea-20261012

Task: REA spot-check of ped helpers (`0x004415b0`, `0x004414a0`, `0x0044a020`, `0x004498c0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `aa39561` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, pedestrians section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/pedhelpers-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4415b0-pedsignal.md` (+ `.log`) — success, 1136 lines
- `analysis/oracle/rea-4414a0-arrival.md` (+ `.log`) — success, 914 lines
- `analysis/oracle/rea-44a020-laneof.md` (+ `.log`) — success, 860 lines
- `analysis/oracle/rea-4498c0-accel.md` (+ `.log`) — success, 904 lines

## Progress / evidence

Four pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004415b0` | 895 | 896 | TRUE |
| `0x004414a0` | 406 | 407 | TRUE |
| `0x0044a020` | 303 | 304 | TRUE |
| `0x004498c0` | 385 | 386 | TRUE |

No RNG in any helper; no new semantics beyond the section. Goldens + pixel frame remain open (need working captures).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` pedestrians section (stale-tag retry once, resolved via fresh read). No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: Step-5 acceptance (plan 24 Task 4 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 4 next index refresh; keep the four `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
