# Spare player-half trace (read-only, 2026-10-16)

Scenario: solo captures freeze every scheduler node except player bike +
player rider-half (`sim_freeze_others`, oracle/aweman_proxy.c:61-94).
Half = node with think 0x00434660 (=4408928, `rider.1.think` /
`nodeaddr.31.think`, type 8); bike = think 0x00407a90 (=4225680,
`rider.1`→slot correction: bike is `rider.0`, think 0x00407a90).
Evidence: analysis/oracle/sched900-20261015.tsv (900 frames, course 4,
seed 1, DRIVE=2). No source edits, no captures run, no build dirs touched.

## 1. Every rand() site reachable from 0x00434660 (analysis/decompiled/00434660.c)

Method: BFS over call graph scraped from analysis/decompiled/*.c
(310 functions reachable from 00434660). Sole rand site reachable is
FUN_00441750 (the `_rand()` wrapper; bypasses the draw and returns
DAT_0049bce8 when DAT_004753b0==3 — analysis/decompiled/00441750.c).
No direct `_rand` in any of the 18 direct callees. The 4 static
callers of 00441750 reachable from the half think, with gates:

- 004096f0:71 — weapon-steal roll (`rand&3==3`). Gated: victim exists
  (iVar1!=0), cooldown/distance proximity gates (004096f0.c:67-70),
  attacker +0x1d4==0, victim +0x1d4 not in {0,3},
  FUN_00409510(victim)!=0. And 004096f0 itself is only called from
  00434660:73-76 when FUN_00409510(bike)!=0 (contact anim state).
  No contact → never runs.
- 004096f0:96-97 — attack-damage roll (`rand%(6-level)==0`). Gated:
  DAT_004b7ecc > victim+0x1d0 (cooldown expired) after contact setup;
  004096f0:102 further requires param_1 != player bike
  (DAT_004642d8) AND `rand&3==2` (AI-only anim pick).
- 00407f30:66,83,86 — remount-anim rolls. Gated: bike+0x2f4==1
  (crash state 1) plus anim char/speed thresholds ('!'<0x1f41,
  '"'/0x1c/0x1d with <0x1b59/<0x9c4). 00434660 calls 00407f30
  unconditionally (line 131), but in-capture crash==0 every tick
  (rider.0/1.crash), so the draw sub-paths are dead.
- 004092e0:22 — AI overtake/attack roll, via 00434660→00407bd0→00408ad0
  chain. 00407bd0 is only reached on the demo/AI early-return branch
  (00434660:25-31, DAT_004753c8!=0 || DAT_004c5d7c!=1). Cooldown +
  distance gates precede the draw (004092e0.c:13-20).
- 00405990:37 — build-time personality pick (`rand%3`), reachable only
  via the 00407f30 cop-spawn path (tick&0xfff==0, +0x324==0,
  DAT_004642dc==0); not a per-tick draw.

In-capture path (controls observed ∈ {0,1,4,5,513}: never 0xE0 attack
bits, never 0x100 crash bit; crash==0; no contact): the think executes
only the drive/steer/anim tail (0040b110/0040b170, 0040acd0, 0040ad10,
0040ad30, 00407f30 crash==0 sub-path, 00407bd0-free). **Answer: the
half's think cannot draw while mirroring with no attacks, no contact,
no crash — static fact from the call graph, not just absence of
evidence.** Same holds for the bike think 00407a90: its only draw
(00407a90.c:49) needs crash∈{1,2} AND anim+0x80∈[0x2C,0x30].

## 2. Object addressing (00434660.c + 00405990.c)

- 00434660 param_1 = half node. `half+0x1c4` (=0x71*4) → bike ptr
  (iVar2); all physics via bike: +0x2f4 crash, +0x2fc rider struct,
  +0xec speed, +0xc0 flags, +0x18 dt, +0x1c pos. Half's own words:
  +0x70 anim struct, +0x80 anim char, +0x1d4/+0x1c8 attack state,
  +0x1c/+0xec pos/speed (contact-setup args), +0xc0 flags (0x10 crash bit).
- 00405990 linkage: piVar1=bike node, piVar2=half node.
  bike+0x2fc (=0xbf*4)=half; half+0x1c4 (=0x71*4)=bike; both +0x144
  (=0x51*4) cross-link. FUN_00407990 inits the brain on the half
  (rider.0.think=0x407a90 is the BIKE node word; 4225680=0x407A90,
  not 0x407990). Dump cross-check: nodeaddr.32 (type 4).half=20995544,
  nodeaddr.31 (type 8).half=0, rider.1 has no raw_1d4 window (rh==0).
- Scheduler cadence verified: rider.1.bike_14(+0x14 interval)=1,
  bike_10(+0x10 last)=tick, bike_c(+0x0c next)=tick+1, bike_18(+0x18
  dt)=1 (2 on tick 1) — live every tick.

## 3. Contact paths (src/race.cpp)

Confirmed. `rider_or_ped` (race.cpp:2229) admits only
`(rider>=0 && type==4) || pedestrian>=0`. Synthesized halves get
type 8 (race.cpp:1244-1248, lateral from `half_x()` = bike lateral
unless detached, race.hpp:369). Halves are skipped in BOTH the outer
loop (:2232) and inner loop (:2245) — neither role. (Comment :2221-2224
describes original flag semantics; the native type gate enforces it.)

## 4. Verdict: model vs prove-benign

- Corrections to the brief: (a) "mirrors exactly through tick 900" is
  wrong — 3/900 frames diverge in lateral/speed (position always
  matches): frames 207,208 (lat 29163v28203, 31052v30092; spd
  2738v3369, 2241v2758; gear 1v0; yaw60≠0) and 574 (lat 14878v15838;
  spd 6089v7494; gear 3v0). (b) "drew no RNG through tick 375" is not
  checkable in sched900: RR_ORACLE_RANDCOUNT was off (receipt env lacks
  it), so rng_calls≡0 by construction (aweman_proxy.c:146-200,235);
  the rng STATE first changes tick 139 (1133891140→2574079260) and
  keeps changing — something live draws. sim-randcount3.tsv (other
  scenario, counter on) shows counted draws at frames 84,115,139,….
- Model (minimal native step): nothing — keep dropping rider.1 rows
  (oracle_sim_test.cpp:644-651). The think needs no counterpart (no
  draws, no contact roles); the 3 mirror divergences are lateral-word
  only and never feed bike physics or RNG on the observed path.
  SPECULATION: the +0x1c/+0x28/+0x108 mirror is maintained by the
  half's move fn (+0xe4, never dumped), not the think — unverified.
- Prove-benign (capture evidence that would settle it): rerun the
  sched900 scenario with RR_ORACLE_RANDCOUNT=1 + randlog/randtrace at
  the state-change ticks (139,170,194,208,209,217,…) — expect zero
  callers inside the 00434660 subtree (004096f0/00407f30/004092e0);
  plus per-tick dump of half +0xe4 (move word), +0x80/+0x1d4/+0x1d0
  (anim/attack/cooldown), and bike+0x2f4, to pin the divergence
  trigger at frames 207/208/574.
- Open (not the half): which live subsystem draws from tick 139 on
  (bike/half move fns, ring, sound, ped) — needs the randlog above.

No implementation done. No commit.
