# replan-verify-20261012

Task: Verify the sibling AI state-machine merge (`36c03dc`) by REA spot-checking its two headline functions (`0x004082f0`, `0x00407990`), per user note to use `npx rea-agents`. Read-only review of merged code; no edits to it.

Baseline: `1b8bc12` (own prior commit; `36c03dc` sits below it).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, AI-brain section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/replan-verify-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4082f0-replan.md` (+ `.log`) — success, 7024 lines
- `analysis/oracle/rea-407990-braininit.md` (+ `.log`) — success, 1442 lines

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), both exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004082f0` | 10023 | 10024 | TRUE |
| `0x00407990` | 1171 | 1172 | TRUE |

Merge review (read-only): `src/race.cpp` (+816/−422), `src/race.hpp`, `src/race_pedestrians.cpp`, harness + `tools/sim_known_divergences.json`. Ports replan/overtake/transitions, dispatch `0x00465980`, opponent-slot brains, native countdown, `0x004368f0` walk with `0x004373f0` gate, `0x00436a40` separation — all of which this session previously confirmed at export level. The tick-207 parity / tick-208 steer-divergence claims belong to the merge's own replay test, not re-verified here. Open question from the message (countdown `dt` semantics) is faithfully left open.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under AI-brain-finding via gap-PUT, explicitly scoping fidelity-vs-parity. No source, test, plan, or other doc file touched — merged files left exactly as committed.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: behavioral re-verification of the merge (its own tests own that); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5/6 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Merged files reviewed read-only, never edited. Sibling uncommitted changes untouched. `oracle/`, prefix/install untouched.
