# airacing-tail-rea-20261012

Task: REA spot-check of the AI racing baseline (`0x00409000`) and shared steering/speed tail (`0x00409f70`), per user note to use `npx rea-agents`.

Baseline: `6bead2b` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, AI-driving section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/airacing-tail-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-409000-racing.md` (+ `.log`) — success, 778 lines
- `analysis/oracle/rea-409f70-tail.md` (+ `.log`) — success, 1704 lines

## Progress / evidence

```sh
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00409000 --format md  # EXIT=0
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00409f70 --format md  # EXIT=0
```

Findings (normalized whitespace comparison against `analysis/decompiled/`):
- `0x00409000`: 165 vs 166 chars, NORMALIZED MATCH TRUE.
- `0x00409f70`: 1720 vs 1721 chars, NORMALIZED MATCH TRUE.
- Semantics for the Task-6 porter: racing = retarget + `0x004090a0`-when-empty + always-tail; tail = network/combatant gates, atan2 steering via `0x0040af50`, throttle/brake/coast via `0x0040acd0` with half-target coast hysteresis. Every dispatch arm 0/1/2/4/6 and the shared tail now have exact original text; remaining: state 5 (`0x0040a4f0`) and callees `0x004090a0`/`0x004096f0`/`0x004095d0`.
- REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` AI-driving section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual racing/tail ports (sibling `native-port-cc` scope); state 5 + callees trace; proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `docs/WEB.md`, `cmake/`, status docs — none touched. `oracle/`, prefix/install untouched.
