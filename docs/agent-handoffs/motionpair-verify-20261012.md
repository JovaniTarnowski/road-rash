# motionpair-verify-20261012

Task: Verify the sibling AI merge's contact-walk motion pair (`0x00439920`, `0x00439270`), per user note to use `npx rea-agents`. (Todo named `0x00409510/0x00449c20`/hazard-gate, all already confirmed in earlier turns; the merge's message cites the motion pair as its contact-walk core, so traced that instead; recorded substitution.) Read-only review; no edits to merged code.

Baseline: `88a3c7d` (own prior commit; `36c03dc` sits below it).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, contact section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/motionpair-verify-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-439920-motion.md` (+ `.log`) — success, 2280 lines (renamed from `rea-439a80-fspeed.*`: requested `0x00439a80` but REA returned `FUN_00439920`; filename now matches content. The real `0x00449a80` was confirmed in an earlier turn.)
- `analysis/oracle/rea-439270-roadadv.md` (+ `.log`) — success, 1402 lines

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), both exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00439920` | 2526 | 2527 | TRUE |
| `0x00439270` | 1439 | 1440 | TRUE |

Address hygiene: my request typo (`0x00439a80` for the motion fn) is documented in the doc bullet so a future auditor isn't confused by the rename. The merge's tick-208 steer divergence lives in `0x004049c0`, downstream of this pair — this note covers the motion pair only.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under separation via gap-PUT. No source, test, plan, or other doc file touched — merged files left exactly as committed.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: behavioral re-verification of the merge; proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5/6 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Merged files reviewed read-only, never edited. Sibling uncommitted changes untouched. `oracle/`, prefix/install untouched.
