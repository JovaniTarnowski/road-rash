# proximitybox-verify-20261012

Task: Verify the remount proximity-box region (`40a785–40a7c9`) by disassembly for plan-24 Task-3. Sibling sessions active in tree; unowned slice only.

Baseline: `3b08cb2` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, run-back section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/proximitybox-verify-20261012.md` (this file)

No new `analysis/oracle/` files: the evidence is `analysis/original-disassembly.txt` (`40a785–40a7d3`, already in tree). No REA call needed.

## Progress / evidence

Region walk: `Δpos`/`Δlat−0x1770` computed → `|Δpos| ≤ |Δlat|/128` (sign-fixed shift) → `|Δlat−0x1770| ≤ 0x1770` → `+0x2f0` test → remount path. This explains the `recover_crash_actor` box constants AND the `0x00449c20` sign quirk the port preserves (subtraction precedes the absolute value — the quirk is faithful, not a bug). Closes the last unverified constant in the already-ported box.
Per AGENTS.md, disassembly is authoritative for control flow.

Shared-doc edit: one sub-bullet under run-back-input via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the `step_crash` port itself (Task-3 owner scope); Task-3 capture trilogy; proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 3 next index refresh.

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
