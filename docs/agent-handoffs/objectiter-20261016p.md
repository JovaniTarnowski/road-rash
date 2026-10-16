# Handoff: scheduler object iteration — NULL-callback nodes advance (0x00437e60 cadence + pre/post passes) — October 16, 2026

## Task / plan

Plan 10 Task 6 fix order #1c follow-up (frozen scheduler cadence). Complements
`sched-cadence-20261015.md` (0x00437e60 cadence), `sched-metadata-trace-20261015.md`
(0x00438170 driver + writers), `sched-interval-writer-20261015.md` (build-time
+0x14), `frozen-scheduler-trace-20261014.md` (dt5→dt4 measurement). Slice: the
object-iteration frame around the cadence — pre-pass `0x00437df0` and post-pass
`0x004383e0`, the two scheduler callees with no prior REA coverage, plus the
exact NULL-callback advance table for `0x00437e60`.

Baseline: `e309e0a` (HEAD verified at session start; no commits during session).

## Mode

READ-ONLY background research. No tracked-file edits, no builds, no captures,
no oracle/prefix writes. Writes in this session: this handoff +
`analysis/objectiter-20261016p/` (2 REA outputs + logs + `compare.md`) only.

## 1. REA spot-check (pinned 6.4.0, this session, 2/2 calls)

Env per `docs/TOOLING.md`: `PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=openjdk@21`. EXE sha256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(matches `analysis/inventory.json`). Both `--format md`, both exit 0:

| addr | evidence_id | normalized pseudo | export | match |
| --- | --- | ---: | ---: | --- |
| `0x00437df0` | `ev_a14c7a05a7bf60faad50a9844401821b813d9c34791efb898081132919469914` | 645 chars | 648 (`analysis/decompiled/00437df0.c`) | TRUE |
| `0x004383e0` | `ev_25d2ff3d052dae5712044382127c32186fe21fe7cb6af79f01a1649d9f8dea9b` | 883 chars | 886 (`analysis/decompiled/004383e0.c`) | TRUE |

3-char deltas are trailing-newline normalization (same pattern as
`objectiter-rea-20261012.md`, `schedwalk-rea-20261012.md`). Raw outputs +
receipt: `analysis/objectiter-20261016p/rea-437df0.md`,
`rea-4383e0.md`, `compare.md`. REA is a spot-check, not runtime proof.

Uncovered-address selection: `0x00437e60`/`0x00438170` (REA-verified),
`0x00438930`/`0x004387a0`/`0x00405990`/`0x00405c80`/`0x00405ea0` (writer audit),
`0x004368f0` (contact REA Oct 12/16), `0x00449ce0`/`0x00449c20`/`0x00449e60`
(schedwalk), `0x00451e20`/`0x00451cb0`/`0x00452230` (objectiter),
`0x0044b3b0` (countdown-driver trace) were all covered by prior handoffs;
`0x00437df0`/`0x004383e0` had zero handoff/`REVERSE_ENGINEERING.md` hits.

## 2. Cadence table: which NULL-callback nodes advance (`00437e60.c`, REA-consistent)

Head (`:11-13`) runs BEFORE either NULL check; tail `LAB_00438108` (`:152-153`)
is the move-NULL landing pad. So:

| node state (`+0xe0` think / `+0xe4` move) | `+0x10`/`+0x18` head | think runs | move runs | interval recompute (`:30-142`) | `+0x0c = +0x14 + tick` tail | `+0xe8` slice callback |
| --- | --- | --- | --- | --- | --- | --- |
| live / live | yes | yes | yes | yes | yes | mid (`:23-28`, `+0x2c`-gated) + range (`:143-151`) + slice (`:154-171`, `param_3`-gated) |
| NULL think / live move | yes | SKIPPED | yes | yes (interval recomputed from live move) | yes | same as live |
| live think / NULL move (frozen shape) | yes | yes | SKIPPED | SKIPPED (`goto LAB_00438108`) | yes (old interval retained) | slice-only (`:154-171`) |
| NULL / NULL (solo frozen) | yes | skipped | skipped | skipped | yes | slice-only |

CORRECTION to loose "NULL-callback" phrasing in earlier notes: think-NULL alone
does NOT freeze a node — move still runs and the interval is recomputed.
Only move-NULL (`+0xe4 == 0`, `:17`) takes the frozen path. The solo frozen
bikes are NULL/NULL, so they advance `last/next/dt` every scheduled run with
a retained interval while skipping think, move, both mid `+0xe8` paths, the
interval branches, and the `+0x28` range gate — but still taking the `param_3`
slice callback at the tail.

Driver gate (unchanged, `00438170.c:19-24`): solo `param_1 == 0` path runs a
node iff `+0x0c <= tick` (signed); `param_3` passed as `+0x2c != 0`. Band loops
(`:30-71`) pass `'\\0'` in the third loop (`:56`), suppressing the slice
callback there — solo recipe is believed (UNPROVEN, record pending) to take
the `param_1 == 0` path.

## 3. New: pre-pass `0x00437df0` and post-pass `0x004383e0` (both REA-identical)

- `0x00437df0` (driver head, gated on `DAT_004977b8 != 0`): pre-sweep reaper
  keyed on byte `+0xc0` (`piVar3 + 0x30`), NOT on think/move NULL. Nodes with
  `+0xc0 & 4` set are skipped (kept); the rest route to `0x00438a70`
  (`+0xc0 & 8` clear) or `0x00438c20` (bit 8 set), with `DAT_004c3cf0` /
  `DAT_004c3d20` band markers advanced past the visited node first. Follows:
  NULL-callback (frozen) nodes survive this pass unless separately flagged in
  `+0xc0` — the freeze alone does not reap them. `0x00438a70`/`0x00438c20`
  semantics (remove vs relocate) are UNTRACED; flagged as next slice, not
  guessed.
- `0x004383e0` (driver tail, `00438170.c:82-86`; re-run iff `0x004368f0`
  returns nonzero): insertion-sort pass keyed on word `+0x1c` (`piVar4[7] <
  piVar3[7]`), re-linking nodes into ascending `+0x1c` order with the same two
  band markers maintained. Order-only: no callback, gate, or metadata word is
  tested or written. Follows: frozen nodes keep their list position by `+0x1c`
  across the sort; the `+0x0c`-driven cadence loop order for the NEXT tick
  reflects re-sorted positions.
- Untouched callees remaining in the frame: `0x00438a70`, `0x00438c20`
  (reaper arms), `0x00407610` (frame-end tail). Suggested next REA pair.

## 4. Native counterpart (read-only; line numbers at `e309e0a`)

- `src/race.hpp:180-183` — `Rider::sched_last/sched_next/sched_interval` +
  `sched_live` already exist (bike `+0x10/+0x0c/+0x14`, `+0x18` mirrored).
- `src/race.cpp:1806-1816` (`step_frozen_schedule`) — already implements the
  head+tail subset (gate `sched_next > ticks_`, `dt = tick − last`,
  `last = tick`, `next = tick + interval`), matching §2's NULL/NULL row.
- Patterns to follow unchanged: `src/traffic.cpp:434-436`,
  `src/race_pedestrians.cpp:131-149`.
- Native gaps noted (fix owner, not this session): no `0x00437df0`-style
  pre-pass or `0x004383e0`-style `+0x1c` re-sort exists natively (list order
  is grid order); think-NULL/move-live split has no native counterpart.

## 5. NOT-changed list (explicit)

- Sibling-live 8 (present as `M` at session start AND end, never read for
  content beyond `git status`): `src/course_scene.cpp`, `src/race_caches.hpp`,
  `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`,
  `tests/course_scene_test.cpp`, `tests/race_cache_test.cpp`,
  `tests/road_test.cpp`. All `rg` scans in this session excluded these paths.
- Untouched by design: `oracle/*`, `tests/oracle_sim_test.cpp`,
  `tests/sim_fields.hpp`, `src/race.*`, `src/traffic.*`,
  `src/race_pedestrians.*`, `CMakeLists.txt`, all shared docs/plans/status.
- NOT done: no source/test/oracle edit, no build, no capture, no golden
  change, no commit/push, no `0x00438a70`/`0x00438c20`/`0x00407610` trace,
  no freeze-timing or band-gate determination (open unknowns #2/#3 stand).

## Checks

- `git status --short`: the 8 pre-existing sibling `M` entries + untracked
  `analysis/*`/`docs/agent-handoffs/*` scratch (this session's two paths only).
  `git diff --check`: nothing staged; no tracked file modified.
- REA: 2/2 exit 0, whitespace-normalized pseudocode identical to exports.
- Reads (all excluding the 8): decompiled `00437e60/00438170/00437df0/004383e0`,
  `src/race.cpp:1806-1816`, `src/race.hpp:170-214`, `src/traffic.cpp`,
  `src/race_pedestrians.cpp`, prior sched/frozen/objectiter/schedwalk handoffs,
  `docs/TOOLING.md`, `analysis/inventory.json`.
- No builds or captures run; no locks taken.

## Live resources

None held by this session. No background shells, builds, captures, or Ghidra
projects started (a sibling `rea ... 0x0040af50` + Ghidra PIDs were observed
in `ps`; left untouched). REA evidence is file-local under
`analysis/objectiter-20261016p/`.

## Remaining work (fix owner / integrator)

1. Trace `0x00438a70`/`0x00438c20` (pre-pass arms) + `0x00407610` (frame tail).
2. Oracle `sched900` capture validation + native hydration (per
   `sim-sched-dump-20261015.md` §Remaining work); expect frozen intervals 4,
   lasts 1,5,…,205,209 — do NOT hardcode.
3. Decide whether native needs the `+0x1c` re-sort or grid order suffices
   (order only affects multi-node same-tick sequencing, not per-node dt).
