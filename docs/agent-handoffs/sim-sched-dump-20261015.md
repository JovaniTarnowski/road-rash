# Handoff: SIM restore + scheduler-metadata dump (plan 10) — October 15, 2026

## Task / plan

Plan 10 (simulation oracle), Task 6 fix order #1c (frozen scheduler cadence).
Follows `sched-cadence-20261015.md`, `sched-interval-writer-20261015.md`,
`sched-metadata-trace-20261015.md` (subagent REA, this session) and the
forensics handoffs (`codex-sim-*`, `frozen-*`). Goal: push solo replay past
the tick-210 divergence (original frozen dt 4 vs native dt 1).

## Baseline and commits (branch `main`, all pushed)

- `6788592` — sim: restore SIM captures and hydrate frozen setup state.
  Staged the SIM worker's dirty tree after stripping temporary diagnostics
  (`RR_ORACLE_MENUDBG`/`FAULTEIP`/`GREENSTART` logs, PAGE_GUARD trap,
  KillTimer block, proj secwalk) from `oracle/aweman_proxy.c`. Production
  kept: billboard forwarding + 512-record cap + per-present reset,
  SIM tick-count limit, frozen-word hydration, actor-record regression,
  `-lwinmm` link. Left untracked (not committed): `analysis/*`,
  `fuzz/seeds/*`, `tests/dash_bench.cpp` (opt-in `RR_BENCH_DASH`, not needed).
- `95af953` — oracle: dump per-tick scheduler metadata. `sched.<addr>.
  next/last/interval/dt` for type-4 nodes (address-keyed, `nodeaddr` block
  maps address to position) plus `+0x0c/+0x10/+0x14` in the slotted-rider
  `extra[]` window. No call-site change. Docs: capture spec
  (`REVERSE_ENGINEERING.md`), `sched900` recipe (`SIM_SCENARIOS.md`),
  plan note, verification receipts.

## Owned and changed files

This session edited: `oracle/aweman_proxy.c`, `docs/REVERSE_ENGINEERING.md`,
`docs/SIM_SCENARIOS.md`, `docs/VERIFICATION.md`,
`docs/plans/10-simulation-oracle.md`. No `src/*` edits (translation
deferred until the capture lands — no hardcoded interval).

## Evidence and checks

- `oracle/build.sh` (MinGW -O2 -Wall) exit 0 for both commits.
- Native Debug `build-verify-20261015` configure/build exit 0 (both commits).
- Focused `ctest -R 'oracle-patch|oracle-actor-record|frozen_state|
  sim_fields|oracle-sim'` 5/5 pass. `oracle-sim` keeps the known tick-210
  divergence on old goldens; no golden/divergence rule changed.
- `git diff --cached --check` clean both commits. Explicit-path staging;
  shared-tree friendly (no `git add -A`, no history rewrite).
- Subagents (all complete, no writes except one handoff):
  - build verification: dirty-tree build exit 0, 12/12 focused pass.
  - REA trace: `0x00438170` identical to export; wrote
    `sched-metadata-trace-20261015.md`; found interval-writer label slip
    (`00438930.c:17-20` mapping) and the player-bike driver-tail `+0x0c`
    rewrite (`00438170:79-80`).
  - plan audit: surface fix landed (`a5f612c`); finish clamp and scheduler
    fields still open; smallest next slice is this capture.

## Live capture (in flight at handoff time)

- New proxy installed (`oracle/install.sh`): prefix DLL hash
  `1b8e2357...f31c89a1` matches `oracle/build/AWEMAN32.DLL`.
- Shell `sh_13e55cca40010Iu2eax8fwqDYH` (background):
  `RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_SIM=1
  RR_ORACLE_SOLO=1 RR_ORACLE_FRAMES=900 oracle/run.sh sched900-20261015 900`,
  log `/tmp/sched900-capture.log`. Output `analysis/oracle/
  sched900-20261015.tsv` (gitignored). Holds resource claims on the shared
  prefix, oracle install and output dir until it exits; do not start a
  second capture or reinstall the DLL before it finishes.

## Remaining work (next slice, plan-close)

1. Validate the capture: header, contiguous ticks 1..900, `end` marker;
   confirm frozen interval 4 and lasts 1,5,...,209 in `sched.*` keys.
2. Translate the `0x00437e60` cadence natively (`src/race.hpp` Rider
   `sched_last/sched_next/sched_interval`, stepped for frozen riders in
   `step()`/`countdown_step()` before the early return, hydrated once from
   the capture beside `apply_frozen_state`); follow the
   `traffic.cpp:434-436` / `race_pedestrians.cpp:130-152` pattern. Do not
   touch `separate()`, the `0x3c` gate, `move()`, or live-rider dt paths.
3. Replay all three solo goldens plus the new trace with a single green
   anchor; ticks 208/209 must show dt-4 pushes before any physics edit.
4. Commit per-subsystem with full-gate receipts; push to `main`.

## Held resources

- Background capture shell (above); owned build dir
  `build-verify-20261015`; no locks. No LSP/database changes made.
