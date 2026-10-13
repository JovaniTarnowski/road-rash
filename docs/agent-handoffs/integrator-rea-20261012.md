# integrator-rea-20261012

Task: REA spot-check of the tick-physics integrator (`0x00438c70`, `0x004390c0`, `0x004395e0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `30e6786` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, physics section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/integrator-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-438c70-traction.md` (+ `.log`) — success, 4318 lines
- `analysis/oracle/rea-4390c0-speed.md` (+ `.log`) — success, 1434 lines
- `analysis/oracle/rea-4395e0-move.md` (+ `.log`) — success, 4256 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00438c70` | 5976 | 5977 | TRUE |
| `0x004390c0` | 1488 | 1489 | TRUE |
| `0x004395e0` | 4255 | 4256 | TRUE |

Key replay fact: no RNG anywhere in the tick physics — solo-physics divergences are deterministic given inputs, so the tick-75 lateral gap class of failure is always a harness/setup bug, never randomness. No new semantics beyond the section; the bike port was built from these exports.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` physics section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any physics re-port (none needed — already ported); remaining untraced: `0x004391d0` gear shift, `0x004394c0` bend sweep, `0x0043a190` vertical; proxy capture fix; no captures run.
- Integrator: link from plan 10/16 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
