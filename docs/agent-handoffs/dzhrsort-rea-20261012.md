# dzhrsort-rea-20261012

Task: REA spot-check of DZHR jitter/sort (`0x00453e60`, `0x00451ee0`, `0x00451fc0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only. Directly audits the merged `TrackRing::jitter_events` draw order.

Baseline: `5db2832` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, pedestrians section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/dzhrsort-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-453e60-dfs.md` (+ `.log`) — success, 1430 lines
- `analysis/oracle/rea-451ee0-recjit.md` (+ `.log`) — success, 1254 lines
- `analysis/oracle/rea-451fc0-sort.md` (+ `.log`) — success, 1104 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00453e60` | 1356 | 1357 | TRUE |
| `0x00451ee0` | 1140 | 1141 | TRUE |
| `0x00451fc0` | 906 | 907 | TRUE |

Audit note for the merge owner: `0x00451ee0` makes TWO draws per record (offset + level-gate), and the gate CONSUMES failing records — both affect the setup-stream count, not just the offset draw. If the merge's K = 2363 accounting assumed one draw per record here, it needs a second look. Flagged, not fixed (their module, their test).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under per-course-census via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: walker enter/init (`0x00452f50`, `0x00452cb0`, `0x00450fc0`, `0x00453ae0` — last unconfirmed in the merge's list); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5 next index refresh; surface the two-draw audit note to the merge owner; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
