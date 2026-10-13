# startchain-rea-20261012

Task: REA spot-check of race-start chain (`0x0044db80`, `0x0044dd00`, `0x004552e0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `9ecbf73` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, intersections section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/startchain-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-44db80-densinit.md` (+ `.log`) — success, 962 lines
- `analysis/oracle/rea-44dd00-density.md` (+ `.log`) — success, 1024 lines
- `analysis/oracle/rea-4552e0-ringfill.md` (+ `.log`) — success, 4040 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0044db80` | 311 | 312 | TRUE |
| `0x0044dd00` | 567 | 568 | TRUE |
| `0x004552e0` | 2664 | 2665 | TRUE |

Key scoping fact: no RNG in any of the three — the 11,461-call pre-green setup stream comes from `0x00405600` rider building. This supports (not replaces) the single-anchor-at-green acceptance rule.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Race-start-order via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: `0x00405600` rider-building RNG audit (the actual setup-stream source); proxy capture fix; no captures run.
- Integrator: link from plan 10/17 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
