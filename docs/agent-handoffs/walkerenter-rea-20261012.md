# walkerenter-rea-20261012

Task: REA spot-check of walker enter/init (`0x00452f50`, `0x00452cb0`, `0x00450fc0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only. Near-completes the merge's draw-order confirmations.

Baseline: `7823b4d` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, terrain section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/walkerenter-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-452f50-walkroot.md` (+ `.log`) — success, 1034 lines
- `analysis/oracle/rea-452cb0-secenter.md` (+ `.log`) — success, 1278 lines
- `analysis/oracle/rea-450fc0-lihrinit.md` (+ `.log`) — success, 1346 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00452f50` | 619 | 620 | TRUE |
| `0x00452cb0` | 966 | 967 | TRUE |
| `0x00450fc0` | 1390 | 1391 | TRUE |

The 16-draw LIHR init cost is confirmed at the exact draw sites (2 per point × 4 points × 2 sides per walker, × 2 walkers = 32 setup draws). Only `0x00453ae0` walk-to-camera remains unconfirmed in the merge's list.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Terrain-iterator-recovery via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: `0x00453ae0` walk-to-camera (last item, next turn); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
