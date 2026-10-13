# animadv-verify-20261012

Task: Verify the sibling AI merge's animation advance (`0x0043ae90`, `0x0043ae10`; `0x00407a90` attempted), per user note to use `npx rea-agents`. Read-only review; no edits to merged code.

Baseline: `e789063` (own prior commit; `36c03dc` sits below it).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, animation section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/animadv-verify-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-43ae90-animadv.md` (+ `.log`) — success, 1486 lines
- `analysis/oracle/rea-43ae10-animset.md` (+ `.log`) — success, 1030 lines
- `analysis/oracle/rea-407a90-bikeanim.md` (+ `.log`) — REA `invalid_input`: no procedure at `0x00407a90`

## Progress / evidence

Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0043ae90` | 1355 | 1356 | TRUE |
| `0x0043ae10` | 535 | 536 | TRUE |
| `0x00407a90` | — | — | REA gap #8 (valid 1631-char export) |

(Todo named `0x00407f30`-anim; that 5KB arrest dispatch was already confirmed in an earlier turn. Traced the advance/set pair the merge's `advance_ride_animation` actually ports; substitution noted.)
The merge's tick-207 parity stays with its own replay test; this note covers export fidelity only.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Animation via gap-PUT. No source, test, plan, or other doc file touched — merged files left exactly as committed.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: behavioral re-verification of the merge; proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5/6 + plan 11 Task 5 next index refresh; keep the two success `analysis/oracle/rea-*` files local (git-ignored). The 35-line bikeanim error file is also ignored; harmless.

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Merged files reviewed read-only, never edited. Sibling uncommitted changes untouched. `oracle/`, prefix/install untouched.
