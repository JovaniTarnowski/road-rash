# aistate5-predicates-rea-20261012

Task: REA spot-check of AI state 5 (`0x0040a4f0`) and the lane/contact predicates (`0x004092e0`, `0x0040a4a0`, `0x00409510`), per user note to use `npx rea-agents`.

Baseline: `36f6920` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, AI-driving section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/aistate5-predicates-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-40a4f0-state5.md` (+ `.log`) — success, 1024 lines
- `analysis/oracle/rea-4092e0-selflane.md` (+ `.log`) — success, 1530 lines
- `analysis/oracle/rea-40a4a0-tgtlane.md` (+ `.log`) — success, 924 lines
- `analysis/oracle/rea-409510-contact.md` (+ `.log`) — success, 984 lines

## Progress / evidence

Four pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0040a4f0` | 639 | 640 | TRUE |
| `0x004092e0` | 1384 | 1385 | TRUE |
| `0x0040a4a0` | 481 | 482 | TRUE |
| `0x00409510` | 537 | 538 | TRUE |

Semantics for the Task-6 porter: state 5 = retarget + lateral snap/bail; self-lane gate has a level-scaled `rand&0xff` roll (ONE draw — replay must preserve); target-lane is a pure geometric test; contact test is a 4-table animation-code match. Full AI dispatch now has exact original text except callees `0x004090a0`/`0x004091f0`/`0x004096f0`/`0x004095d0`/`0x004092b0`.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` AI-driving section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual state-5/predicate ports (sibling `native-port-cc` scope); remaining callees trace; proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 next index refresh; keep the four `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `docs/WEB.md`, `cmake/`, status docs — none touched. `oracle/`, prefix/install untouched.
