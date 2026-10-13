# keycontact-rea-20261012

Task: REA spot-check of keymap + contact (`0x0044fa80`, `0x004368f0`, `0x004049c0`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `5e59cc5` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, Contact section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/keycontact-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-44fa80-keymap.md` (+ `.log`) — success, 1218 lines
- `analysis/oracle/rea-4368f0-contact.md` (+ `.log`) — success, 1792 lines
- `analysis/oracle/rea-4049c0-bikecontact.md` (+ `.log`) — success, 6978 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x0044fa80` | 868 | 869 | TRUE |
| `0x004368f0` | 1769 | 1770 | TRUE |
| `0x004049c0` | 9952 | 9953 | TRUE |

`0x004049c0` at 10KB confirms the 11a car-contact port's source text. No new semantics beyond the section.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under Contact in `docs/REVERSE_ENGINEERING.md`. Edit-tool note: a CUT/PUT register dance mid-edit swallowed the `- **Contact:**` and `- **Combat:**` header lines; both restored and the final `git diff` is a pure single-line addition (verified). No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: any contact re-port (already ported); `0x00436a40` separation + `0x004373f0` cull trace; proxy capture fix; no captures run.
- Integrator: link from plan 10/16 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
