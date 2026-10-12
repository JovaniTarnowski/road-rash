# Road hazards, police and crash recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task by task. If that skill is unavailable, run the same workflow by hand: one task at a time, a failing test first, then the port, then the gate. Steps use checkbox (`- [ ]`) syntax. Read [00-INDEX.md](00-INDEX.md) first. Acceptance uses the plan 10 tick oracle and the plan 11 scene oracle.

**Goal:** Every hazard the original puts on the road exists in the port and behaves as the original does, on all five courses:
- traffic lights and crossing traffic at intersections;
- per-zone traffic density, which landed on October 12 (see "Done");
- police: live spawning, chase, contact, sirens, arrest and the busted outcome;
- the crash and run-back sequence under player control, including its wreck and out-of-bounds endings;
- pedestrians: the 15-actor pool spawned from DZHR course events, which walk across the road, get knocked down and get up (Task 4);
- oil slicks, if the original has them (none found so far).

**Architecture:** No new top-level structure. Hazards belong to the module that owns the actor class: `traffic.*` for cars, crossing cars and intersection controllers; `police.*` for cops; `crash.*` plus `Race::step_crash` for the rider on foot. Each runs from its original point in the tick (`0x0044b3b0` → `0x00454120` ring build → `0x0044dd90` spawns → `0x00438170` scheduler). The draw side goes through `actor_nodes.*`/`traffic_draw_nodes`.

**Tech Stack:** C++17, SDL3, CTest, Wine oracle (`oracle/run.sh`, `oracle/aweman_proxy.c`), `oracle-sim-test`, `oracle-scene-test`, existing exports in `analysis/decompiled/`.

**Spec:** owner request of October 12, 2026: "confirm you have written plans for implementing other road hazards from the game like pedestrian, oil slick, police. falling down - currently game auto walks, that's not how the game works."

## Answer to the request (October 12, 2026 review)

Plans before this one covered police (plan 11 Task 6, plan 16 Task 1) and walls, crashes and remount (plan 11 Task 4). **No plan covered pedestrians, oil slicks, traffic lights, crossing traffic or per-zone traffic density.** This plan adds them. What the executable shows, from static evidence:

| Hazard | Original evidence | Port today |
| --- | --- | --- |
| Free traffic | Car pool `0x0049e654` (16 × 0x210, template `0x00475698`, type `0x10`) | Ported (`traffic.*`) |
| Per-zone traffic density | CESR events of types 1–3 call `0x0041ec70` → `0x0044dd00(byte +10)` when their end sample enters the ring front | **Ported October 12** (see "Done") |
| Traffic lights | Controller pool `0x00468ee4` (10 × 0x114, template `0x00468f10`, type `0x400`, think `0x0041f040` every 0x1e ticks); signal pool `0x00468ee0` (40 × 0x108, template `0x00468f30`, type `0x40 << index`) | **Ported** (v2 `6eaab29`): event-start placement, slots 0..3, 30-tick think, short phases; free cars stop at red. Signal heads are never drawn (`0x0041f220` is dead in the original) |
| Crossing traffic | `0x0041f040` spawns kinds 1/3 through `0x0044df60` on the cross road's phase | **Ported** (v2): start ±0x78000 to the side, drive across at ±90°, stop on red, follow obstacles; car-field golden open (Step 5) |
| Police | Police bikes are riders built by `0x00406b80`, spawned by DZHR actor events of type ≥ 0xb (`0x004520d0` → `0x004409e0` else-branch, placed 0x2000–0x2800 Q8 from the player by two `rand()` draws). CITY 12, HIWAY 11, MEDLY 17, NAPA 20, CANYN 12 events | Arrest decisions and cop setup offline only; never spawned (Task 2) |
| In-road solid objects | BORR pool `0x00476ce8` (20 × 0xec, template `0x00476cb8`, type 1, no draw callback) | Collision spans ported (plan 16 Task 2) |
| Rider on foot | Rider half type 8; contact `0x004046b0`; recovery `0x0040a600` | Recovery ported; bounds wreck, police owner and traffic-on-foot open (Task 3) |
| Pedestrians | Pool `0x004c1274` (15 × 0x468, template `0x0046dde0`, type `0x20`, draw `0x00440680`, think `0x00440dd0`, move `0x00440990`), spawned by DZHR actor events of type < 0xb through `0x004409e0`. Artwork is in FAMILIES.RSC category 2: walk, hit, fall and get-up cycles (the woman with a walker/crutches is family 101, plus joggers, a skateboarder, businessmen, construction workers, flag girls and more). CITY 449, HIWAY 93, MEDLY 476, NAPA 85, CANYN 43 events | **Ported October 12** (`src/pedestrians.*`, `race_pedestrians.cpp`; obey signals via `0x004415b0`; capture golden open, Step 5) |
| Oil slicks | No pool, template, surface class or artwork found | Not built unless evidence appears |

**"The game auto-walks":** the port's run-back follows `0x0040a600`. In the single-player branch, with no direction held the rider walks to the bike by itself at speed 1200 (`0x0040a9d0`). Holding accelerate, brake, left or right (bits `0x800/0x1000/0x4000/0x2000`, the same keys as riding, table `0x00476c00`/`0x00465fb9`) steers the walk by hand and resets the 1,200-tick auto-remount timer. `0x0040a9e0` is the same routine for remote network riders, fed from a packet (`0x00440410`). So "no input means auto-walk" matches the static trace. Task 3 settles it against a capture of the original instead of trusting either the trace or memory. Three parts of the sequence really are missing and could make the port feel different:
- the out-of-bounds wreck;
- police ownership of the timer;
- traffic hitting the rider on foot.

## Global constraints

- Port line by line with address comments. All randomness goes through `Race::random()` in the original call order. No floats, no wall clock and no allocation in the tick path (pools are preallocated).
- Acceptance is a golden. A behaviour passes when a plan 10 tick golden replays with one RNG anchor and zero divergence in the fields it covers. A picture passes when an `oracle-scene` golden matches. Synthetic tests only pin boundaries.
- `race.*`, `traffic.*` and `game_main.cpp` are shared. Check `git status` and coordinate before editing; see AGENTS.md.
- Keep the 60 Hz tick, the integer semantics and the RNG stream intact. Rendering must not consume randomness.

## Done (October 12, 2026)

- **Per-zone traffic density.** `PathSample::zone_end` marks where `0x00452710` returns `0x80000000` (an event's end sample, types other than 4, 5 and 7). `TrafficSystem::build_slice` applies `traffic_set_density(event_parameters[1])` (`0x0044dd00`) when the slice enters the ring front. It also does this for the initial 128-slice fill (`0x004552e0` runs after `0x0044db80`). Negative values set the spawn rate and the car cruise scale `DAT_004b7e8c`. At a steady 0x60 Q8/tick on Napa this gives 123 spawns against 733 at the old fixed density 6 (`traffic-test`).
- **Ring offset.** The ring camera starts at 0x3200 and the view camera at 0x3100, and both advance by the same delta, so the ring front is view camera + 0x5100. This comes from the static trace of `0x0044b250`/`0x004552e0`/`0x00454120`/`0x0044b3b0`. The port used +0x5000. **Not capture-checked yet;** Task 1 Step 2 covers it.

## Task 1: Traffic lights and crossing traffic at intersections

**Files:** Modify `src/traffic.hpp/.cpp`, `src/race.cpp` (call point only), `src/actor_nodes.cpp` or `traffic_draw_nodes` (signal heads), `tests/traffic_test.cpp`, docs.

- [x] **Step 1: Trace.** (Second-pass corrections in `analysis/intersections-notes.md`: slots 0..3 by index, `cVar2` = zone length − 2, 0x50/0xb0 are fractions, ±0x78000 is a start offset.)
  - `0x0041eca0`: controller placement, phase table `{600, 180, 780}` or `{75, 0, 240}` (the second when the slice is type 2 and the CESR flag byte +11 is 0), the four signal objects and which roadways get them.
  - `0x0041ef60`: signal position, kind, `+0xf0` phase and `+0xf8` flags.
  - `0x0041f040`: phase advance by the controller's `+0x18` tick, the window test `[back+0x100, front-0x200]`, the every-8-ticks crossing spawn through `0x0044df60` kinds 1 and 3 with lateral speed ±0x78000, and its `rand()` position.
  - `0x0041f220`: signal head sprite `DAT_00468ee8` and lamp `DAT_00468ef0[phase]`.
  - `0x0041f1f0`: teardown.
  - Find where `DAT_00468ee8`/`DAT_00468ef0` are loaded. Find how crossing cars (kind bit 0 set) move in `0x0044e710` and whether riders collide with them.
  - Record everything in REVERSE_ENGINEERING.md "Intersections".
- [ ] **Step 2: Capture** `city-intersection`: the CITY attract demo or a human run through the first type-1 CESR zone, with car fields (`car.<i>.*`) and the ring cursors `DAT_004b6e50`/`DAT_0049f240`/`DAT_004b6e5c` dumped every tick. This also checks the +0x5100 ring offset and the zone density timing. Confirm the native replay fails.
- [x] **Step 3: Tests (done, rewritten for v2).** `traffic_test`:
  - `test_intersections`: layout (start/end, variant, lanes, slot order, fractions, lane bytes, shoulders), the first think and the 0x1e cadence, the crossing-car spawn shape and bit-3 gate, crossing cars drive across, a kind-0 car holds at the stop line on red and goes on green, pool caps and teardown;
  - `test_signal_phase`: the crosswalk-edge pedestrian lookup;
  - the all-course sweep.
- [x] **Step 4: Port (done; first version superseded by 4b).**
- [x] **Step 4b: Intersection v2 (done: `6eaab29`).** Supersedes the first port with event-start placement (new course `event_start/event_end`), heading-model crossing (`+0x5c`, signal latch `+0x1ec`, zone span `+0x1d0/+0x1d4/+0x111`, near-camera `collides`), obstacle scans for lane logic, think-cadence/teardown findings, and a stricter pedestrian-signal query (crosswalk marking flags + lane + 0x1e0 window).
- [ ] **Step 5: Accept.** `city-intersection` replays car fields with zero divergence. One CITY intersection frame matches pixels in `oracle-scene`. Commit `sim: intersection signals and crossing traffic`.

## Task 2: Police on every course

Plan 11 Task 6 (Napa) and plan 16 Task 1 (cop contact) own the pieces. This task covers the whole game and the order they land in.

**Files:** `src/police.hpp/.cpp`, `src/race.cpp`, `src/actor_nodes.cpp`, `src/sound.cpp`, `src/game_main.cpp`, `tests/police_test.cpp`, docs.

- [x] **Step 1: Trace the spawn (done).**
  - Cops come from DZHR actor events with type ≥ 0xb: the else-branch of `0x004409e0` draws `rand() % 0x81` twice, picks a ring slice `(r+0x200)·16` from the player's slice (`0x00449910`), and builds the police bike there with `0x00406b80`.
  - `placed_scenery`'s level gate already applies the `level − 4` probability to these records (word bit 31 with type ≥ 11).
  - `0x00406b80`: single-cop guard (`DAT_004642dc == 0`), network early-out, video slot cycle 0–7, rider-pair build (`0x00405990`, templates `0x464310/0x464378`, row 15), cop-becomes-owner, 10-column relationship reset + level-scaled rows (`397691a`, REVERSE_ENGINEERING.md). Police bikes are riders: they run the rider think/move/draw, not `0x00440dd0` (that think is the pedestrian-crossing machine, Task 4).
  - Napa level 1 has NO cops (level-gated census: 42 ped / 0 cops; `072c7d0`). `napa-cops` must use level 3+ (2 records). CITY L1 has 6.
  - Earlier notes called pool `0x004c1274` the cop pool. That was wrong: it is the pedestrian pool (Task 4).
- [ ] **Step 2: Capture** `napa-cops` (a level with cops), `napa-cops-crashed` (arrest while on foot) and `city-cops`. Each must reach the busted outcome `0x00416010` reason and its video choice.
- [ ] **Step 3: Port** the spawn, think and move, rider–cop contact (`0x004049c0` type `0x20` branch; plan 16 Task 1) and sirens (slots 30/31, event 19 via `0x00435ae0`). Busted feeds results and the cutscene (plan 13 Task 4). Cops count toward the HUD "nearest opponent" only if `0x00438550`'s slot list includes them (slot 14 is `DAT_004642dc`); check this.
- [ ] **Step 4: Accept** the three scenarios with a single anchor. A cop frame must match pixels. Commit `sim: live police, sirens and arrests`.

## Task 3: Crash and run-back under player control

**Files:** `src/crash.hpp/.cpp`, `src/race.cpp` (`step_crash`, `collide_traffic`), `src/game_main.cpp` (camera/HUD while on foot), `tests/crash_test.cpp`, `tests/race_test.cpp`, docs.

- [ ] **Step 1: Settle the owner report with a capture.** Record three original runs on Napa level 1. In each, crash into the first oncoming car (the 11a car-contact scenario's controls), then do one of:
  - (a) release every key;
  - (b) hold accelerate;
  - (c) hold left.

  Dump the rider half's `+0x1c/+0x28/+0x128/+0xec/+0x80` and the bike's `+0x2f4/+0x2f0` every tick. Expected from the trace:
  - (a) walks to the bike and remounts;
  - (b) walks straight ahead only while the bike is ahead, and never auto-remounts while the key is held;
  - (c) walks left.

  If the original differs, the trace is wrong: fix REVERSE_ENGINEERING.md first.
- [ ] **Step 2: Port the missing branches of `0x0040a600` for the player:**
  - the bounds wreck: on slice type 1, a bike or rider lateral outside `[+0x14c, +0x158]` calls `0x00416010(5, 0xb4)` and zeroes `+0x2f0`;
  - the police-owner exemption from the 1,200-tick timer (`DAT_004642dc`);
  - the `0x00404560` reset on state 3;
  - the race-state gate `DAT_004c5d7c == 1` and demo hold `DAT_004753c8` for directed input.

  Also port rider-on-foot contact `0x004046b0` (type 8), so traffic and riders can hit the walking rider.
- [ ] **Step 3: Native presentation.**
  - Camera behaviour while on foot (`0x00437c30` follows the player's bike `+0x138`; confirm against the capture).
  - No input is consumed by the native autopilot or touch layer during the walk.
  - Touch and gamepad controls map to the same four walk bits.
- [ ] **Step 4: Accept.** The three Step 1 scenarios replay with zero divergence in the dumped fields. `race_test` pins the bounds-wreck boundary (lateral ±1 around each edge). Commit `sim: player-directed run-back, bounds wreck and on-foot contact`.

## Task 4: Pedestrians

The first pass of this task (October 12) concluded "no pedestrians", from a pool census that mislabelled the pedestrian pool as cops and never opened the artwork. The owner then reported a person on crutches crossing the street. Evidence since then:
- FAMILIES.RSC family 101 holds that walker woman's walk/hit/fall cycle, alongside about 50 other people animations.
- `0x004520d0` spawns them from DZHR records.
- `0x00440dd0` drives them across the road.

> **Status (October 12, 2026): Steps 1, 3 and 4 done; Steps 2 and 5 (original capture and golden) open.**
> - `src/pedestrians.*` holds the pool, template and draw node; `src/race_pedestrians.cpp` holds the spawn, think, move and contact ports; `place_dzhr` keeps the actor records; `CourseScene::pedestrian_look` loads the category-2 bank.
> - `pedestrian-test` checks a CITY race (spawn, crossing, pool ≤ 15, determinism) and staged hits (fast → knocked down, slow → stagger, the rider is never unseated).
> - Native differences are listed at the top of `race_pedestrians.cpp`: no traffic lights, assets load at spawn, walls skipped, camera roadway only, pedestrians run after riders in the scheduler, no shadow.
> - Port cars don't brake for pedestrians (`0x0044a070` car braking is unported), so traffic knocks down more of them than the original likely does.

**Files:** create `src/pedestrians.hpp/.cpp`, `tests/pedestrian_test.cpp`; modify `src/scenery.cpp` (keep the DZHR actor records), `src/course_scene.*` (category-2 family residency), `src/race.cpp` (call point and contact), `src/actor_nodes.cpp` (draw), `src/sound.cpp` (event 0x16), docs.

- [x] **Step 1: Trace, recording each in REVERSE_ENGINEERING.md "Pedestrians".**
  - **DZHR actor record** (`0x004520d0`):
    - word0 bit 31 marks an actor;
    - type is `word0 & 0xf`;
    - family selector is `(word0 >> 8) & 0xff`;
    - lateral is the signed 12-bit `word1 & 0xfff` plus the slice's `+0xcc`;
    - spread is `(word1 & 0xff0000) >> 14`, giving a position of `·0x100 − 2·spread + rand() % (4·spread + 1)`;
    - `word1 >> 24` is passed as the fourth argument;
    - the record is consumed with `|= 0x3f00` after it fires.

    The level/probability gate and the pairwise sort in `0x00451fc0` (already in `placed_scenery`) apply to these records too, and so does the `rand()` order.
  - **`0x004409e0` setup:**
    - `+0x1dc` family; `+0x1e4` type;
    - `+0x1ec` side (3 when the position is ≥ 1, else 1);
    - `+0x1f8` trigger at the player position + `rand() % 0x4fb1`;
    - animation block at `+0x70` (queued `0xff`).
  - **`0x00440dd0` think by type:**
    - types 4, 5, 6 and 7 cross the road, with direction from `0x0046df30` and type 4 fixed at `0xff800800`;
    - state 0 waits until the player passes the trigger, then rolls `rand() & 1` and checks a clear road with `0x00441500`;
    - state 1 walks at `+0x1f4·100`;
    - state 3 is hit: animation 2 and a scream (`0x00436830(0x16)` at most every 0xb4 ticks);
    - states 4 and 5 get up;
    - on arrival (`0x004414a0`) the pedestrian turns around and re-arms at the player position + 0xa0;
    - type 10 plays animation 6 once the player is near;
    - types 0–3 and 8 stand.

    Also trace `0x004415b0`, `0x00441500`, `0x004414a0`, `0x0044a020` and `0x004498c0` (motion).
  - **Contact:** trace `0x00440760`/`0x00440b80` (hit, fall and collision box), rider ↔ pedestrian in `0x004049c0`, and whether a hit costs the rider speed, stamina or a crash. Also trace the draw `0x00440680` (category-2 frames 0/1 and its billboard path).
  - **Category 2 resolution** in `Family::resolve`: map animation codes 0–6 to RRAN nodes for families such as 101.
- [ ] **Step 2: Capture** `city-pedestrians` (the CITY attract demo or a human run past the first crossing type) and `city-pedestrian-hit` (ride into a crossing pedestrian). Dump the 15 pool slots every tick: position, lateral, state `+0x1e8`, animation and frame. Confirm the native replay fails.
- [x] **Step 3: Tests** (as `pedestrian-test`, against imported CITY rather than synthetic fixtures).
  - DZHR parse: CITY has 449 actor events below type 0xb and 12 at or above.
  - The spawn position formula and its `rand()` count.
  - The pool caps at 15.
  - A state-machine walk across a synthetic two-lane road.
  - A hit puts the pedestrian in state 3 with animation 2 and queues sound event 0x16 once.
- [x] **Step 4: Port** with a fixed pool of 15, at the slice-build call point (`0x00454240` → `0x004520d0`, forward ring builds only, next to `TrafficSystem::build_slice`). All `rand()` draws go through `Race::random()` in the original order. Pedestrians draw through the actor node path with category-2 RRAN frames.
- [ ] **Step 5: Accept.** Both scenarios replay with zero divergence on the pool fields. A CITY frame with a crossing pedestrian matches pixels in `oracle-scene`. Commit `sim: DZHR pedestrians, crossing, hits and screams`.

Oil slicks stay closed unless new evidence appears. No pool, template, surface class (`0x0043a0c0` returns 0..3) or artwork for them has been found.

## Task 5: Hazards on all courses and the soak

- [ ] Every Task 1–3 scenario has a counterpart on CITY, HIWAY, MEDLY and CANYN wherever the hazard occurs there (`tests/course_scene_test.cpp`'s all-course sweep is the model). `race-soak` (plan 16 Task 7) runs all five courses with hazards active. Pools never exceed their original sizes: 16 cars, 10 controllers, 40 signals, 15 cops, 20 BORR objects.

## Dependencies

Task 1 needs plan 10's car-field dump (done for traffic). Task 2 needs plan 11 Task 2 cop nodes. Task 3 needs plan 11a Task 1 car contact (done) and plan 10's single-anchor human replay. Task 4 needs the DZHR actor records kept by `placed_scenery` and the category-2 family residency; it can start now.
