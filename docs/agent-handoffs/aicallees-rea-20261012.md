# aicallees-rea-20261012

Task: REA spot-check of the AI attack/fallback callees (`0x004090a0`, `0x004091f0`, `0x004095d0`, `0x004096f0`, `0x004092b0`), per user note to use `npx rea-agents`.

Baseline: `41b4d5c` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, AI-driving section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/aicallees-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4090a0-nobody.md` (+ `.log`) — success, 1400 lines
- `analysis/oracle/rea-4091f0-lane.md` (+ `.log`) — success, 1198 lines
- `analysis/oracle/rea-4095d0-attackstart.md` (+ `.log`) — success, 1522 lines
- `analysis/oracle/rea-4096f0-hit.md` (+ `.log`) — success, 3864 lines
- `analysis/oracle/rea-4092b0-behind.md` (+ `.log`) — success, 788 lines

## Progress / evidence

Five pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004090a0` | 1417 | 1418 | TRUE |
| `0x004091f0` | 1001 | 1002 | TRUE |
| `0x004095d0` | 1464 | 1465 | TRUE |
| `0x004096f0` | 4451 | 4452 | TRUE |
| `0x004092b0` | 209 | 210 | TRUE |

Semantics for the Task-6 porter: curve-follow fallback, lane-line targets, attack-start animation + relationship pings, contact hit resolver with steal/cooldown/shove RNG rolls in order, behind-test. AI trace now complete except `0x00409400` victim pick, `0x004095a0` weapon reach, `0x00409ad0`/`0x00409bd0` damage, `0x00408d50` relationship write.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` AI-driving section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual callee ports (sibling `native-port-cc` scope); remaining leaf trace (`0x00409400`, `0x004095a0`, `0x00409ad0`/`0x00409bd0`, `0x00408d50`); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 next index refresh; keep the five `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `docs/WEB.md`, `cmake/`, status docs — none touched. `oracle/`, prefix/install untouched.
