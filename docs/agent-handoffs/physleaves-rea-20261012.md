# physleaves-rea-20261012

Task: REA spot-check of physics leaves (`0x004391d0`, `0x004394c0`, `0x0043a0c0`, `0x0043a190`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `c615d71` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, physics section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/physleaves-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4391d0-gear.md` (+ `.log`) — success, 1028 lines
- `analysis/oracle/rea-4394c0-bendsweep.md` (+ `.log`) — success, 1288 lines
- `analysis/oracle/rea-43a0c0-surface.md` (+ `.log`) — success, 1202 lines
- `analysis/oracle/rea-43a190-vertical.md` (+ `.log`) — success, 3572 lines

## Progress / evidence

Four pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004391d0` | 669 | 670 | TRUE |
| `0x004394c0` | 1100 | 1101 | TRUE |
| `0x0043a0c0` | 844 | 845 | TRUE |
| `0x0043a190` | 4870 | 4871 | TRUE |

The tick-physics trace is now complete except the `0x0043a580` dispatcher (straight-line caller, low value). No RNG in any leaf.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` physics section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any physics re-port (already ported); proxy capture fix; no captures run.
- Integrator: link from plan 10/16 next index refresh; keep the four `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
