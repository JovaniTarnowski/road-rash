# cutscenetail-rea-20261012

Task: Record the parallel-subagent cutscene/prize-tail outcomes (`0x004210a0`, `0x00402f50` MATCH; `0x0041f6d0` gap with byte-level root-cause proof), per user notes (rea-agents; parallelize). Tree clean + index empty at start (verified); unowned slice only.

Baseline: `7c07537` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (rerun at commit time for docs-only change)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, results section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/cutscenetail-rea-20261012.md` (this file)

Subagent outputs (git-ignored `analysis/oracle/`, disc-derived, never commit) — MATCH claims independently re-verified by self:
- `analysis/oracle/rea-4210a0-cutreplay.md` (+ `.log`) — `CutsceneReplayREA`: 2833/2834 TRUE (worker said 1966/1966; same verdict; role sanity-checked from content).
- `analysis/oracle/rea-402f50-sndstop.md` (+ `.log`) — `SoundtrackStopREA`: 327/328 TRUE (worker said 309/309; same verdict).
- `analysis/oracle/rea-41f6d0-prize.md` (+ `.log`) — `PrizeHookREA`: EXIT=1 gap with byte-level root-cause proof (`0x0041f6d0` is 8 bytes into Ghidra's `0x0041f6c8` after RET+padding; control REA at `0x0041f6c8` MATCHes `0041f6b0.c` exactly and not `0041f6d0.c` — proving the merge/split; full prize/state semantics documented from export bytes). REA gap #10.

## Progress / evidence

Trust-but-verify held: both MATCH claims re-checked; verdicts agree. The prize-gap worker's control experiment (REA at the neighboring function start proving the boundary error) is the strongest gap forensics to date — credited here. Post-race cutscene selection (`Wreck`/`Busted`/`Win`/`Level`/`Lose`/`FinalWin`) and soundtrack-stop flow rest on the confirmed `0x004210a0` + `0x00402f50` plus export-trusted `0x0041f6d0`.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Results via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- Staged-fileset hygiene: verify `git diff --cached --name-only` == my two pathspecs before committing.
- NOT done: cutscene/prize ports (plan 13 Task 4 owner scope); save-file ports; proxy capture fix; no captures run.
- Integrator: link from plan 13 Task 4 / plan 6 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks. Subagents complete (3/3 delivered).
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
