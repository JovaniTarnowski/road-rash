# walktocam-rea-20261012

Task: REA spot-check of walk-to-camera + section change (`0x00453ae0`, `0x00452e10`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only. Completes the merge's draw-order confirmations.

Baseline: `628c673` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, terrain section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/walktocam-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-453ae0-walktocam.md` (+ `.log`) — success, 992 lines
- `analysis/oracle/rea-452e10-secchange.md` (+ `.log`) — success, 1510 lines

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), both exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00453ae0` | 615 | 616 | TRUE |
| `0x00452e10` | 1526 | 1527 | TRUE |

The merge's draw-order list is now FULLY double-confirmed (13 functions across 6 turns: slice build, walker step, terrain trio, object trio, jitter/sort trio, enter trio, these two). No RNG in either (pure stepping/dispatch; draws live in the callees already confirmed).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Terrain-iterator-recovery via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the two-draw audit follow-up with the merge owner (prior turn); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5 next index refresh and close the draw-order confirmation thread; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
