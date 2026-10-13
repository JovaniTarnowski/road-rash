# controls-rea-20261012

Task: REA spot-check of controls chain (`0x0044f800`, `0x0040acd0`, `0x0040af50`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `950ac0e` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, physics section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/controls-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-44f800-controls.md` (+ `.log`) — success, 1118 lines
- `analysis/oracle/rea-40acd0-drive.md` (+ `.log`) — success, 876 lines
- `analysis/oracle/rea-40af50-steer.md` (+ `.log`) — success, 1930 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0044f800` | 615 | 616 | TRUE |
| `0x0040acd0` | 308 | 309 | TRUE |
| `0x0040af50` | 2375 | 2376 | TRUE |

No RNG in the poll/dispatch/steer path (joystick/keyboard only). Relevant to plan 18 (Configure Input rebinding maps onto the `0x00476c00` table + `DAT_0049e65c` count) and to replay (`RR_ORACLE_INPUT` words feed `DAT_004b7ed0` directly, bypassing this poll).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` physics section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any controls re-port (already ported); remaining untraced: `0x0044fa80` keyboard map, `0x0040adb0`/`0x0040aea0` ramps, `0x0040ad30` steer-target; proxy capture fix; no captures run.
- Integrator: link from plan 10/18 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
