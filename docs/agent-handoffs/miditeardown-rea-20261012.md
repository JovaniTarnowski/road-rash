# miditeardown-rea-20261012

Task: Record the parallel-subagent MIDI volume/teardown results (`0x00433cb0`, `0x00433cf0`, `0x00433f70`, `0x00433dd0`) with independent verification, per user notes (rea-agents; parallelize). Tree clean + index empty at start (verified); unowned slice only.

Baseline: `5d1d6f5` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (rerun at commit time for docs-only change)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, music section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/miditeardown-rea-20261012.md` (this file)

Subagent outputs (git-ignored `analysis/oracle/`, disc-derived, never commit) — all created by workers, MATCH claims independently re-verified by self:
- `analysis/oracle/rea-433cb0-midivol.md` (+ `.log`) — `MidiVolumeREA`: 186/187 TRUE.
- `analysis/oracle/rea-433cf0-teardown.md` (+ `.log`) — `MidiTeardownREA`: 384/385 TRUE.
- `analysis/oracle/rea-433f70-hdrclean.md` (+ `.log`) — `MidiHeaderCleanupREA`: 670/671 TRUE.
- `analysis/oracle/rea-433dd0-streamdown.md` (+ `.log`) — `MidiHeaderCleanupREA`: 385/386 TRUE.

## Progress / evidence

Trust-but-verify held: all 4 worker MATCH claims re-checked; verdicts agree (counts differ only by window sizing). The race-MIDI chain is now COMPLETE end-to-end across 10 functions / 3 turns: starter → probe → parser → feed → session → pump → volume/teardown. The plan-13 Task-1 porter has exact source text for the entire chain; only Win32 midiStream/midiOut calls need SDL equivalents.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Music via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- Staged-fileset hygiene: verify `git diff --cached --name-only` == my two pathspecs before committing.
- NOT done: MIDI ports (plan 13 Task 1 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 13 Task 1 next index refresh and mark the MIDI trace complete; keep the four `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks. Subagents complete (3/3 delivered).
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
