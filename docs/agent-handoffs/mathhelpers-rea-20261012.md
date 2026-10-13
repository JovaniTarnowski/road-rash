# mathhelpers-rea-20261012

Task: REA spot-check of math helpers (`0x0044f5a0`, `0x0044f4a0`, `0x0044a870`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only. (Todo named scene builders `0x0043c170/0x0044bd40/0x0041d260`, but those have no decompile exports — documented from proxy fixtures instead; traced the nearest export-covered equivalents and recorded the gap.)

Baseline: `8f1d1b8` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, physics section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/mathhelpers-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-44f5a0-atan2.md` (+ `.log`) — success, 1224 lines
- `analysis/oracle/rea-44f4a0-sin.md` (+ `.log`) — success, 884 lines
- `analysis/oracle/rea-44a870-mipsize.md` (+ `.log`) — success, 940 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0044f5a0` | 831 | 832 | TRUE |
| `0x0044f4a0` | 398 | 399 | TRUE |
| `0x0044a870` | 355 | 356 | TRUE |

These underpin every steering/yaw/angle computation in the AI, camera and tail code traced earlier. No RNG (pure tables).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Math-helpers via gap-PUT, including the explicit scene-builder export-gap note. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any math re-port (already ported); scene-builder fixture coverage stays proxy-based; no captures run.
- Integrator: link from plan 10/19 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
