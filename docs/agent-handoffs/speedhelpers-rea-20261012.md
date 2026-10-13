# speedhelpers-rea-20261012

Task: REA spot-check of AI speed/obstacle helpers (`0x00409020`, `0x00449a80`, `0x0044a070`, `0x00449b40`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `ec29c8f` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, AI-driving section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/speedhelpers-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-409020-tspeed.md` (+ `.log`) — success, 928 lines
- `analysis/oracle/rea-449a80-fspeed.md` (+ `.log`) — success, 1262 lines
- `analysis/oracle/rea-44a070-obst.md` (+ `.log`) — success, 1032 lines
- `analysis/oracle/rea-449b40-bend.md` (+ `.log`) — success, 1108 lines

## Progress / evidence

Four pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00409020` | 512 | 513 | TRUE |
| `0x00449a80` | 882 | 883 | TRUE |
| `0x0044a070` | 584 | 585 | TRUE |
| `0x00449b40` | 689 | 690 | TRUE |

These pin the replay speed numbers: curve-tapered target speed, the shared follow-speed choke, lane-obstacle gating, bend averaging. No RNG in any helper. No new behavior beyond the section; the porter now has exact text for every speed term in the AI/car follow path.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` AI-driving section. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual ports (sibling `native-port-cc` scope); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 6 / plan 16 next index refresh; keep the four `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
