# Gameplay Systems Completion Implementation Plan (AI, Physics, Collisions, Animation, Graphics)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires plan 10 Tasks 1–4 (done). Tasks here are accepted against plan 10 tick goldens and plan 11 pixel goldens.

**Goal:** Make every race system complete and original-accurate on Napa first, then on every course: AI, physics, collisions, animation and graphics. This plan has two parts:
1. **A completion matrix.** For each system it says what "complete" means, what the code does today (review of October 11, 2026), and which plan task owns each remaining item. Most items already have an owner in plans 10, 11 and 11a; the matrix makes the whole picture visible in one place.
2. **New tasks** for the gaps no plan owned yet: rider knock-off on contact, roadside-object collision, the weapon lifecycle, off-road surfaces and reverse riding, animation verified tick by tick, the remaining dashboard pieces, and a native determinism/soak gate.

**Architecture:** No new top-level structure. `rr::Race` (`src/race.*`) steps riders; `crash.*`, `police.*`, `traffic.*` own their actors; `actor_animation.*` and `actor_descriptors.*` own playback; `actor_nodes.*`/`actor_render.*` build display nodes; `hud.*` draws the dashboard. New behaviour goes into the module that owns the actor, called from the original's point in the tick (`0x0044b3b0`).

**Tech Stack:** C++17, SDL3, CTest, Wine oracle (`oracle/aweman_proxy.c`, `oracle/run.sh`), plan 10 sim dumps (`tests/sim_fields.hpp`, `oracle-sim-test`), scene dump v3 (`tests/scene_dump.hpp`, `oracle-scene-test`).

**Spec:** the owner's request of October 11, 2026 ("complete working AI, physics, collisions, animations, graphics"), [00-INDEX.md](00-INDEX.md) definition of done item 2, [../NAPA_IMPLEMENTATION.md](../NAPA_IMPLEMENTATION.md).

## Global Constraints

- **Acceptance is a golden.** Behaviour passes when a plan 10 tick golden replays with **one** RNG anchor at the green light and zero field divergence (`oracle-sim-test`, not `--reanchor`). Drawing passes when an original pixel or display-list golden matches (`oracle-scene-test`). Synthetic tests pin boundaries only.
- **Port rule** (00-INDEX): line-by-line fixed-width integer translation with address comments; no improvements until the golden passes; all randomness through `Race::random()` in original call order; no floats, wall clock or allocation in the tick path.
- **Stand-ins** listed in PROGRESS.md "Assumptions" are removed only in the commit that adds the evidence replacing them.
- **Parallel sessions:** `src/race.cpp`/`race.hpp` are under active edit (AI replan/overtake work, October 11). Before starting a task here, run `git status`; if `race.*` has uncommitted changes you did not make, coordinate (finish or wait for that commit) rather than editing over them.
- Scenario names follow SIM_SCENARIOS.md; every new scenario is documented there with its exact capture command.

## Review Focus

1. **Two contacts in the same tick** (a rider hits a car while an opponent punches them): the original resolves them in a fixed order inside `0x004049c0`/`0x00403d60`, and only one crash starts. Task 1's `napa-contact-double` scenario covers it.
2. **Weapon held when its owner crashes beside another rider:** the weapon drops or transfers exactly as the original does, and is never duplicated or lost. Task 3's `napa-weapon-crash` scenario.
3. **Riding backwards or stopped across the start line, and across a split boundary:** position wrap, lap/finish detection and route choice must match the original. Task 4's `napa-solo-reverse` scenario.
4. **Animation state across a pause, a renderer reset, or a frame-rate drop:** animation advances on simulation ticks only, never on presented frames. Task 5's animation fields catch any frame-paced advance, and Task 7's determinism test runs with randomised frame pacing.
5. **Very long races** (level 5 Napa, a stalled player, 20+ minutes): no overflow, no unbounded containers, no slowdown. Task 7's soak runs a 30-minute simulated race under UBSan/ASan.

---

## Part 1: Completion matrix (October 11, 2026)

Status words: **verified** (golden passes), **ported** (translated, no golden yet), **partial**, **stand-in**, **missing**.

### Physics

| Item | Today | Owner |
| --- | --- | --- |
| Player bike step, gears, throttle/brake, steering, lean | ported; solo demo-hold golden matches 124 ticks with per-tick re-anchoring | Plan 10 Task 5 |
| Player-control path `0x00434660` (human inputs, not demo AI) | not exercised by any golden | Plan 10 Task 5 Step 2a |
| Splits and branch commit `0x00439390` | ported | Plan 10 Task 5 (`napa-solo-split-left/right`) |
| Jumps and landing `0x0043a190` | ported, unverified | Plan 10 Task 5 (`napa-solo-jump`) |
| Engine rpm `0x004349c0` (`+0x2dc`) | ported (session 25) | 11a Task 2 (golden in plan 10 Task 5 field table) |
| Off-road surfaces (grass/dirt drag), shoulder ride | unknown: terrain flags read, drag rule not traced | **Task 4** |
| Reverse / riding the wrong way / stopped | untested | **Task 4** |
| Nitro `0x0040b110`/`0x0040b170` | missing | Plan 17 Task 6 (HIWAY nitro bikes) |
| Race-setup RNG (11,461 calls before green) | not reproduced; anchored at green | Plan 10 known limitation; revisit after plan 17 Task 2 |

### AI

| Item | Today | Owner |
| --- | --- | --- |
| Racing state 1, replan `0x004082f0`, transitions `0x00408ad0` | ported, in progress (uncommitted, parallel session) | Plan 10 Task 6 |
| Overtaking `0x00408e40`, scans `0x00449ce0`/`0x00449e90` | in progress (uncommitted) | Plan 11 Task 5 |
| Fight states 2–5 (`0x0040a4f0`, `0x00409510`) | partial; "attack alongside every 45 ticks" stand-in | Plan 11 Task 5 |
| Rubber band, level throttle `0x004642a0` | ported, unverified | Plan 10 Task 6 (`napa-field-l1/l5`) |
| Police AI, spawn, chase, arrest | arrest/setup verified offline; not live | Plan 11 Task 6 |
| Traffic driving, lane changes, honking | spawn ported; motion approximate | Plan 10 Task 7 |
| Field composition per level/character | stand-in (personalities 1..7) | Plan 17 Task 2 |

### Collisions and contact

| Item | Today | Owner |
| --- | --- | --- |
| Rider–rider bike contact (`0x004049c0` bike branch) | ported | Plan 10 Task 6 |
| Knock-off decision on contact `0x004052c0` | missing | **Task 1** |
| Rider–car contact | missing (owner-reported) | 11a Task 1 |
| Rider–cop contact | missing | **Task 1** (after plan 11 Task 6 spawns cops) |
| Walls and terrain `0x0043a610` | stand-in clamp | Plan 11 Task 4 |
| Roadside objects (signs, posts, trees, buildings) | missing; unknown whether the original tests them | **Task 2** |
| Attacks: hit test, damage, stamina | partial; timing stand-in (hit at tick 7 of 18) | Plan 11 Tasks 2 and 5 |

### Animation

| Item | Today | Owner |
| --- | --- | --- |
| Playback core `0x0043ae10`/`0x0043ae60`/`0x0043ae90`, fallback `0x0040ac70` | ported with tests | — |
| Riding, lean, crash bike/rider branches | ported (`advance_riding_animation`, crash branches) | Verified by **Task 5** |
| Attack animations per weapon and side, hit frames driving combat | partial (`attack_codes` table read; hit frame stand-in) | Plan 11 Task 2 Step 4 |
| Running back to the bike, remount, wrecked | ported offline | Plan 11 Task 4 |
| Cops, car animation and car views | partial (car views imported) | Plan 11 Tasks 2 and 6 |
| Tick-by-tick animation equality with the original | no animation fields in the sim dump | **Task 5** |

### Graphics

| Item | Today | Owner |
| --- | --- | --- |
| Road, hills, shoulders, buildings, scenery, horizon (Napa) | replays verified; native lists safe, not pixel-compared | Plan 11 Task 3 |
| Split stitching, marked terrain, building marking transitions | missing | Plan 11 Task 3 |
| Rider/bike/shadow nodes (owner-reported glitches) | native, never compared | 11a Task 3 |
| Actor/scene merge order, sprite scale law | partial | Plan 11 Task 2 |
| Dashboard: speedo, tach, stamina/condition, place | ported; needles drawn by native Bresenham | **Task 6** (needle pen) |
| Distance readout `DAT_004b8a34` | traced, not ported | 11a Task 2 |
| Mini dashboard, opponent list overlay | missing | Plan 11 Task 7 (mini dash), **Task 6** (opponent list) |
| Low/inset race views, low-detail bikers | missing | Plan 18 Task 4 |
| Other courses | placeholder renderer | Plan 17 |

## Napa critical path

The order that finishes Napa fastest, given what depends on what:

1. **11a Tasks 1–3** (car collision, distance readout, bike/shadow nodes): owner-visible defects.
2. **Plan 10 Task 5** to single-anchor acceptance, then **Task 6** (AI/contact/combat) and **Task 7** (traffic).
3. **Plan 11 Task 2** (animation-driven combat timing, merged actor nodes) together with **Task 5 here** (animation fields), because both need the same captures.
4. **Plan 11 Task 4** (walls/crashes) then **Tasks 1–2 here** (knock-off, cop contact, roadside objects).
5. **Plan 11 Task 5** (overtaking, fights) then **Task 3 here** (weapons).
6. **Plan 11 Task 6** (police live), **Task 4 here** (off-road, reverse).
7. **Plan 11 Task 3** (scene construction) can run in parallel with 2–6: it touches only `course_scene`/panel modules.
8. **Plan 11 Task 7** and **Task 6 here** (dashboard), then **Task 7 here** (soak), then **plan 11 Task 8** (Napa acceptance).

---

## Part 2: New tasks

### Task 1: Knock-off on contact, and rider–cop contact

**Files:**
- Modify: `src/race.cpp`, `src/race.hpp`, `src/police.cpp`, `tests/race_test.cpp`, `docs/SIM_SCENARIOS.md`, `docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`, `docs/PROGRESS.md`

**Starting point:**
- `0x004049c0` contact handler: only the bike-against-bike branch is ported (PROGRESS.md "Contact response").
- `0x004052c0` knock-off decision: missing.
- 11a Task 1 adds the rider–car branch; this task adds the knock-off decision for every branch and the rider–cop branch.

**Interfaces:**
- Consumes: `Race::contact(Rider& self, Rider& other)`, `begin_crash(...)` (`src/crash.hpp`), the cop actors from plan 11 Task 6 (`PoliceActor`).
- Produces: `bool Race::knock_off(const Rider& victim, const Bike& other, int32_t impulse)` (`0x004052c0`), called by every contact branch before `begin_crash`.

- [ ] **Step 1: Trace** `0x004052c0`: inputs (relative speed, lateral impulse, stamina, rider flags), the random call (if any) and its position in the stream, and the crash reason codes it passes to `0x00403d60`. Record them in REVERSE_ENGINEERING.md under "Contact".
- [ ] **Step 2: Capture** three plan 10 scenarios (field captures, single anchor), commands added to SIM_SCENARIOS.md:
  - `napa-contact`: drive=2 with the player steering into the pack at the start (`RR_ORACLE_INPUT` file `napa-contact.controls`, 900 ticks);
  - `napa-contact-double`: the player rides between two opponents and a car (Review Focus 1);
  - `napa-contact-cop`: a level where cops appear on Napa (plan 11 Task 6 Step 1 decides which), the player rams a cop.
- [ ] **Step 3: Write the failing tests.** Register the three scenarios in `oracle-sim-test`. Add a synthetic `race_test` case: two riders at equal position with a lateral closing speed at the traced threshold minus one and plus one; the first does not knock off, the second does, and only one crash starts when two contacts land in one tick. Run; expected FAIL.
- [ ] **Step 4: Port** `0x004052c0` and the cop branch of `0x004049c0` literally.
- [ ] **Step 5: Accept.** The three scenarios replay with zero divergence; the "Contact response" assumption is deleted from PROGRESS.md. Commit: `sim: original knock-off decision and rider-cop contact`.

---

### Task 2: Roadside objects (time-boxed trace first)

**Files:**
- Modify: `src/race.cpp`, `src/scenery.cpp`/`src/scenery.hpp`, `src/crash.cpp`, `tests/race_test.cpp`, `tests/scenery_test.cpp`, docs

**Starting point:** scenery placement is verified (1,024 original samples; 6,471 Napa objects resolve frames). Whether the original tests riders against scenery objects is unknown. Candidates: the non-bike branches of `0x004049c0`, the wall test `0x0043a610` (which may treat building facades as walls), and object flags in the MSRR/CESR records.

- [ ] **Step 1: Trace (time box: one session).** Find every caller of `0x00403d60` and list its reason codes. For each, record what was hit. Answer with evidence: does the original crash a rider on signs, posts, trees or buildings, and if so, which object flags make an object solid? **If the original never tests scenery objects, record that with the caller list, mark this task "not applicable" in the matrix, and stop.**
- [ ] **Step 2: Capture** `napa-crash-scenery`: steer off the road into the first solid object found in Step 1 (the controls file is written by hand from the course geometry: `build/rr-course` prints the sample index and lateral of each object).
- [ ] **Step 3: Write the failing tests.** The scenario in `oracle-sim-test`; a synthetic `scenery_test` case that the solid-flag rule from Step 1 picks the expected objects on a hand-made section.
- [ ] **Step 4: Port** the object test at its original point in the tick. Store solid objects per route sample when the course loads (no allocation in the tick path).
- [ ] **Step 5: Accept** and commit: `sim: original roadside object collision`.

---

### Task 3: Weapon lifecycle

**Files:**
- Modify: `src/race.cpp`, `src/race.hpp`, `src/crash.cpp`, `tests/race_test.cpp`, docs

**Starting point:** riders start with the weapon from rider table word 6 + level (`Opponent::weapon`). Grabs use the stand-in "a fist hit on an armed rider succeeds 1 time in 4". The original rules live around `0x004096f0` (backhand shove and grab) and the attack resolution after the hit frame (plan 11 Task 2).

- [ ] **Step 1: Trace** grab/steal, drop on crash, pickup (if weapons can lie on the road), weapon-specific reach and damage (fists, kick, club, chain), and the player's weapon after a race (does it carry into the career? check `career.cpp` save fields). Record in REVERSE_ENGINEERING.md "Weapons".
- [ ] **Step 2: Capture** `napa-weapon-steal` (the player punches an armed opponent repeatedly) and `napa-weapon-crash` (an armed rider crashes beside another rider, Review Focus 2).
- [ ] **Step 3: Write the failing tests.** Both scenarios in `oracle-sim-test`. Synthetic `race_test`: across 1,000 seeded grab attempts the weapon count over all riders is conserved (never duplicated or lost) unless Step 1 found a rule that destroys weapons, in which case assert that rule.
- [ ] **Step 4: Port** and delete the "Weapon grabs" stand-in from PROGRESS.md.
- [ ] **Step 5: Accept** and commit: `sim: original weapon grab, drop and damage`.

---

### Task 4: Off-road surfaces, reverse riding and the finish line

**Files:**
- Modify: `src/bike.cpp`, `src/race.cpp`, `tests/bike_test.cpp`, `tests/race_test.cpp`, docs

- [ ] **Step 1: Trace** the surface drag: how terrain type/flags under the wheels (`TerrainSlice`, `0x0043a610` neighbourhood) change speed and steering; what happens past the shoulder before a wall. Trace reverse movement (negative speed) and how `0x00416010` decides the finish (Review Focus 3: a rider who stops on the line, reverses over it, or crosses it while crashed).
- [ ] **Step 2: Capture** solo scenarios: `napa-solo-offroad` (ride on the verge for 10 seconds each side), `napa-solo-reverse` (stop, reverse 200 samples across a split boundary, then ride forward), `napa-solo-finish-crash` (crash on the last 20 samples; needs a short course start: use `RR_ORACLE_START` if plan 10 added it, otherwise a long capture).
- [ ] **Step 3: Write the failing tests.** The three scenarios in `oracle-sim-test`. Synthetic `bike_test`: speed at the reverse limit and position wrap at `0` and `0xffffffff` (`uint32_t`).
- [ ] **Step 4: Port** each rule, re-running after each.
- [ ] **Step 5: Accept** and commit: `sim: off-road surfaces, reverse riding and finish detection`.

---

### Task 5: Animation verified tick by tick

The animation core is ported and unit-tested, but nothing compares a running rider's animation with the original's on each tick. Pixel goldens see only sampled frames, so a frame that advances one tick late passes unnoticed.

**Files:**
- Modify: `oracle/aweman_proxy.c` (dump fields), `tests/sim_fields.hpp` (field table), `src/race.cpp` (snapshot), `tests/oracle_sim_test.cpp`, `tests/sim_fields_test.cpp`, docs

**Interfaces:**
- Consumes: plan 10's shared field table (`tests/sim_fields.hpp`), `ActorAnimationState` (`animation`, `frame`, `queued`, `loops`, `deadline`).
- Produces: per actor (riders, crash riders, cops, cars), five new fields named `<actor>.anim`, `<actor>.anim_frame`, `<actor>.anim_queued`, `<actor>.anim_loops`, `<actor>.anim_deadline`, dumped by the oracle from the object's animation block and by `Race::snapshot` from `ActorAnimationState`. Dump version bumps by one; old goldens still load (missing fields are skipped, as the field table already does for unknown columns — check `sim_fields_test` and add that case if it does not exist).

- [ ] **Step 1: Locate** the animation block offsets in the rider, bike, cop and car objects (the fields `0x0043ae90` reads). Add them to the field table with their addresses.
- [ ] **Step 2: Write the failing test.** `sim_fields_test`: a dump line without the new fields still parses and the new fields read as "absent"; a line with them parses each value. Run; expected FAIL.
- [ ] **Step 3: Implement** the oracle dump and the native snapshot. Re-capture `sim-napa-solo` and one field scenario (`napa-field-l1`).
- [ ] **Step 4: Accept.** `oracle-sim-test` compares the animation fields with zero divergence on both. Any divergence found is fixed in the module that owns it (`actor_animation.cpp`, or the caller passing the wrong tick). Then re-run the Review Focus 4 check: `rr-game --race --ticks 1800 --autopilot` with `RR_FRAME_JITTER=1` (add: sleep a random 0–30 ms per presented frame) gives the same final snapshot hash as without it.
- [ ] **Step 5:** Commit: `oracle: per-tick animation fields; animation matches original`.

---

### Task 6: Dashboard: original needle pen and opponent list

**Files:**
- Modify: `src/hud.cpp`, `src/hud.hpp`, `src/game_main.cpp`, `tests/hud_test.cpp`, docs

**Starting point:** needles are drawn with a native 3×3 Bresenham line (11a findings). The original pen routine is called from `0x00416310`'s dashboard update. `OpponentList` (`0x004753a8`) enables an on-screen list of nearby opponents; the original toggles it in the race (plan 18 Task 1 records the key).

- [ ] **Step 1: Trace** the needle pen (width, end caps, colour index) and the opponent list draw (position, font or bitmap glyphs, rows, update rate).
- [ ] **Step 2: Capture** dashboard pixel goldens from the existing `napa-actors` or a new `napa-dash` capture with `OpponentList` on, frames where the speed and rpm needles sit at several angles, including reverse speed (11a found reverse wrap untested).
- [ ] **Step 3: Write the failing tests.** `hud_test`: the dashboard region of each captured frame matches every pixel when composed from the dumped `HudState`. Run; expected FAIL.
- [ ] **Step 4: Port** the pen and the list. Delete the Bresenham code.
- [ ] **Step 5: Accept** and commit: `hud: original needle pen and opponent list`.

---

### Task 7: Native determinism and soak gate

**Files:**
- Create: `tests/race_soak_test.cpp` (CTest `race-soak`, skips 77 without the disc, label `slow`)
- Modify: `CMakeLists.txt`, `tools/ci.sh`, `docs/VERIFICATION.md`

**Interfaces:**
- Consumes: `make_race_setup(const Assets&, exe, const RaceConfig&)` (`src/race_setup.hpp`, created by plan 17 Task 2 with plan 10's interface; if it does not exist yet, do that extraction step first, exactly as plan 17 Task 2 specifies), `Race::snapshot`.

- [ ] **Step 1: Write the test.** For each of the five levels on Napa: build the race with seed 1 and seven opponents, drive the player with the autopilot for up to 108,000 ticks (30 simulated minutes) or until the race ends, and hash every tick's snapshot into one 64-bit FNV-1a value. Run the same setup twice; the hashes must be equal. Also assert: every rider's position is monotonic modulo wrap except during recorded crashes; no container in `Race`, `TrafficSystem` or `Police` grows beyond its original pool size (16 traffic objects, the rider count, the cop count); the run completes in under 60 seconds in the Release build.
- [ ] **Step 2:** Run it in `build` (Debug), `build-ubsan` and a Release build. Fix any failure in the owning module.
- [ ] **Step 3:** Add `ctest -L slow` to `tools/ci.sh` (Release tree only). Record the five hashes and run times in VERIFICATION.md. When plan 17 adds courses, it extends the test to every course.
- [ ] **Step 4:** Commit: `test: native race determinism and 30-minute soak`.

## Exit criteria

- Every row in the Part 1 tables reads **verified**, or **not applicable** with the trace evidence, or names a later plan that owns a non-Napa item.
- `napa-contact`, `napa-contact-double`, `napa-contact-cop`, `napa-crash-scenery` (if applicable), `napa-weapon-steal`, `napa-weapon-crash`, `napa-solo-offroad`, `napa-solo-reverse` and `napa-solo-finish-crash` pass with a single RNG anchor; the animation fields match on every scenario; `race-soak` passes in all three build trees.
- PROGRESS.md "Assumptions" lists no AI, physics, collision, animation or graphics stand-in for Napa.
