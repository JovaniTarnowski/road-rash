# hudtarget-rea-20261012

Task: REA spot-check of HUD target path (`0x00438550`, `0x00446b80`, `0x00446a40`), per user note to use `npx rea-agents`. Sibling sessions active in tree (including the freshly merged `cc/plan10-task5` track-ring work); unowned slice only.

Baseline: `d5ffeac` (own prior commit; `cbd1b54` merge sits below it).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, dashboard section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/hudtarget-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-438550-nearopp.md` (+ `.log`) — success, 1194 lines
- `analysis/oracle/rea-446b80-tgtname.md` (+ `.log`) — success, 944 lines
- `analysis/oracle/rea-446a40-distread.md` (+ `.log`) — success, 806 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00438550` | 784 | 785 | TRUE |
| `0x00446b80` | 506 | 507 | TRUE |
| `0x00446a40` | 249 | 250 | TRUE |

(Todo named `0x00446c30` meter-draw; that 17KB function is the draw path already covered by the playtest-visual-fixes session's disassembly work — the traced scan/copy/readout triple is the sim-side equivalent. Recorded here instead of re-tracing draw code.)
No new semantics beyond the section. No RNG in scan/copy/readout.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Dashboard-opponent-target via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: `napa-dash` pixel acceptance (11a Task 2 Step 5, needs post-dashboard capture point); proxy capture fix; no captures run.
- Integrator: link from plan 11a/16 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched (merge `cbd1b54` reviewed read-only). `oracle/`, prefix/install untouched.
