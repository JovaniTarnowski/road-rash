# outcomestep-rea-20261012

Task: REA spot-check of race outcome + step driver (`0x00416010`, `0x0044b3b0`, `0x0044b250`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `0a4f4da` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, race-end section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/outcomestep-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-416010-outcome.md` (+ `.log`) — success, 1532 lines
- `analysis/oracle/rea-44b3b0-step.md` (+ `.log`) — success, 2216 lines
- `analysis/oracle/rea-44b250-start.md` (+ `.log`) — success, 1304 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00416010` | 1272 | 1273 | TRUE |
| `0x0044b3b0` | 1844 | 1845 | TRUE |
| `0x0044b250` | 511 | 512 | TRUE |

The step-driver confirmation pins the exact tick order the proxy hook and native `Race::step` must both follow (timers → rider pass → scheduler → camera → ring → traffic). No RNG in outcome/start; traffic draws inside `0x0044dd90` keep their fixed tick position.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Race-end via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any step-driver re-port (already ported); proxy capture fix; no captures run.
- Integrator: link from plan 10/15 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
