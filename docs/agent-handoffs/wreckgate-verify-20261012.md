# wreckgate-verify-20261012

Task: Verify the bounds-wreck region inside `0x0040a600` by disassembly (no REA needed for a region check), per plan-24 Task-3 trace needs. Sibling sessions active in tree; unowned slice only.

Baseline: `c459db3` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, run-back section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/wreckgate-verify-20261012.md` (this file)

No new `analysis/oracle/` files: the evidence is `analysis/original-disassembly.txt` lines 12289+70–92 (already in tree, committed disassembly). No REA call needed.

## Progress / evidence

Disassembly walk (all addresses `0x0040a6xx`-relative within `0x0040a600`):

| Lines | Instructions | Meaning |
| --- | --- | --- |
| 70–72 | `movb 0x3c, %cl; cmpb $0x1; jne` | slice-type-1 gate |
| 73–79 | bike `+0x28>>8` vs `[+0x14c, +0x158]`, `jg/jl` out | bike bounds check |
| 80–86 | rider `+0x28>>8` via `+0x140`-slice vs same bounds, `jg/jge` out | rider bounds check |
| 87–92 | `mov $0xb4; mov $0x5; call 0x00416010; movl $0, +0x2f0` | wreck outcome + dismount clear |

Matches the doc paragraph instruction-for-instruction. The port's gap ("does not have this yet") is confirmed as genuinely missing — the Task-3 porter knows exactly what to build (two comparisons + one call + one store).
Per AGENTS.md reverse-engineering rule this is disassembly-authoritative evidence, stronger than a decompile read for control flow.

Shared-doc edit: one sub-bullet under run-back-input via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the bounds-wreck port itself (plan 24 Task 3 owner scope: two comparisons + call + store in `step_crash`); Task-3 capture trilogy; proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 3 next index refresh.

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
