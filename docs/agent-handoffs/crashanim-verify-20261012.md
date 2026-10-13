# crashanim-verify-20261012

Task: Verify crash-anim queue/loop (`0x0040aca0`, `0x0043ae60`), per user note to use `npx rea-agents`. (Todo named crash bike/rider anim functions; those are native ports with no 1:1 original — traced the original queue/loop primitives they rest on instead; recorded substitution.) Read-only review; no edits to ported code.

Baseline: `0b13ea3` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, animation section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/crashanim-verify-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-40aca0-animqueue.md` (+ `.log`) — success, 868 lines
- `analysis/oracle/rea-43ae60-animloop.md` (+ `.log`) — success, 920 lines
- (`0x0040ac70` re-fetched identical; duplicate removed, original `rea-40ac70-animset.md` from the earlier turn kept.)

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), both exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0040aca0` | 252 | 253 | TRUE |
| `0x0043ae60` | 285 | 286 | TRUE |

With advance/set/queue/loop all confirmed, the full anim primitive set the crash ports use has exact original text.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Animation via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: crash-anim behavioral parity (plan 11 Task 5 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 11 Task 5 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
