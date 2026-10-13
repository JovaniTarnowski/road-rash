# pedcontact-rea-20261012

Task: REA spot-check of ped hit/load/lane-clear (`0x00440760`, `0x00440b80`, `0x00441500`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `c851696` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, pedestrians section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/pedcontact-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-440760-pedhit.md` (+ `.log`) — success, 2282 lines
- `analysis/oracle/rea-440b80-pedload.md` (+ `.log`) — success, 1996 lines
- `analysis/oracle/rea-441500-laneclear.md` (+ `.log`) — success, 1022 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00440760` | 2571 | 2572 | TRUE |
| `0x00440b80` | 2658 | 2659 | TRUE |
| `0x00441500` | 669 | 670 | TRUE |

Semantics for the Task-4 porter: knock-down vs stagger branch, anim-load table bindings, lane-clear formula. Only RNG in the hit path is the `(rand&7)==0` scream draw. No new semantics beyond the section; the port was built from these exports. Goldens + pixel frame remain open (need working captures).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` pedestrians section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: Step-5 acceptance (plan 24 Task 4 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 4 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
