# draworder-rea-20261012

Task: REA re-confirmation of draw-order roots (`0x00443af0`, `0x00443cd0`, `0x00438610`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only. (Todo label said `0x00443ae90/0x0043af60/0x0044ba40`, but those lack project exports; the start-grid session's documented triple is the traceable equivalent — recorded here instead of inventing scope.)

Baseline: `7851abc` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, nodes section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/draworder-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-443af0-order.md` (+ `.log`) — success, 852 lines
- `analysis/oracle/rea-443cd0-worldobj.md` (+ `.log`) — success, 1082 lines
- `analysis/oracle/rea-438610-halfdispatch.md` (+ `.log`) — success, 1798 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00443af0` | 376 | 377 | TRUE |
| `0x00443cd0` | 533 | 534 | TRUE |
| `0x00438610` | 1587 | 1588 | TRUE |

This is the SECOND independent REA confirmation of the same triple (the first is documented two lines above in the same section from the start-grid session). No new semantics; strengthens the W/O-order evidence both sessions cite.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: separate world/object passes in native emission (plan 11a Task 3 owner scope); constructed-list/full-frame parity; no captures run.
- Integrator: link from plan 11a Task 3 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
