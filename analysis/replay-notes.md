# replay-notes (ReplayFix slice, plan 10 task 5 replay-harness corrections, Oct 12)

Doc deltas for the owner to merge (do NOT edit PROGRESS/VERIFICATION/REVERSE_ENGINEERING directly).

## What changed (harness only; no src/ behavior change)

`tests/oracle_sim_test.cpp`:
- `driver=human|demo` axis independent of `solo`: `--driver` selects the
  `0x00434660` branch; the golden's measured `player.demo_hold` (the
  oracle's `DAT_004753c8` sample) is the default. Without either, the
  binary throws instead of inferring from `solo`/filename/recorded input.
  All current captures predate the probe, so they require `--driver`.
- Header-driven construction via one shared factory (`make_race_bundle`,
  mirrors `Game::load_course`/career setup): default bike from the career
  `level_bike` table (char 1/rider 8/L0 -> index 1, was `specs.at(1)`
  hard-code), rider value/weapon/stats from the `0x00463c78` row (was
  90/2 hard-code), personalities + `exe_image` wired in, vehicle library
  WITH EXE descriptors, traffic (density 6) + crash config attached.
- Demo brain: bike/value/weapon/stats stay rider-8, but the AI brain row
  is `DAT_00467a6c`'s `0x004c6b40` copy — the `RR_ORACLE_COURSE`
  bootstrap leaves it at startup 0, NOT rider 8. Measured: golden
  base/current2 = pers0 L0 (87/70). The harness matches the measured base
  against the personality table (default row 0) and seeds that row's
  tables; `--driver` goldens should record `DAT_00467a6c` directly (open).
- Green anchor is the PRE-step state (preceding frame's post-step rng),
  proven equivalent by asserting every hold frame has constant rng and
  `rng_calls==0`; else exit 1 asking for pre-step RNG capture. Ticks stay
  absolute (animation deadlines accumulate from absolute ticks).
- Comparator requires the intended set: every native field per live rider
  + rng, resolved through the proxy-era map (10-field -> think/raw ->
  full brain/bike eras; `slice` via position-derived sample, AI via
  `brain_<hex>`). A value mismatch FAILS. Offsets no era dumps (steer,
  steer_rate, ground, stamina, weapon) are counted as uncovered, never
  silently skipped. Golden-only extras are diagnostic context, counted.
- `--capabilities` reports `human-controls-single-anchor`, `driver-axis`,
  `green-pre-step-anchor`, `strict-field-set` (see `tools/verify_sim.py`).
- Road base is measured per golden
  (`frame0 lateral - route[grid].offset*256 - 0x00464218 word`, reported
  every run: 1792 on `sim-napa-solo.tsv`) through the game's own
  `Race road_base_` + grid-word mechanism. The game defaults it to 0
  (relative frame); unifying the frames is open (plan 17).

`tests/golden.hpp`: v1 keeps parsing (no magic bump); accepted versions
are explicitly enumerated (`kVersions`), new versions only with a parser.

`tests/sim_fields_test.cpp`: one synthetic green-anchor replay — fists
vs a club-carrying neighbour in reach, seed 777 (first two draws have
`&3==3`): punch, hit lands tick 7 with exactly one draw (steal, verified
`rng==s1` + `rider.0.weapon==1`); cooldown, re-arm, second punch lands
with exactly one more (`rng==s2`, weapon==2); absolute tick 32.

## Divergence report (diagnostic, not acceptance)

`--driver=demo` on `sim-napa-solo.tsv` (single anchor at green):
- Green through frame 73 (tick 74) incl. rng; first divergence frame 74
  (tick 75): `rider.0.lateral` original 18173 vs native 18053, rng equal.
- Mechanism (measured): the solo golden carries a frozen rider.1 (slot 1,
  pos 12800/lat 18176, think `0x00434660`); the harness builds zero
  opponents (slot mapping is task-6 work; the port's `frozen` flag is
  never checked in `step_rider`, so a harness-only frozen build is
  impossible without `src/` support). The original's pre-green replan saw
  rider.1 ahead and scaled current 87->81 (case 2, `(0+94)*87/100`), and
  its green tick ran state 2 with the replan target (tx snapped to own
  lateral 18176); native has no target (replan never ran pre-green), runs
  state 1 straight target (tx=1799), and steers a different line from
  tick 1. The 120-Q8 lateral gap at tick 75 is 15 ticks of that steering
  difference, not a physics bug (speed/pos/rng match to that point).
- Previous pin (frame 138 tick-139 rng) is superseded: with the corrected
  construction the run reaches the tick-75 field divergence first.
- `sim-napa-solo-human.tsv` needs a true `DAT_004753c8=0` capture (open:
  `RR_ORACLE_HUMAN` hook clearing the flag post-setup, per sim-notes);
  `--driver=human` on the current file exercises `apply_controls` but the
  golden itself ran the AI branch.

## Suggested SIM_SCENARIOS.md lines (owner merges)

See the `docs/SIM_SCENARIOS.md` deltas in this slice: driver axis,
measurement requirements for new captures (`player.demo_hold`,
`player.think`/`player.think_count`, `demo.base_speed`, `grid.bias`,
`DAT_00467a6c`), and the tick-75 solo status replacing the tick-139 note.

## Suggested VERIFICATION.md lines (owner merges)

- `oracle-sim-test --driver=demo analysis/oracle/sim-napa-solo.tsv`:
  road_base 1792, 59 frozen hold frames, green tick 60, green through
  tick 74; first divergence tick 75 lateral 18173 vs 18053 (rng equal;
  frozen-rider.1/target mechanism, task-6 work).
- `sim_fields-test`: synthetic one-draw-at-green + one-later replay passes.
- `RR_ORACLE_DIR=/nonexistent ctest -R oracle-sim` SKIPs 77 clean.
- Strict `verify-sim --mode acceptance` still fails (no human-branch
  golden, no capture receipts) — expected, not a regression.

## Open questions

1. `RR_ORACLE_HUMAN=1` hook (clear `DAT_004753c8` post-setup, ensure
   `DAT_004c5d7c=1`) vs a real menu flow reaching `0x00421430`?
2. Record `DAT_00467a6c`, `demo.*` (base/lane/weapon/stats), `grid.bias`,
   `player.think`/`player.think_count`, and pre-step RNG in the proxy?
3. Unify the game's relative route frame with the absolute sub-slice
   frame (road_base default 0 vs measured 1792) in plan 17?
4. Pinned diagnostic rule (`tools/sim_known_divergences.json`) still
   carries the old tick-139 pin; update to the tick-75 output (hash
   `dca3a7126f429ee2993e0a437835dc492d290b71a5509be70fe6619789ee3ec9`,
   trace `3ab614a0...`) AND pass `--driver=demo` through
   `verify_sim.py`/CTest, or hold until the human golden lands?
5. `sim-napa-solo-human-replay.tsv` scratch + `sim-*-probe*.tsv` files in
   `analysis/oracle/`: keep or delete at commit time?
