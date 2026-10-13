# driveleaves-rea-20261012

Task: REA spot-check of drive leaves (`0x0040adb0`, `0x0040aea0`, `0x0040ad30`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `d16509b` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, physics section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/driveleaves-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-40adb0-throttle.md` (+ `.log`) — success, 1576 lines
- `analysis/oracle/rea-40aea0-brake.md` (+ `.log`) — success, 1276 lines
- `analysis/oracle/rea-40ad30-steertgt.md` (+ `.log`) — success, 1044 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0040adb0` | 1336 | 1337 | TRUE |
| `0x0040aea0` | 1056 | 1057 | TRUE |
| `0x0040ad30` | 494 | 495 | TRUE |

No RNG in any leaf. The controls + drive chain (`0x0044f800` → `0x00434660` → `0x0040acd0` → `0x0040adb0`/`0x0040aea0`, `0x0040ad30` → `0x0040af50`) now has exact original text at every level except the keyboard map `0x0044fa80`.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` physics section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any drive re-port (already ported); `0x0044fa80` keyboard map trace; proxy capture fix; no captures run.
- Integrator: link from plan 10 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
