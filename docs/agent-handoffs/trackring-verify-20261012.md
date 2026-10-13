# trackring-verify-20261012

Task: Verify the merged `cc/plan10-task5` TrackRing work (`cbd1b54`) by REA spot-checking its two RNG-core functions, per user note to use `npx rea-agents`. Read-only review of merged code; no edits to it.

Baseline: `833308b` (own prior commit; `cbd1b54` merge sits below it).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, ring section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/trackring-verify-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-454c80-slicebuild.md` (+ `.log`) — success, 5622 lines
- `analysis/oracle/rea-453030-walkstep.md` (+ `.log`) — success, 9944 lines

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), both exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00454c80` | 9674 | 9675 | TRUE |
| `0x00453030` | 13951 | 13952 | TRUE |

Merge review (read-only): `src/track_ring.hpp/.cpp` (219+90 lines), `tests/track_ring_test.cpp` + `tests/ring_probe.cpp`, harness changes in `tests/oracle_sim_test.cpp`, `tools/sim_known_divergences.json`. All ~20 cited addresses have project exports (verified by listing). The handoff's open question (`0x0044b250` ring-before-`0x00405600` ordering vs 11 post-ring draws) is faithfully recorded as open — no silent closure. Behavioral evidence (K = 2363, anchor closure) belongs to the merge's own tests, not re-verified here.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Ring-cursors via gap-PUT, explicitly scoping fidelity-vs-counts. No source, test, plan, or other doc file touched — merged files left exactly as the integrator left them.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: behavioral re-verification of the merge (its own tests own that); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Merged files (`src/track_ring.*`, `tests/ring_probe.cpp`, harness) reviewed read-only, never edited. Sibling uncommitted changes untouched. `oracle/`, prefix/install untouched.
