# actordispatch-rea-20261012

Task: REA spot-check of actor dispatch (`0x0043af60`, `0x0044ba40`) + post-merge suite state, per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `b614d98` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (two full `ctest` runs; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, actor section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/actordispatch-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-443af60-selview.md` (+ `.log`) — success, 2470 lines
- `analysis/oracle/rea-44ba40-billboard.md` (+ `.log`) — success, 3080 lines

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), both exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0043af60` | 3487 | 3488 | TRUE |
| `0x0044ba40` | 3686 | 3687 | TRUE |

Suite (71 tests post-merge): first full run had ONE failure (`oracle-sim`); isolated re-run passed; second full run 71/71 green. Signature is ordering flake, not a code regression (my changes since the last green are docs-only; the merge's own tests pass). Recorded honestly: one flake observed, green on re-run.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under actor-selection via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: `select_actor_view` remaining work (orientation `0x00413ae0`, live triggers, shadow remaps, depth dispatch); proxy capture fix; no captures run.
- Integrator: link from plan 11a Task 3 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
