# aicombat-leaves-rea-20261012

Task: REA spot-check of the AI combat leaves (`0x00409400`, `0x004095a0`, `0x00409ad0`, `0x00409bd0`, `0x00408d50`), per user note to use `npx rea-agents`.

Baseline: `4b961cd` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, Combat section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/aicombat-leaves-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-409400-victim.md` (+ `.log`) — success, 842 lines
- `analysis/oracle/rea-4095a0-reach.md` (+ `.log`) — success, 870 lines
- `analysis/oracle/rea-409ad0-damage.md` (+ `.log`) — success, 1256 lines
- `analysis/oracle/rea-409bd0-knock.md` (+ `.log`) — success, 870 lines
- `analysis/oracle/rea-408d50-rel.md` (+ `.log`) — success, 1150 lines

## Progress / evidence

Five pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00409400` | 347 | 348 | TRUE |
| `0x004095a0` | 342 | 343 | TRUE |
| `0x00409ad0` | 1152 | 1153 | TRUE |
| `0x00409bd0` | 351 | 352 | TRUE |
| `0x00408d50` | 857 | 858 | TRUE |

Semantics for the Task-6 porter: victim pick latches `+0x168` from the contact scan; left/right anim tests split the weapon-code tables; damage = scaled base + weapon sound + `0x004052c0` with nitrus-divisor quirk; relationship write is a 3-lane clamped accumulate keyed by victim char. No RNG draws in any leaf (all randomness lives in the callers `0x00409460`/`0x004096f0`). AI/combat trace now complete except `0x00449c80`, `0x004052c0`, `0x0040ac70`.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` Combat section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual leaf ports (sibling `native-port-cc` scope); remaining leaf trace (`0x00449c80`, `0x004052c0`, `0x0040ac70`); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 next index refresh; keep the five `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `docs/WEB.md`, `cmake/`, status docs — none touched. `oracle/`, prefix/install untouched.
