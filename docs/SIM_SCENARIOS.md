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
Single-anchor replay (`oracle-sim-test`, default; owner follow-up Oct 12):
physics green through tick 139 modulo the traffic-timer RNG draw. The
earlier tick-120 speed gap (original 1479 vs native 1474) root-caused to
the harness, not physics — solo replay built `Race` without personalities,
so the first `ai_replan` (tick 119→120) reset speed from zeroed
`spd_a`/`spd_b`; with `read_personalities` wired in, native matches
exactly through frame 138 (speed 1919). Next divergence is frame 138
(tick 139): RNG original 2574079260 vs native 1133891140, fields still
matching — most likely the `0x0044dd90` traffic-spawn timer draw (the
solo hook nulls the pool but the timers still tick and draw; the replay
never steps `TrafficSystem`). See `analysis/sim-notes.md`.

Known limitation: menu course-entry captures (`RR_ORACLE_COURSE`) run the
player on the demo AI branch, so `DAT_004b7ed0` words are written to the
dump but never read by the think function. The native replay mirrors this
with `set_demo_hold` for `napa-solo`; the human golden documents the
control-driven branch instead. Clearing `DAT_004753c8=0` mid-capture would
need a new oracle hook (open).

## Planned (tasks 5–7)

- `napa-solo-split-left`, `napa-solo-split-right`: branch commits at the first split (solo).
- `napa-solo-jump`: airborne ticks on crest sections (solo).
- `napa-field-l1`, `napa-field-l5`: seven opponents, levels 1 and 5.
- `napa-fight`, `napa-contact`: combat and contact scenarios.
- `napa-traffic-solo`, `napa-traffic-field`: traffic scenarios.
