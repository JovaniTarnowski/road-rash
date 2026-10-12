# Road hazards, police and crash recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task by task. If that skill is unavailable, run the same workflow by hand: one task at a time, a failing test first, then the port, then the gate. Steps use checkbox (`- [ ]`) syntax. Read [00-INDEX.md](00-INDEX.md) first. Acceptance uses the plan 10 tick oracle and the plan 11 scene oracle.

**Goal:** Every hazard the original puts on the road exists in the port and behaves as the original does, on all five courses:
- traffic lights and crossing traffic at intersections;
- per-zone traffic density, which landed on October 12 (see "Done");
- police: live spawning, chase, contact, sirens, arrest and the busted outcome;
- the crash and run-back sequence under player control, including its wreck and out-of-bounds endings;
- pedestrians and oil slicks, if the original has them. This plan proves either way before anything is built.

**Architecture:** No new top-level structure. Hazards belong to the module that owns the actor class: `traffic.*` for cars, crossing cars and intersection controllers; `police.*` for cops; `crash.*` plus `Race::step_crash` for the rider on foot. Each runs from its original point in the tick (`0x0044b3b0` → `0x00454120` ring build → `0x0044dd90` spawns → `0x00438170` scheduler). The draw side goes through `actor_nodes.*`/`traffic_draw_nodes`.

**Tech Stack:** C++17, SDL3, CTest, Wine oracle (`oracle/run.sh`, `oracle/aweman_proxy.c`), `oracle-sim-test`, `oracle-scene-test`, existing exports in `analysis/decompiled/`.

**Spec:** owner request of October 12, 2026: "confirm you have written plans for implementing other road hazards from the game like pedestrian, oil slick, police. falling down - currently game auto walks, that's not how the game works."

## Answer to the request (October 12, 2026 review)

Plans before this one covered police (plan 11 Task 6, plan 16 Task 1) and walls, crashes and remount (plan 11 Task 4). **No plan covered pedestrians, oil slicks, traffic lights, crossing traffic or per-zone traffic density.** This plan adds them. What the executable shows, from static evidence:

| Hazard | Original evidence | Port today |
| --- | --- | --- |
| Free traffic | Car pool `0x0049e654` (16 × 0x210, template `0x00475698`, type `0x10`) | Ported (`traffic.*`) |
| Per-zone traffic density | CESR events of types 1–3 call `0x0041ec70` → `0x0044dd00(byte +10)` when their end sample enters the ring front | **Ported October 12** (see "Done") |
| Traffic lights | Controller pool `0x00468ee4` (10 × 0x114, template `0x00468f10`, type `0x400`, think `0x0041f040`); signal pool `0x00468ee0` (40 × 0x108, template `0x00468f30`, type `0x40`, draw `0x0041f220`) | Missing (Task 1) |
| Crossing traffic | `0x0041f040` spawns kinds 1/3 through `0x0044df60` on the cross road's phase | Missing (Task 1) |
| Police | Cop pool `0x004c1274` (15 × 0x468, template `0x0046dde0`, type `0x20`, draw `0x00440680`, think `0x00440dd0`), spawned by `0x004409e0` | Arrest decisions and cop setup offline only; never spawned (Task 2) |
| In-road solid objects | BORR pool `0x00476ce8` (20 × 0xec, template `0x00476cb8`, type 1, no draw callback) | Collision spans ported (plan 16 Task 2) |
| Rider on foot | Rider half type 8; contact `0x004046b0`; recovery `0x0040a600` | Recovery ported; bounds wreck, police owner and traffic-on-foot open (Task 3) |
| Pedestrians | **No actor pool, template or type code found.** Object types the executable tests are 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x400. Pedestrians appear in verified City frames as scenery. | Drawn as scenery only; whether they move or collide is unknown (Task 4) |
| Oil slicks | **No pool, template, string or asset name found.** | Unknown (Task 4) |

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

- [ ] **Step 1: Trace.**
  - `0x0041eca0`: controller placement, phase table `{600, 180, 780}` or `{75, 0, 240}` (the second when the slice is type 2 and the CESR flag byte +11 is 0), the four signal objects and which roadways get them.
  - `0x0041ef60`: signal position, kind, `+0xf0` phase and `+0xf8` flags.
  - `0x0041f040`: phase advance by the controller's `+0x18` tick, the window test `[back+0x100, front-0x200]`, the every-8-ticks crossing spawn through `0x0044df60` kinds 1 and 3 with lateral speed ±0x78000, and its `rand()` position.
  - `0x0041f220`: signal head sprite `DAT_00468ee8` and lamp `DAT_00468ef0[phase]`.
  - `0x0041f1f0`: teardown.
  - Find where `DAT_00468ee8`/`DAT_00468ef0` are loaded. Find how crossing cars (kind bit 0 set) move in `0x0044e710` and whether riders collide with them.
  - Record everything in REVERSE_ENGINEERING.md "Intersections".
- [ ] **Step 2: Capture** `city-intersection`: the CITY attract demo or a human run through the first type-1 CESR zone, with car fields (`car.<i>.*`) and the ring cursors `DAT_004b6e50`/`DAT_0049f240`/`DAT_004b6e5c` dumped every tick. This also checks the +0x5100 ring offset and the zone density timing. Confirm the native replay fails.
- [ ] **Step 3: Failing tests.** In synthetic `traffic_test`:
  - a type-1 zone end creates one controller and four signals;
  - phases advance at 600/180/780;
  - a crossing car spawns only in phase 0 on ticks with bit 3 set;
  - pools cap at 10 and 40.
- [ ] **Step 4: Port** into `TrafficSystem` with fixed pools. Spawning, crossing movement and the signal draw node come from the traced data, not invented sprites.
- [ ] **Step 5: Accept.** `city-intersection` replays car fields with zero divergence. One CITY intersection frame matches pixels in `oracle-scene`. Commit `sim: intersection signals and crossing traffic`.

## Task 2: Police on every course

Plan 11 Task 6 (Napa) and plan 16 Task 1 (cop contact) own the pieces. This task covers the whole game and the order they land in.

**Files:** `src/police.hpp/.cpp`, `src/race.cpp`, `src/actor_nodes.cpp`, `src/sound.cpp`, `src/game_main.cpp`, `tests/police_test.cpp`, docs.

- [ ] **Step 1: Trace the spawn.** `0x004409e0` (pool `0x004c1274`, 15 objects, template `0x0046dde0`): which courses and levels spawn cops, from what trigger (CESR or timer), and with what `rand()` calls. Then trace the think `0x00440dd0`, draw `0x00440680` and move `0x00440990`. Answer "does Napa level 1 have cops" with evidence (plan 11 Task 6 Step 1).
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

## Task 4: Pedestrians and oil slicks (evidence gate first)

- [x] **Step 1: Prove what exists (done October 12, 2026).** Census in `analysis/hazards-census-notes.md`: 8 pools (riders/signals/controllers/cops/cars/ring/BORR), 5 templates (types 1, 0x10, 0x20, 0x40, 0x400 + riders); type 2 is the self-ignore in `0x004046b0`, not a pedestrian; `surface_class` returns 0..3 only; `0x0044e220`/`0x0044e270` car-only; EXE strings PED/SLICK/OIL/WALK/RRAN/DZHR all 0 hits.
  - Census every `0x00438b70` pool and every `0x00438930`/`0x004387a0` template, extending the table above.
  - Census every `+0xc4` type test. Find what type 2 is; `0x004046b0` treats it specially.
  - List every family on the disc whose RRAN animates (FAMILIES.RSC) and whether DZHR scenery placement (`0x00452060`/`0x00452230`) gives any of them motion or a collision span.
  - List surface handling in `0x0043a610` and `surface_class` for a low-grip class (oil, gravel, water).
  - Check `0x0044e220`/`0x0044e270` (scripted car spawns from a packet-like record) for non-car payloads.
- [x] **Step 2: Decide (done October 12, 2026).** Verdict: neither exists in this binary — no pool, template, type code, spawn path, surface class or asset string. Recorded in PROGRESS.md timeline ("No such actor was found statically"). Task closed; neither hazard is built. Never add hazards the original lacks: a "Road Rash had oil slicks" memory from another platform is not evidence for this binary.

## Task 5: Hazards on all courses and the soak

- [ ] Every Task 1–3 scenario has a counterpart on CITY, HIWAY, MEDLY and CANYN wherever the hazard occurs there (`tests/course_scene_test.cpp`'s all-course sweep is the model). `race-soak` (plan 16 Task 7) runs all five courses with hazards active. Pools never exceed their original sizes: 16 cars, 10 controllers, 40 signals, 15 cops, 20 BORR objects.

## Dependencies

Task 1 needs plan 10's car-field dump (done for traffic). Task 2 needs plan 11 Task 2 cop nodes. Task 3 needs plan 11a Task 1 car contact (done) and plan 10's single-anchor human replay. Task 4 has no dependency and can run now.
