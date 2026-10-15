# Handoff: frozen scheduler cadence trace (plan 10 Task 6 fix order #1c) — October 15, 2026

## Task / plan

Plan 10 Task 6 fix order #1 (grid setup) is done for previous-copies (#1a,
`51cb898` follow-up `prev-sync-20261014.md`); the `0x28` placement anchor
(#1b) is traced in `grid-setup-20261014.md`. This handoff traces the NEXT
cause in the same chain: frozen-node scheduler metadata (`+0x0c/+0x10/+0x14/
+0x18`, `0x00437e60` + driver `0x00438170` + `LAB_00438108`). It unblocks the
tick-210 native-only contact (lateral off by one 960-unit nudge, speed off by
424, native +2 RNG draws; `sim-fault-2.md`, `plan10-tick208-20261014.md`).

Baseline: `a5f612c` (branch `main`) plus the SIM worker's uncommitted
frozen-state integration (preserved untouched; see Hygiene). REA on the
hash-verified disc EXE per `docs/TOOLING.md`.

## Mode

READ-ONLY research plus replay diagnostics. No source edits, no oracle edits,
no Wine captures, no shared-prefix writes. One owned build dir
(`build-goal-20261015`, Debug) for gates; no shared build dirs touched.
This handoff file is the only write.

## 1. REA spot-check (this session, pinned 6.4.0)

Env per `docs/TOOLING.md` (node v24.13.0, Ghidra 12.1.4 libexec,
JDK 21); single call exit 0 on the hash-verified EXE
(`73d7f974…9a60f18a2`, matches `analysis/inventory.json`):

- `0x00437e60` — pseudocode IDENTICAL to
  `analysis/decompiled/00437e60.c` (head, interval tail, `LAB_00438108`;
  body range `0x437e60–0x43816d`, 782 bytes). Evidence: `/tmp/rea-437e60.md`
  (local, not committed).

## 2. Exact semantics (project decompile, REA-identical)

Head (`00437e60.c:11-13`, REA-confirmed): `+0x10 = tick`, `+0x18 = tick −
old_last`, written BEFORE the `+0xe0` think / `+0xe4` move null checks.
Null move (`+0xe4 == 0`, `:17`) jumps to `LAB_00438108`, skipping think,
move AND the interval recompute.

Tail (`:152-153`): `LAB_00438108: +0x0c = +0x14 + tick`, then a
range-gated `+0xe8`/slice callback when `param_3` (slice present) is set.
So a frozen (nulled) node still evolves `last/next/dt` on every scheduled
run; only the interval is retained.

Driver (`00438170.c:19-23`, param_1 == 0 path): each object in the scheduler
list runs `0x00437e60(node, tick=DAT_004b7ecc, slice-present)` iff
`+0x0c <= tick`. The spatial-band paths (`:27-64`) add `+0x10 < tick[-2]`
alternate gates for flagged nodes, but the primary gate is `+0x0c <= tick`.

Interval tail (skipped when nulled; determines the frozen interval from
whatever ran BEFORE the freeze): for bikes (`+0xc4 == 4`):
`(4 & 0x3c) != 0`, `!= 0x10`, `(4 & 0xc) != 0` → `+0x2c == 0 ? 0x1e(30)` else
the second distance variant (`00437e60.c:51-94`): `d = (+0x1c −
DAT_004b6e5c) >> 8` with sar-rounding, band mapping (`< 4` → 1 or
`(−1−d)/2 + 2` below −2; `>= 4` → `((d+1) round)/4 + 2`), slow-speed bump
(`tick < 300 && +0xec < 0x1800` and not-the-player: +1, +2 below `0x1000`,
+2 below `0x800`), clamp 8, far-field `×4` clamp `0x3c` outside
`−3..0x25`. `DAT_004b6e5c` is the moving ring base (`004552e0.c:27` sets it
to the ring-build position; `00454120.c:14` advances it), NOT a constant.

## 3. Replay evidence (owned build, frozen integration in tree)

`cmake --build build-goal-20261015 --parallel`: exit 0, no warnings.
`ctest -R 'frozen_state|oracle-actor-record|oracle-patch|sim_fields'`: 4/4
pass. Fast gate minus 6 slow tests: **89/89 pass** (114 s; excluded:
`race-soak`, `raster-rgb`, `hires-render`, `oracle-scene`, `widescreen`,
`course_scene` — slow-gate run in progress at handoff time).

All three solo goldens reproduce the IDENTICAL tick-210 signature through
the integrated frozen replay (`--driver=demo`, single green anchor):

- `analysis/oracle/sim-napa-solo-human.tsv`
- `analysis/oracle/codex-sim-frozen900.tsv` (900 ticks, symlinked forensic)
- `analysis/oracle/codex-sim-mask300.tsv` (300 ticks, symlinked forensic)

Signature: `road_base 1792`, 59-frame start-hold, green at tick 60,
`countdown: 58 native ticks, brain equal`, first divergence frame 209
(tick 210): `rng original 849046096 native 2140064586`; prior 7 RNG pairs
in sync. Frozen hydration therefore holds through tick 209; the remaining
cause is scheduler dt (original dt4 → 3840-unit nudges at 208/209 per
`codex-sim-mask300` forensics; native dt1 → 960, leaving the tick-210
overlap, speed loss 424 and 2 contact-sound draws).

## 4. Native counterpart map (baseline line numbers)

- `src/race.cpp:145` — all bikes `+0x18 = 1` at build; only `riders_[0]` is
  ever re-written (`:2164` countdown, `:2228` step). Frozen riders keep 1
  forever (`step_rider :1769` early-return; `advance_ride_animation :2083`
  also returns).
- `src/race.cpp:985-986` (`contact`) — `push =
  div_pow2(s.174 + o.174, 4) * s.18`; the frozen bike as `self` pushes with
  dt 1 natively vs 4 originally (960 vs 3840). `contact_car :869` same shape.
- `src/race.cpp:1016-1017` (`move`) — `distance` uses dt, but frozen riders
  never reach `move()`. No change needed there.
- `src/race.cpp:2193-2195` (`collide_riders`) — frozen skipped as OUTER
  nodes (comment `:2174` already records `+0x10 == tick` never holds); they
  still collide as INNER nodes via live outers (`collide() :509-514`).
- `src/race.cpp:2157-2170` (`countdown_step`) — comment `:2159-2162` already
  records the gap ("distance-based `0x00437e60` cadence, which is not
  modelled: they keep dt 1").
- Models to follow: `src/traffic.cpp:434-436` and
  `src/race_pedestrians.cpp:130-152` (`next <= tick` → `dt = tick − last;
  last = tick; next = tick + interval`, else retain dt).

## 5. Open unknowns (do NOT guess; record-then-translate per 00-INDEX)

1. Build-time `+0x14` writer: `0x00438930.c:18-19` writes `[0x38]/[0x39]`
   (bytes `0xe0/0xe4`, think/move pointers) — NOT the interval. Which of
   `0x004387a0` / `0x00405990`-tail / `0x00405c80` / `0x00405ea0` (or the
   template image) sets bike `+0x14` is untraced.
2. Freeze timing: whether the oracle's solo freeze nulls think/move before
   tick 1 (intervals come from the build writer) or at/after green
   (intervals come from live countdown scheduling with real
   speeds/distances). Statically computing the tail at build gives ≥ 5 for
   grid geometry (base ≥ 1 + slow bump +4), contradicting measured 4 — so a
   live-scheduled origin is plausible but UNPROVEN. Needs the capture.
3. The `+0x10 < tick[-2]` band gates (`00438170.c:31-64`) for `+0xc4 & 0x3c`
   nodes: whether frozen grid bikes take the primary or alternate gate on
   any tick. The record settles it.

## 6. Minimal edit proposal (fix owner; no code changed here)

Oracle (`oracle/aweman_proxy.c`, SIM owner coordinates): extend the per-node
dump with `+0x0c/+0x10/+0x14` (keep `+0x18`), add to `oracle/sim_fields.h` +
`tests/sim_fields.hpp`; capture one `codex-sim-sched900` on the same
course/seed/drive/solo recipe as frozen900. Expect frozen slot intervals 4
with lasts 1,5,…,205,209.

Native (`src/race.hpp` `Rider` + `src/race.cpp`): add
`sched_last/sched_next/sched_interval` (do NOT overload bike words until the
record lands); step them for frozen riders in `step()` and
`countdown_step()` before the frozen early-return, following the
traffic/pedestrian pattern; hydrate `sched_*` + bike `+0x18` once from the
capture in `tests/oracle_sim_test.cpp` beside the frozen-state call. Rerun
the same three traces: tick 208/209 must show dt4 pushes before any physics
edit. Do NOT touch `separate()`, the `0x3c` gate, `move()`, or live-rider
dt paths; do NOT hardcode interval 4 as a final value.

File ownership: `oracle/*`, `tests/oracle_sim_test.cpp`, `CMakeLists.txt`
and shared docs have live claims by `codex-sim-integrate-20261014`
(stale PID 62801, verified gone via `ps`, but its tree edits were
preserved untouched). Coordinate with the SIM integrator; this handoff
touches none of those files.

## Checks

- `git status --short`: only pre-existing SIM-worker modifications plus
  this handoff; `git diff --check` clean; `python3 tools/agent_lsp.py`
  reports the database healthy=false (stale: `CMakeLists.txt` newer than
  `build-clangd/compile_commands.json` — the SIM worker's uncommitted CMake
  edit; LSP lookups still resolve, no action taken on the shared symlink).
- REA: 1/1 exit 0, pseudocode identical to `analysis/decompiled/00437e60.c`.
- EXE reads: `DAT_004b6e5c` writers (`004552e0.c:27`, `00454120.c:14`),
  driver gates (`00438170.c:19-64`), tail branches (`00437e60.c:11-153`) —
  read-only `read`/`grep`, no captures, no builds in shared dirs.
- NOT done: build-time `+0x14` writer trace, freeze-timing determination,
  any code edit, slow-gate completion (running in background at handoff
  time; receipts to be appended by the integrator).

## Live resources

No live processes (slow-gate background shell `sh_13d9e57b1001q5ZxEL2hYm2Afm`
owned by this session, releases on completion), no builds in shared dirs,
no locks, no captures. REA evidence at `/tmp/rea-437e60.md` (outside repo).
