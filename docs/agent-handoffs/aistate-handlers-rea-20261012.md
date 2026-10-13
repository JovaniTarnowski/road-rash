# aistate-handlers-rea-20261012

Task: REA spot-check of the plan-10 Task-6 AI state handlers (`0x004092d0`, `0x00409460`, `0x00409ca0`, `0x0040a560`), per user note to use `npx rea-agents`.

Baseline: `5a6d1c7` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, AI-driving section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/aistate-handlers-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-409ca0-follow.md` (+ `.log`) — success, 2388 lines
- `analysis/oracle/rea-409460-attack.md` (+ `.log`) — REA `invalid_input`: no procedure at `0x00409460`
- `analysis/oracle/rea-40a560-state6.md` (+ `.log`) — REA `invalid_input`: no procedure at `0x0040a560`

## Progress / evidence

```sh
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00409460 --format md  # EXIT=1, no procedure
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00409ca0 --format md  # EXIT=0
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x0040a560 --format md  # EXIT=1, no procedure
```

Findings (normalized whitespace comparison against `analysis/decompiled/`):
- `0x00409ca0`: 3001 vs 3002 chars, NORMALIZED MATCH TRUE.
- `0x004092d0`: not sent to REA — project export is a 1-instruction thunk (`FUN_00409000(param_1)`); nothing to second-opinion.
- `0x00409460` / `0x0040a560`: absent from REA's Ghidra procedure database, but both are valid code in `analysis/original-disassembly.txt` (`409460: pushl %ebx`, prologue + `call 0x409ca0` verified at lines 10791–10810; `40a560: pushl %esi`). REA silence here is a database gap, not evidence against the project exports. Record the gap in `docs/TOOLING.md` at the next tooling pass.
- Semantics for the Task-6 porter: state-2 thunk → racing; state-3 = follow + contact-gated weapon rolls (TWO `rand()` draws in order: `rand % +0x1d0`, then `rand % +0x1d1`); state-4 = follow with near-field lateral clamp and `0x00449a80` speed; state-6 = hold-or-overtake. All end in `0x00409f70`.
- REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` AI-driving section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual state-handler ports (sibling `native-port-cc` scope); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 next index refresh; keep `analysis/oracle/rea-409ca0-follow.md` local (git-ignored). The two failed-call `.md` files (35 lines of error JSON) are also ignored; harmless.

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/` (race, game_main, sound, mixer, race_music, bike), `tests/`, `docs/WEB.md`, `cmake/emscripten.cmake`, status docs — none touched. `oracle/`, prefix/install untouched.
