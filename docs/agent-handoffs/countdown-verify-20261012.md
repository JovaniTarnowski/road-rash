# countdown-verify-20261012

Task: Close the sibling AI merge's open countdown question (`36c03dc`: `dt 2 at tick 1`) at trace level; flag the hazard-fraction observation to the right owner. Read-only review; no edits to merged code.

Baseline: `cad60a9` (own prior commit; `36c03dc` sits below it).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, AI-brain section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/countdown-verify-20261012.md` (this file)

No new `analysis/oracle/` files: the evidence is export line `00438930.c:46` (`piVar1[0x18] = 0`) plus the merge's own cited implementation (`src/race.cpp:2134,2198`), both read directly. No REA call needed (no new function).

## Progress / evidence

- Countdown: `0x00438930` zeroes `+0x18` at build; first scheduled tick computes dt 2 (creation tick − 1), then 1. The merge's `countdown_step` (`ticks_ == 1 ? 2 : 1`) + `step` (reset to 1) implements exactly this with cited comments. Open question from the merge message → closed at trace level. Behavioral parity stays with the merge's replay test.
- Hazard fraction: native `collide_hazards` (`src/race.cpp:621-634`) tests sample-granularity `hazards_[]` spans; the export `0x004373f0` sweeps rider box at 0x80-fraction precision against slot fractions. Possible precision gap — but `collide_hazards` predates the merge (plan 16 Task 2), so flagged to that owner, NOT filed against the merge. No behavior change proposed.
- (Todo named REA spot-checks; nothing new to REA — all cited functions already confirmed. Verified by reading instead; substitution noted.)

Shared-doc edit: one sub-bullet under AI-brain-finding via gap-PUT. No source, test, plan, or other doc file touched — merged files left exactly as committed.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: behavioral re-verification of the merge; proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5/6 next index refresh; surface the hazard-fraction note to the plan-16 Task-2 owner.

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Merged files reviewed read-only, never edited. Sibling uncommitted changes untouched. `oracle/`, prefix/install untouched.
