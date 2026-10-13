# objectiter-rea-20261012

Task: REA spot-check of object iterators (`0x00451e20`, `0x00451cb0`, `0x00452230`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only. Completes the TrackRing draw-site confirmations (terrain last turn, objects this turn).

Baseline: `b57e1df` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, census section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/objectiter-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-451e20-borstep.md` (+ `.log`) — success, 1308 lines
- `analysis/oracle/rea-451cb0-borslots.md` (+ `.log`) — success, 1894 lines
- `analysis/oracle/rea-452230-dzhridx.md` (+ `.log`) — success, 1112 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00451e20` | 1217 | 1218 | TRUE |
| `0x00451cb0` | 2385 | 2386 | TRUE |
| `0x00452230` | 869 | 870 | TRUE |

With the terrain trio, every RNG draw site the merged `TrackRing` claims is now double-confirmed (exports + independent REA). Behavioral counts stay with the merge's own tests. Remaining unconfirmed in the merge's list: `0x00453e60` jitter/sort, `0x00451ee0`/`0x00451fc0` DZHR sort, walker enter `0x00452f50`/`0x00452cb0`, LIHR init `0x00450fc0`, walk-to-camera `0x00453ae0`, section change `0x00452cb0` — all have exports; next turns.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under actor-census via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: jitter/sort/enter/init draw sites (same pattern); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
