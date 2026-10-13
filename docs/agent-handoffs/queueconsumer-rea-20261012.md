# queueconsumer-rea-20261012

Task: REA spot-check of event consumer + queue pop (`0x004351f0`, `0x004367f0`), per user note to use `npx rea-agents`. (Todo named impact/grunt dispatch; those codes route through this consumer — traced the dispatcher instead; recorded substitution.) Tree clean + index empty at start (verified); unowned slice only.

Baseline: `7224d0e` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, sound section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/queueconsumer-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4351f0-consumer.md` (+ `.log`) — success, 8066 lines
- `analysis/oracle/rea-4367f0-queue.md` (+ `.log`) — success, 818 lines
- (`rea-436830-soundevt.*` re-fetch deleted: comparison used a truncated window — lengths 708 vs 966 — so no claim made from it; the prior turn's rigorous 965-char confirmation stands untouched.)

## Progress / evidence

Normalized whitespace comparison against `analysis/decompiled/` (full-body windows, verified lengths):

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004351f0` | 5040 | 5041 | TRUE |
| `0x004367f0` | 312 | 313 | TRUE |

Methodology note (self-caught): the first comparison helper used a fixed `segsize` window that can truncate long bodies; the consumer comparison used a 40000 window and exact lengths. Future turns: always print both lengths and distrust any MATCH where they differ by more than 1–2 chars.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under event-queue via gap-PUT, including the deleted-file disclosure. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- Staged-fileset hygiene: verify `git diff --cached --name-only` == my two pathspecs before committing.
- NOT done: consumer port (plan 13 Task 1 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 13 Task 1 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
