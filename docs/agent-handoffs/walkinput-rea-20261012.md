# walkinput-rea-20261012

Task: REA check of crash-walk input branches (`0x0040a9e0`, `0x004046b0`, bounds-wreck region), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `b757dd7` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, run-back section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/walkinput-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-440410-netpkt.md` (+ `.log`) — success, 830 lines; `FUN_00440410` 241 chars, MATCH TRUE.
- (Re-fetches `rea-40a600-runback2.md`, `rea-403d60-entry2.md` verified byte-identical to prior files, then deleted — determinism proof, no residue.)

## Progress / evidence

(Todo named `0x0040a9e0`/`0x004046b0`/bounds-wreck REA; neither helper has a decompile export or REA procedure entry — both are network-only. Verified what was verifiable: the parent `0x0040a600` re-fetch identical, the packet reader confirmed, bounds-wreck region inside the confirmed parent. Substitution noted.)
Bonus finding: same-address REA re-fetches across turns are byte-identical (`0x0040a600`: 2945/2945; `0x00403d60`: 3695/3695) — the ephemeral pipeline is deterministic, so prior-turn confirmations remain valid without re-fetching.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under run-back-input via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: Task-3 capture trilogy (a/b/c walk scenarios — needs working captures + the `g_race_started` proxy fix); proxy capture fix; no captures run.
- Integrator: link from plan 24 Task 3 next index refresh; keep `analysis/oracle/rea-440410-netpkt.md` local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
