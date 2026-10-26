# Napa solo split commits (plan 10 Task 5)

Baseline: `91bdc91` (solo script green; L1/schema2/L5/L5d/L5e/human green).

## Plan

Plan 10 Task 5 `napa-solo-split-left/right`: commit to each branch at the
first Napa split (route A split@700) in solo, player-control (`HUMAN=1`),
to verify the rider-commit assumption (`0x00439390`: lateral vs
half-0 right edge decides `route 0->1`; native `move_along` ports this,
merge-back `1->0` not yet ported -- flagged plan-17).

First split at sample 700; grid ~50; L1 pace reaches sample ~292 in 2400
ticks (~0.1 samples/tick early). Split arrival ~6500 ticks. Approach-window
forcing (no precise timing): hard steer 1500 ticks covering arrival.

## Controls (git-ignored; seeded uniquely, no clobber)

- `sim-napa-solo-split-left.controls`: 6500 hex words.
  `1*4500` (throttle to near split) + `5*1500` (throttle+left, force left
  of threshold, stay route 0) + `1*500` (past split).
  Seeded as `prefix/drive_c/napa-split-left-20261026.controls`.
- `sim-napa-solo-split-right.controls`: same but `201*1500`
  (throttle+right, force `lateral >= half0_right`, take route 1) in the
  window. Seeded as `prefix/drive_c/napa-split-right-20261026.controls`
  after left validated (serialized prefix).

## Captures (both complete; shared prefix/install/output claims serialized)

Left (`sim-napa-solo-split-left.tsv`, 6681 frames, human
`3500e654…`) and right (`sim-napa-solo-split-right.tsv`, 6681 frames,
human `102544a8…`): tool rejected `frame count 6681 != expected 7000`
(exit 0, `end` marker; actual = 6500 script + 181 coast, like script
1620+181=1801 and human 1250+181=1431). Validated partials with
`validate --simulation --frames 6681`: schema, exe hash, ticks 1-6681,
seed 1, `control_branch human`, `complete true`. Copied to
`analysis/oracle/` (git-ignored; no accepted capture overwritten; receipt
not required for diagnostic replay, like script).

Native replay (owned `build-agent-crash-2278`, auto human driver): BOTH
diverge identically at `frame 844 (tick 845): rider.0.lateral original 70
native -1952`, RNG in sync (8/8 history match). Scripts identical through
845 (throttle-only; split window starts at 4500), so shared first divergence
is pure curve physics, not branch commit. Player: pos 31396->31432 (+36,
sample 122, route 0), steer 0, speed ~9322, lat_vel 450 (golden, constant),
ground 61. Native lat_vel -1572 implied. `+0x130`/`+0x10c`/`+0x104`/dt/steer
match where compared; `+0x130` (bend) is uncompared.

## Root-cause trace (in progress; probes reverted; tree clean)

- `move()` (`0x004395e0`, REA fetched) is CORRECT at 845: after-move
  lateral 70 matches golden (probe entry lat -380 +450 drift; `+0x130`
  bend 288, `+0x10c` 450, dt 1, `+0x104` 0 -- all match). `walls()`
  leaves it 70 (probe after-walls). Final native -1952 must come AFTER
  `rider_move` -- contacts.
- Overlap probe at tick 845 (reverted): player bike (r0,t4,pos31432,lat70)
  vs frozen opponent bike (r1,t4,pos31488,lat15360): pos diff 56,
  lat diff 15290 <= half-widths sum 15360 (marginal overlap). Triggers
  native `collide()` separation + handler shove (-2022). Golden has NO
  shove (70, drift only) with RNG in sync (no sounds) -- original sees
  NO overlap (frozen lateral must differ; it is NEVER compared: solo
  dumps only player bike/half; `node.*` has no lateral).
- Naive fix (skip frozen inners) makes it WORSE: first divergence moves
  EARLIER to tick 149 RNG (original 2193798822 vs native 1793882572).
  I.e. the tick-149 draw IS a frozen contact with sounds in both worlds
  (player pos ~13746, lat 15123 after -1000 shove, speed 3351->2739;
  RNG changes in both without the skip). Correct fix must preserve
  sounds/RNG while fixing the 845 shove -- i.e. correct the frozen
  geometry (spawn laterals / half-widths) so overlaps occur exactly
  when original overlaps (149: yes, both; 845: no, neither), not
  blanket-skip. Reverted the skip; tree clean at `b619cb4` + docs.
- Marginal overlap at 845: native lat diff 15290 <= half-widths sum
  15360 (overlap by 70). Player lat 70/70 matches; frozen lat 15360
  (native, never compared) must be ~70+ off vs original (outside
  70±15360). Pos diff 56 matches (player 31432/31432; frozen 31488 via
  `node.*` -- nodes match). Half-widths (`+0x174`, never compared)
  may also be off. L1 spawn laterals match (L1 green), so grid tables
  are right for live; solo frozen spawn needs its own audit
  (`place_rider` grid vs `read_grid_slot` EXE values for frozen slots;
  `nodeaddr.*` may carry original frozen words).
- `+0x324` now set (0/1) but `+0x328`/branch mapping still 0 for all;
  walls gate still always takes walls path (routes 0/0 at 845).
  L1 spawn laterals match (L1 green), so grid tables are right for live;
  solo frozen spawn needs its own audit (`place_rider` grid vs
  `read_grid_slot` EXE values for frozen slots; `nodeaddr.*` may carry
  original frozen words).

## Next (bounded; do NOT guess the frozen lateral)

1. REA `0x00405990` grid path + `read_grid_slot` EXE values for frozen
   slots vs `place_rider` fallback (`grid_lateral[slot%4]`); disassembly
   for `+0x2c`/`+0x140` spawn-sample pointer. Determine why solo frozen
   lat 15360 differs (or whether original also overlaps but handles shove
   differently -- e.g. axis choice `was_clear_forward` vs
   `was_clear_lateral` from `+0x30`/`+0x3c` prev copies, which for frozen
   never advance). REA `0x00436a40` fetched (exit 0): separation checks
   `+0x144` self, masses `+0x194`, rel `+0x108`, flags `+0x19c`,
   prev-pos `+0x30` for longitudinal was-clear -- no crash/frozen gate
   in `00436a40` itself (gates are in caller `004368f0` via `+0xc0`).
2. RANDLOG capture (`RR_ORACLE_RANDLOG=1`, like `napa-foot-randlog`)
   around tick 149 to name the draw caller (frozen-contact sounds vs
   other), proving which contacts must stay for RNG.
3. Port with REA/disassembly evidence only; never change goldens.
   Register CTest only if clean pass without golden-only suffix
   (script had +5; split may differ).

## Remaining (explicitly not in this slice)

- `napa-solo-jump` (script already covers 132 airborne ticks green; dedicated
  crest sections only if a divergence needs isolating).
- `napa-fight`, `napa-contact`, `napa-traffic-solo/field` (same Task 5
  procedure, separate captures).
- Full `0x00407410` walk, `+0x328` mapping, class-2-contained crashed speed
  path (flagged in `l5d-crash-car-20261026.md`).
