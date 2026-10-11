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

## Planned (tasks 5–7)

- `napa-solo-script`: scripted throttle/steer/brake/gear pattern to the first split (solo).
- `napa-solo-split-left`, `napa-solo-split-right`: branch commits at the first split (solo).
- `napa-solo-jump`: airborne ticks on crest sections (solo).
- `napa-field-l1`, `napa-field-l5`: seven opponents, levels 1 and 5.
- `napa-fight`, `napa-contact`: combat and contact scenarios.
- `napa-traffic-solo`, `napa-traffic-field`: traffic scenarios.
