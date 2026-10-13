# riderbuild-rea-20261012

Task: REA spot-check of rider-build RNG (`0x00405600`, `0x004078f0`, `0x00441750`), per user note to use `npx rea-agents`. Sibling sessions active in tree; unowned slice only.

Baseline: `48fdd18` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, pair-construction section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/riderbuild-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-405600-riderbuild.md` (+ `.log`) — success, 6590 lines
- `analysis/oracle/rea-4078f0-personality.md` (+ `.log`) — success, 1208 lines
- `analysis/oracle/rea-441750-rand.md` (+ `.log`) — success, 964 lines

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), all exit 0 on the hash-verified reference EXE. Normalized whitespace comparison against `analysis/decompiled/`:

| Function | REA chars | Export chars | Match |
| --- | --- | --- | --- |
| `0x00405600` | 5363 | 5364 | TRUE |
| `0x004078f0` | 1033 | 1034 | TRUE |
| `0x00441750` | 111 | 112 | TRUE |

Setup-stream audit: 50 personality draws + N×(`rand%3` lane) + 1 tail draw + per-pair DZHR/trigger draws account for the pre-green stream structure. A future harness can reproduce setup exactly (removing the "known limitation" in 00-INDEX.md) by porting these four draw sites in order. Until then, single-anchor-at-green stays the acceptance rule.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one sub-bullet under pair-construction via gap-PUT. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the setup-reproduction harness (plan 10 Task 5 owner scope); proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5 next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions own uncommitted `src/`, `tests/`, `tools/`, `docs/`, `cmake/` changes — none touched. `oracle/`, prefix/install untouched.
