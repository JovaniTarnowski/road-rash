# separation-rea-20261012

Task: REA spot-check of separation + scheduler (`0x00436a40`, `0x004373f0`, `0x00437e60`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `675473b` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, Contact section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/separation-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-436a40-sep.md` (+ `.log`) — success, 7742 lines
- `analysis/oracle/rea-4373f0-cull.md` (+ `.log`) — success, 2272 lines
- `analysis/oracle/rea-437e60-sched.md` (+ `.log`) — success, 3544 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00436a40` | 12102 | 12103 | TRUE |
| `0x004373f0` | 3196 | 3197 | TRUE |
| `0x00437e60` | 4565 | 4566 | TRUE |

`0x00436a40` is the largest single-function confirmation to date. The contact pipeline (`0x004368f0` → `0x00436a40` → `+0x1a4` handlers) now has exact original text at every level. No RNG in separation/cull/dispatch (impulses are deterministic physics).
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Contact via gap-PUT (no CUT used — lesson from last turn applied). No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any separation re-port (already ported in `collide`/`contact`); proxy capture fix; no captures run.
- Integrator: link from plan 10/16 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
