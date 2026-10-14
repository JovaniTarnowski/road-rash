# Handoff: move() audit + tick-210/222 characterization (October 14, 2026)

## Task / plan

Plan 10 Task 6 fix order #1, gap (b) investigation (sub-slice anchor).
Baseline commit: `3481b98` (prev-copy sync). Read-only + decompile audit;
no source files changed by this session.

## 1. move() line audit vs `analysis/decompiled/004395e0.c` — MATCHES

Native `Race::move` (`src/race.cpp:1002-1034`) against the full decompile:

- `0x108/0x10c/0x110` init, yaw build/clamps, `local_4` distance,
  5-word history copy, bend 400/800/600 branch, heading, `0x28 += dt*0x10c`
  (speed > 300): all structurally identical.
- `DAT_004c1200` lean boost (`0x108 = lean*0xec>>8 + 0xec`) correctly
  omitted for solo: gated on netplay globals `0x4c5b00/0x4c5af9` (0
  offline) — same justification as the earlier boost-prefix audit.
- `bVar3` teleport overwrite (`0x28/0x20/0x58/0x5c/0x60` from the `0x324`
  table) correctly omitted for solo: fires only when `0x324 == -1`
  (solo param_5 is 0/1).
- Yaw wrap uses `0x800000` in BOTH (decompile line ~44); heading wrap
  uses `0x7fffff` in BOTH (line ~126). The old "0x800000" flag
  conflated the two — no code difference anywhere.
- `div256` == `(x + (x>>31 & 0xff)) >> 8` for all int32 (trunc division;
  prior exhaustive proof stands): bend-push rounding is NOT a drift
  source.
- One unreachable-in-practice edge (NOT ported, correctly): `local_4`
  is `uint`, so its `>> 1` is logical; native uses arithmetic `>> 1`.
  Differs only when `(0x108*2 - accel*dt)*dt < 0`, i.e. braking below
  standstill speed — brake returns 0 at standstill per the earlier
  audit, so no live tick reaches it. Do not "fix" without a replay
  showing it.

## 2. Drift hypothesis is DEAD (evidence)

Golden `rider.0.lateral` per-tick series (`sim-napa-solo-human.tsv`,
ticks 1..1431): deltas 195..215 are +563 … +200, knock +1889 at 208,
+1415/+1295 after. Lateral velocities run ~200–1400/tick through the
whole 60..208 window (137/149 nonzero). A 240-unit gap against
~1000/tick velocities is a fraction of one tick's travel — a sudden
event, not accumulated rounding. Both goldens match EXACTLY through
209/221, so each gap appears in one tick.

## 3. Both divergences are native-only CONTACTS (overlap boundary)

- Human tick 210: native rng +2 draws (contact sounds), lateral off by
  exactly one 960 nudge, speed off by shit/4 = 424. Loud contact,
  slots 0v2.
- Demo tick 222: lateral off by 240, rng STILL matched. Fits a
  sub-threshold (silent: hit < 0x7d1 and |0x188| < 0x3e9) native-only
  contact: `0xf0` moves by hit/2 with no sounds, no draws.
- Therefore: overlap-existence disagreement → placement (`0x28`
  per-sample anchor), sizes (`0x174/0x178`, templates — ported), or
  prev gate (`0x3c`, now synced to grid-approx by `3481b98`).

## 4. Frame-0 node evidence (no new capture needed)

Human golden frame 0, type-4 (bike) nodes, `position -> lateral`:

| pos | lat | note |
| --- | --- | --- |
| 31488 (0x7b00, slot 1) | 15360 | |
| 30208 | 28672 | |
| 27648 | -9984 | |
| 25088 | 69632 | |
| 23808 | 36864 | |
| 22528 | 24320 | |
| 21248 | 28672 | |
| 14592 | 44544 | |
| 14336 (0x3800, slot 3) | 83712 | far shoulder — verify slot mapping |
| 14080 | 108032 | far shoulder — verify slot mapping |
| 13824 (0x3600, slot 2) | 19200 | tick-210 frozen bike |
| 13568 | 43520 | |
| 13312 (0x3400) | 43264 | |
| 13056 | 43008 | |
| 12800 (0x3200, slot 0) | 18176 | player; equals native anchor exactly |

15 nodes = player + 14 frozen. Grid words (level 0): slots 0–3
laterals ALL 16384, positions 0x3200/0x7b00/0x3600/0x3800.
`(node_lat - grid_lat)`: slot 0: 1792 (= road_base_, anchor sample);
slot 1: -1024; slot 2: 2816. Varying per sample → the centre term is
per-sample, consistent with `route.offset*256 + road_base_` IF native
route offsets at those samples are -11/+4/… — checkable offline.

## 5. Exact next experiment (fix owner)

1. Dump native `routes_[0][sample].offset` at the grid samples (temp
   print in `build_race` or reuse `rr-course`); compare
   `offset*256 + 1792 + grid_lat` against the §4 table. If all 15
   agree: placement is EXACT and the overlap comes from mid-race
   player-line drift (audit steer/physics, not setup). If they differ:
   the residual defines the true `+0xcc` term — read `0x00439b10`
   fully (its head looks like lane/edge cases, not a plain centre;
   REA spot-check proposed) and thread the missing term through
   `Opponent`.
2. `RouteSample::offset` is already documented as "lateral centre of
   this road" — the formula structure is right; do not invent a new
   anchor.
3. Do NOT touch `move()` rounding, the lean boost, or `bVar3`: §1
   clears all three for solo.

## Checks

- Read-only: no builds, no captures, no file claims. `rg`/`fd` for
  search; decompile read directly (REA not needed — exports suffice).
- This handoff is the only write.

## 6. GRIDPROBE result (temporary instrumentation, REVERTED)

Env-gated print in `build_race` (all 14 solo opponents, level 0,
`road_base 1792`), run through `oracle-sim-test`, then fully reverted
(`git status -- src/` clean). Native `route_a[sample].offset` is **0 at
every grid sample** (51–123); native lateral = `grid_lat + 1792`:

| slot | pos | grid_lat | native | node lat | delta |
| --- | --- | --- | --- | --- | --- |
| 1 | 31488 | 16384 | 18176 | 15360 | -2816 |
| 2 | 13824 | 16384 | 18176 | 19200 | +1024 |
| 3 | 14336 | 16384 | 18176 | 83712 | +65536? |
| 4 | 13312 | 40960 | 42752 | 43264 | +512 |
| 5 | 25088 | 16384 | 18176 | 69632 | +51456? |
| 6 | 27648 | 8192 | 9984 | -9984 | -19968 |
| 7 | 30208 | 36864 | 38656 | 28672 | -9984 |
| 8 | 22528 | 20480 | 22272 | 24320 | +2048 |
| 9 | 13056 | 40960 | 42752 | 43008 | +256 |
| 10 | 13568 | 40960 | 42752 | 43520 | +768 |
| 11 | 14080 | 40960 | 42752 | 108032 | +65280? |
| 12 | 14592 | 40960 | 42752 | 44544 | +1792 |
| 13 | 21248 | 16384 | 18176 | 28672 | +10496 |
| 14 | 23808 | 40960 | 42752 | 36864 | -5888 |

(Player slot 0: 18176, exact by anchor construction.)

Readings:

- Sane slots (1/2/4/8/9/10/12) are off by -2816..+2816 (±11 world
  units): the missing term is per-sample, the size of early road
  curvature. `route.offset` integrates bend from 0; Napa's start must
  already bend (or the original centre carries a lane term native
  decodes elsewhere). Either way the fix is gap (b): resolve the true
  `+0xcc` term — read `0x00439b10` fully (REA spot-check), do not guess.
- Wild slots (3/5/11, +51–65k; adjacent samples CANNOT differ that
  much on one road): slot↔node mapping or node-lateral encoding needs
  verification before trusting those three rows. They never interact
  (far from the player line), so they block nothing today. Suspects:
  second-roadway preview laterals (`x_sep`), or a node-kind
  distinction inside type 4. The 15 nodes match the 15 grid positions
  1:1, so the positions are right; the lateral frame is the question.
- Slot 2 (tick-210 bike): native 18176 vs true 19200 (-1024). Combined
  with the player's exact anchor, the boundary is off by ~1024 at the
  contact site — consistent with a native-only overlap once the
  player rides into range. This CONFIRMS gap (b) as the tick-210
  mechanism; no AI/physics cause needed.

## Live resources

None.
