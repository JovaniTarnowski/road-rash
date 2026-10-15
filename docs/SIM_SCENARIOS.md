# Simulation scenarios (plan 10)

Each scenario captures the original game with `RR_ORACLE_SIM=1` (per-tick
`frame` entries, header frame 0 with course/level/seed). Courses use the
original order (`0x0046df58`): 1 CITY, 2 HIWAY, 3 MEDLY, 4 NAPA, 5 CANYN.
Control streams (`.controls`, one hex word per line) replay with
`RR_ORACLE_INPUT`. Native replay is `oracle-sim-test` (plan 10 task 4).

`RR_ORACLE_SOLO=1` freezes every scheduler node except the player bike and
player rider-half and nulls the traffic pool, isolating player physics.
Full-field captures are deterministic through setup (seeded at menu entry)
but diverge mid-race: attack outcomes land on animation frames that advance
with frame pacing, so landing ticks vary run to run with no rng change.
Field scenarios are plan 10 task 6 input; acceptance goldens are solo.

## napa-solo

Level 1, default bike and character, seed 1, drive=2 autopilot, solo.
First acceptance scenario: player physics on Napa.

```sh
RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_SIM=1 RR_ORACLE_SOLO=1 RR_ORACLE_RECORD='C:\sim-napa-solo.controls' RR_ORACLE_FRAMES=2400 oracle/run.sh sim-napa-solo
```

The drive capture fixes the control stream (2,221 words). The acceptance
golden is input-driven from that stream, so capture and replay have
identical, file-determined lengths (2,221 driven ticks + 181 input-exhaustion
coast ticks = 2,402 frames):

```sh
RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_SIM=1 RR_ORACLE_SOLO=1 RR_ORACLE_INPUT='C:\sim-napa-solo.controls' RR_ORACLE_FRAMES=2700 oracle/run.sh sim-napa-solo-b
RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_SIM=1 RR_ORACLE_SOLO=1 RR_ORACLE_INPUT='C:\sim-napa-solo.controls' RR_ORACLE_FRAMES=2700 oracle/run.sh sim-napa-solo-replay
python3 tools/golden_diff.py analysis/oracle/sim-napa-solo-b.tsv analysis/oracle/sim-napa-solo-replay.tsv
```

The committed `sim-napa-solo.tsv` is the input-driven capture (renamed);
`sim-napa-solo-replay.tsv` is its replay. All three pairwise comparisons
(drive vs input-driven, input-driven vs replay) report zero differences.

Self-check (tick contiguity) and determinism replay:

```sh
python3 tools/golden_diff.py analysis/oracle/sim-napa-solo.tsv analysis/oracle/sim-napa-solo.tsv
RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_SIM=1 RR_ORACLE_SOLO=1 RR_ORACLE_INPUT='C:\sim-napa-solo.controls' RR_ORACLE_FRAMES=2400 oracle/run.sh sim-napa-solo-replay
python3 tools/golden_diff.py analysis/oracle/sim-napa-solo.tsv analysis/oracle/sim-napa-solo-replay.tsv
```

## napa-solo-straight (field, reference only)

Same settings with the standard career field (`sim-napa-solo-straight.tsv`,
2,223 ticks). Deterministic through setup; attack landings diverge mid-race
(first observed at frame 392, rider.4.speed). Kept as task 6 input for
opponent slot mapping and fight-state work, not an acceptance golden.

## napa-solo-human (acceptance: player-control path)

Level 1 Napa, default bike and character, seed 1, solo (`RR_ORACLE_SOLO=1`,
which freezes every scheduler node except the player bike and the player
rider-half and nulls the traffic pool). Unlike `napa-solo` (drive=2
autopilot, whose recorded words are ignored), this scenario feeds a
scripted human-style control stream through `RR_ORACLE_INPUT`, so the
player think `0x00434660` takes the player-control branch (applies
throttle/steer/brake from `DAT_004b7ed0`) instead of the attract-demo AI
branch (`set_demo_hold`).

Control bits (bike.hpp, tables `0x00465fb9`/`0x00476c00` order):
accelerate `0x1`, brake `0x2`, left `0x4`, right `0x200`. There are no
manual gear bits; gears shift automatically on speed thresholds
(`shift_gears`, `0x004391d0`: gear 0→1 at speed ~3000, observed at tick
189 in this capture). The script phases (1,250 words; indices are 0-based
into `sim-napa-solo-human.controls`, control ticks start 2 words after the
59-frame start-hold, i.e. word N drives golden tick 60+N):

| Words | Ticks | Controls | Maneuver |
| --- | --- | --- | --- |
| 0–299 | 60–359 | `1` full throttle | launch, gear 0→2 climb |
| 300–359 | 360–419 | `5` throttle+left (60) | hard left |
| 360–479 | 420–539 | `2` brake (120) | braking, downshift |
| 480–539 | 540–599 | `513` (`0x201`) throttle+right (60) | hard right |
| 540–839 | 600–899 | `1` full throttle (300) | straight |
| 840–899 | 900–959 | `5` throttle+left (60) | hard left |
| 900–1019 | 960–1079 | `2` brake (120) | braking |
| 1020–1079 | 1080–1139 | `513` throttle+right (60) | hard right |
| 1080–1249 | 1140–1309 | `1` full throttle (170) | straight |
| — | 1310–1431 | `0` coast (181 ticks) | input-exhaustion coast per the 0xb4 rule |

Capture (seed the control file on the Wine C: drive first; `RR_ORACLE_INPUT`
opens it inside the emulator):

```sh
cp analysis/oracle/sim-napa-solo-human.controls "$HOME/prefix/drive_c/napa-human.controls"  # path varies; run.sh maps C:\X to drive_c/X
RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_SIM=1 RR_ORACLE_SOLO=1 RR_ORACLE_INPUT='C:\napa-human.controls' RR_ORACLE_FRAMES=1800 oracle/run.sh sim-napa-solo-human
```

Result: `analysis/oracle/sim-napa-solo-human.tsv` (1,431 frames, ticks
1–1431, green at tick 60 after a 59-frame start-hold, `end` marker present)
plus `analysis/oracle/sim-napa-solo-human.controls` (a copy of the input
script). Self-check and determinism replay (Task 3 Step 4 procedure):

```sh
python3 tools/golden_diff.py analysis/oracle/sim-napa-solo-human.tsv analysis/oracle/sim-napa-solo-human.tsv
RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_SIM=1 RR_ORACLE_SOLO=1 RR_ORACLE_INPUT='C:\napa-human.controls' RR_ORACLE_FRAMES=1800 oracle/run.sh sim-napa-solo-human-replay
python3 tools/golden_diff.py analysis/oracle/sim-napa-solo-human.tsv analysis/oracle/sim-napa-solo-human-replay.tsv
```

Both report zero differences (replay verified 1,431 frames identical;
`sim-napa-solo-human-replay.tsv` is git-ignored scratch, not committed).

## Replay driver axis (replay prerequisites, Oct 12)

`solo` (field isolation: scheduler freeze, traffic nulled) and
`driver=human|demo` (the `0x00434660` branch) are independent axes.
`oracle-sim-test` selects the branch from the golden's measured
`player.demo_hold` (the oracle's `DAT_004753c8` sample), overridable with
`--driver=human|demo`. It never infers the branch from `solo`, the
`scenario` name, or nonzero recorded input. All captures to date predate
the probe and require `--driver`.

Player-think entry/counter (`0x00434660`): the harness reports
`player.think`/`player.think_count` when the probe records them. Measured
in current captures: `rider.0.think == 4225680` (`0x00407a90`, the AI-think
callee) on the demo/AI side, while the player-think entry `0x00434660`
(= 4408928) shows as `rider.1.think` (the frozen opponent keeps the
player-think dispatch word). New captures MUST record
`player.think`/`player.think_count` directly.

One stable race-setup factory (game + replay): player bike/rider resolve
through the career tables (character 1 → rider 8 via `0x00415fc0`,
`level_bike` at `0x004641c8`, rider row at `0x00463c78`), opponents via
`original_opponents` (offline original: 14, slots 1..14, `0x00405600`;
`DAT_004642d4 = 0x10`), personalities via `read_personalities`, the
vehicle library WITH EXE descriptors, traffic + crash config attached to
the Race, and the Napa road-base anchor through the shared
`rr::race_road_base_q8` helper into the same `Race` ctor the game uses
(the game passes 0 = relative route frame; replay passes the measured
anchor: `road_base = golden_frame0_lateral - route[grid].offset*256 -
grid_word`, 1792 on the solo golden). No hidden replay-only setup.

New captures MUST record, every frame: `player.demo_hold`
(`DAT_004753c8`), `player.think` (entry) + `player.think_count`
(`0x00434660` invocations), `demo.base_speed`/`demo.lane`/`demo.weapon`/
`demo.stats_*`, `grid.bias`, and `DAT_00467a6c` (the brain personality
row — the course-entry bootstrap leaves it at startup 0, not the career
rider). Pre-step RNG capture (or proven post-step equivalence: every
hold frame constant rng with `rng_calls==0`, else exit 1) is required at
the green boundary; ticks stay absolute (animation deadlines accumulate
from absolute ticks). See `analysis/replay-prereq-notes.md`.

Comparator contract: the intended field set (every native snapshot field
per live rider + every traffic pool slot + rng) is REQUIRED — a missing
golden field is a FAIL, never skip-green. Era carve-outs (car.* rows
absent in solo captures; steer/ground/stamina/weapon/slice/ai/anim/nitro
offsets no dumped era covers) are counted as uncovered and reported, not
silently skipped. Car/police/animation fields are in the set: the proxy
must dump them before acceptance.

Single-anchor replay status (`oracle-sim-test --driver=demo
analysis/oracle/sim-napa-solo.tsv`): road_base 1792 (measured per
golden, not a course constant), 59 frozen hold frames, green at tick 60,
green through tick 74 with rng equal. First divergence frame 74 (tick
75): `rider.0.lateral` original 18173 vs native 18053. Mechanism
(measured, not a physics bug): the golden carries a frozen rider.1 that
the harness cannot yet build (opponent slot mapping is task-6 work; the
port's `frozen` flag never gates `step_rider`), so the original's
pre-green replan scaled current 87→81 with rider.1 ahead and its green
tick ran state 2 with the replan target (tx snapped to 18176), while
native runs state 1 straight target (tx=1799) with no target. The
earlier tick-120 speed gap was the same class of harness artifact (no
personalities wired in); the tick-139 traffic-timer RNG note below is
superseded — the run now reaches the tick-75 field divergence first.

Known limitation: menu course-entry captures (`RR_ORACLE_COURSE`) run the
player on the demo AI branch, so `DAT_004b7ed0` words are written to the
dump but never read by the think function. The native replay mirrors this
with `--driver=demo` for `napa-solo`; a TRUE human golden needs the
`RR_ORACLE_HUMAN` hook (clear `DAT_004753c8` post-setup, open). The
current `napa-solo-human` file documents the control stream + determinism
proof, not the control-driven branch.
## Planned (tasks 5–7)

- `napa-solo-split-left`, `napa-solo-split-right`: branch commits at the first split (solo).
- `napa-solo-jump`: airborne ticks on crest sections (solo).
- `napa-field-l1`, `napa-field-l5`: seven opponents, levels 1 and 5.
- `napa-fight`, `napa-contact`: combat and contact scenarios.
- `napa-traffic-solo`, `napa-traffic-field`: traffic scenarios.

## October14 forensic SIM recovery (not acceptance)

New isolated `codex-sim-frozen900` (course4, seed1, drive2, solo, SIM,
RR_ORACLE_FRAMES=900) has900 complete contiguous ticks. The repaired proxy
counts SIM dumps for the limit. Header probe.frozen_words=1 declares the
previous-copy/velocity/flag/crash field set, matched through type4 bike and
its type8 half pointer. Old v1 goldens continue to load. This trace lacks
full required player-think/count evidence; explicit --driver=demo is a
diagnostic override only. First mismatch remains tick210.

`codex-sim-mask300` adds temporary per-tick frozen bike metadata/road-mask
fields and measured player.demo_hold=1; it is likewise forensic-only. It
shows the candidate takes3840-unit nudges at208/209 with dt4, while native
dt1 leaves an extra contact at210. Capture and translate scheduler metadata
next. Neither file replaces the old accepted fixtures or relaxes parity.

## Scheduler-metadata capture (plan 10 Task 6 fix order #1c, not acceptance)

`sched900` recipe (same as frozen900 plus scheduler words): course 4,
seed 1, drive 2, solo, SIM, `RR_ORACLE_FRAMES=900`. The proxy dumps
per-tick `sched.<addr>.next/last/interval/dt` for type-4 nodes and
`rider.<slot>.bike_c/bike_10/bike_14` for slotted riders. Purpose: prove
the frozen interval and last-tick cadence (expected interval 4, lasts
1,5,...,209) before translating the `0x00437e60` cadence natively.
