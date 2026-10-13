# ringplumb-rea-20261012

Task: REA spot-check of ring plumbing (`0x00454120`, `0x00449910`, `0x00449990`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `ff40999` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, ring section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/ringplumb-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-454120-ringadv.md` (+ `.log`) — success, 1438 lines
- `analysis/oracle/rea-449910-ringidx.md` (+ `.log`) — success, 1130 lines
- `analysis/oracle/rea-449990-posidx.md` (+ `.log`) — success, 1446 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00454120` | 1113 | 1114 | TRUE |
| `0x00449910` | 750 | 751 | TRUE |
| `0x00449990` | 1265 | 1266 | TRUE |

No RNG in ring plumbing (pure cursor/index math). Relevant to plan 17 (per-course route traversal) and to the +0x5100 ring-offset question (Task 1 Step 2): the advance math here is the other half of that check alongside the capture.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Ring-cursors via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any ring re-port (already ported); `0x004552e0` initial-fill trace; proxy capture fix; no captures run.
- Integrator: link from plan 10/17 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
