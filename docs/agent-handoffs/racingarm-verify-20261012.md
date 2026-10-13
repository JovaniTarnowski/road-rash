# racingarm-verify-20261012

Task: Verify the sibling AI merge's racing arms (`0x004090a0`, `0x004091f0`, `0x00409000`), per user note to use `npx rea-agents`. (Todo named fight handlers `0x00409460/0x00409ca0/0x0040a560`, but those are already covered: `0x00409ca0` confirmed, the other two documented REA gaps. Traced the unconfirmed racing arms instead; recorded the substitution.) Read-only review; no edits to merged code.

Baseline: `72fb1de` (own prior commit; `36c03dc` sits below it).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, AI-brain section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/racingarm-verify-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-4090a0-nobody.md` (+ `.log`) — success, 1400 lines
- `analysis/oracle/rea-4091f0-lane.md` (+ `.log`) — success, 1198 lines
- `analysis/oracle/rea-409000-racing.md` (+ `.log`) — success, 778 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x004090a0` | 1417 | 1418 | TRUE |
| `0x004091f0` | 1001 | 1002 | TRUE |
| `0x00409000` | 165 | 166 | TRUE |

The merge's tick-207 parity stays with its own replay test; this note covers export fidelity only.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under AI-brain-finding via gap-PUT. No source, test, plan, or other doc file touched — merged files left exactly as committed.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: behavioral re-verification of the merge; proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5/6 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Merged files reviewed read-only, never edited. Sibling uncommitted changes untouched. `oracle/`, prefix/install untouched.
