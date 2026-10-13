# timerpolice-verify-20261012

Task: Verify the 1200-tick timer + police-owner exemption regions by disassembly for plan-24 Task-3. Sibling sessions active in tree; unowned slice only.

Baseline: `1c13eab` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, run-back section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/timerpolice-verify-20261012.md` (this file)

No new `analysis/oracle/` files: the evidence is `analysis/original-disassembly.txt` (`40a649–40a694`, already in tree). No REA call needed.

## Progress / evidence

Region walk: `+0x138[state]` elapsed vs `0x4b0` → over-time cop-owner check (`DAT_004642dc` + `+0x2fc` identity, skip on match) → `+0x2f0` walk-continue → player-bike timer-expiry wreck. Matches the doc paragraph verbatim; the Task-3 porter scope (timer + skip) is fully specified. Per AGENTS.md, disassembly is authoritative for control flow.

Shared-doc edit: one sub-bullet under run-back-input via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the `step_crash` port itself (Task-3 owner scope); Task-3 capture trilogy; proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 3 next index refresh.

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
