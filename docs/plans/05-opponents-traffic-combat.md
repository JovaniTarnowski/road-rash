# Opponents, Traffic, Police and Combat Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires plan 03 (sprites, animations, families) and plan 04 (world, scheduler, rider physics, oracle object dumps).

**Goal:** Complete a full race: AI opponent riders, oncoming and same-direction traffic, pedestrians if present, police and arrests, hand-to-hand and weapon combat, vehicle collisions, finishing order and results. All tick-accurate against the original for recorded scenarios, and drawn with the original sprites.

**Architecture:** Every actor is a `rr::Object` in the plan 04 `World`, with `think`/`move`/`slice_change` behaviours ported from the original function pointers stored at `+0xe0/+0xe4/+0xe8`.
- **Kind table:** Task 1 builds a table from original behaviour addresses to native behaviour functions. That table is the checklist for this plan: every address must be ported.
- **Rendering:** actors produce display nodes through `scene.cpp` using `Family`/`AnimSet`/`BobSheet` (plan 03).

**Tech Stack:** C++17, CTest, oracle (plans 01 and 04).

**Spec:** [00-INDEX.md](00-INDEX.md), [04-race-simulation.md](04-race-simulation.md) (interfaces `World`, `Object`, `Random`, `ControlMask`, `Race`).

## Global Constraints

- Same determinism rules as plan 04: no wall clock, no floats, no allocation in the tick path. All randomness through `World::random`, in the original call order.
- Behaviour dispatch is by `ObjectKind` plus a per-object state enum. Never store or compare original code addresses at runtime; they appear only in comments and the Task 1 table.
- Every task's golden test compares every object's documented fields on every tick of its scenario.

## Review Focus

1. **Many objects at once** (dense City traffic plus a full grid of opponents): the scheduler's distance bands must hold the same update order. The `city-rush` scenario in Task 2 has at least 12 simultaneous vehicles.
2. **Combat at a split** (rider and opponent on different branches that still overlap): hit detection must use the same lateral frame as the original. The `napa-split-fight` scenario is in Task 5.
3. **Arrest while crashed or while running back to the bike:** Task 6's `city-cops` scenario must include one.
4. **Finishing order ties** (two riders crossing in the same tick): Task 7's synthetic test pins the original tie rule once found.
5. **Object count limits** (the original's fixed pools, e.g. the `0x0049e5d0` array of 8 × 12 bytes stepped by `0x0044b0e0`): exceeding a pool must behave as the original does (refuse or recycle), and never crash. Task 2 has a synthetic test spawning past the limit.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/actors.hpp` | `ObjectKind` values and per-kind state structs (extends plan 04 `Object` with a `std::variant` of states) |
| `src/traffic.hpp/.cpp` | Traffic spawn (`0x0044dd90` → `0x0044df60`) and car behaviours |
| `src/opponents.hpp/.cpp` | Race grid and AI rider behaviours |
| `src/police.hpp/.cpp` | Cops: spawn, chase, arrest |
| `src/combat.hpp/.cpp` | Attacks, weapons, hits, knock-offs |
| `src/results.hpp/.cpp` | Finish detection, order, race outcome record |
| `tests/oracle_actors_test.cpp` | All actor goldens, one CTest per scenario |
| `tests/traffic_test.cpp`, `tests/results_test.cpp` | Synthetic tests |

---

### Task 1: Actor census

**Files:**
- Create: `docs/ACTORS.md`

**Interfaces:**
- Produces: a table with one row per distinct `(think, move, slice)` triple seen in the goldens:

  | Kind name | Original addresses | `+0xc4` mask | Sprite source | Native function (to be written) |
  | --- | --- | --- | --- | --- |

  Later tasks fill in the last column and tick rows off.

- [ ] **Step 1: Capture scenarios**

With a fixed seed and recorded input, capture:
- `napa-race-full`: a whole Napa race, riding normally, no attacks
- `city-rush`: City at the highest level reached from a save, dense traffic
- `napa-fight`: attack every opponent in reach with punches, kicks and a weapon, if one is picked up
- `napa-split-fight`
- `city-cops`: provoke a cop, get busted once, crash during a chase once

- [ ] **Step 2: Build the census**

1. Group objects by their `think/move/slice` pointers.
2. For each group, read where those pointers are assigned. Search with:
   ```sh
   grep -nE "0xe0\) = |0xe4\) = |0xe8\) = " analysis/decompiled/*.c
   ```
   Known assignments: `0x00405600` (player sub-object think `0x00407d40`), `0x004060b0`, `0x0040d420`, `0x004551c0`, `0x00432440`.
3. Name the kind and note its sprite source (families, `.CAR` animations, `LOCOP.DAT`).

- [ ] **Step 3: Commit**

`docs: actor census from oracle object dumps`.

---

### Task 2: Traffic

Known:
- `0x0044dd90(distance_moved)` runs three spawn timers: `0x004b7e90` oncoming, `0x004b7e94` behind, `0x004b7e80` ahead. Their reload values are `0x004b7e88` and `0x004b7e9c`, and their rates `0x004b7e84` and `0x004b7ea4`.
- Each timer picks a random lane below the lane count held at ring sub-slice `+0x2c`/`+0x30` (in 256ths).
- `0x0044df60(kind, lane, offset, 0, sub_slice)` places the car:
  - kind bit 0 set: placed relative to the road with offset `0x80`
  - otherwise lateral = `(sub_slice[+0xcc] + lane*250 + 125) * 256`
  - speeds `0x40000`, `0x100000` or `0x80000` depending on kind
  - it refuses to spawn in network modes (`0x004c5b00`/`0x004c5af9`)

**Files:**
- Create: `src/traffic.hpp`, `src/traffic.cpp`, `tests/traffic_test.cpp`
- Modify: `src/world.cpp` (call `spawn_traffic` from `step` where `0x0044b3b0` calls `0x0044dd90`)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct TrafficTimers { int32_t oncoming, behind, ahead; int32_t oncoming_reload, other_reload, oncoming_rate, other_rate; };
  void spawn_traffic(World&, TrafficTimers&, int32_t distance_moved);      // 0x0044dd90
  Object* place_car(World&, int kind, int lane, int32_t offset, const RoadSlice& at);  // 0x0044df60; nullptr when refused
  void car_think(World&, Object&); void car_move(World&, Object&);         // addresses from ACTORS.md
  }
  ```

- [ ] **Step 1: Failing tests**

- **Synthetic:** timers count down by `distance_moved`. A negative timer triggers a spawn and reloads. A lane count of 0 spawns nothing. Spawning past the pool limit behaves as the original (documented in Step 2).
- **Golden:** `oracle_actors_test napa-race-full` compares all car objects every tick.

- [ ] **Step 2: Port**

Port `0x0044dd90`, `0x0044df60` (including `0x00446790` vehicle-type choice) and the car behaviours from ACTORS.md.

- [ ] **Step 3: Pass, commit**

Commit: `sim: traffic spawning and car behaviour`.

---

### Task 3: Opponent riders

**Files:**
- Create: `src/opponents.hpp`, `src/opponents.cpp`
- Modify: `src/race.cpp` (grid setup at race start)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct OpponentSetup { int family; int bike; int skill; };
  std::vector<OpponentSetup> race_grid(const RaceConfig&, Random&);   // grid construction traced from the race preamble
  void rider_ai_think(World&, Object&);                               // AI think from ACTORS.md
  }
  ```

- [ ] **Step 1:** Trace grid construction: which families (`FAMILIES.RSC`), bikes and skills appear per level and course. Find the call order of `Random::next` during setup; the golden start state at tick 0 must match.
- [ ] **Step 2:** Port the AI rider think/move (steering to a target lane, speed control against the player, catch-up rules). `napa-race-full` must match every tick for every rider.
- [ ] **Step 3:** Commit: `sim: race grid and AI riders`.

---

### Task 4: Vehicle collisions and crashes

**Files:**
- Modify: `src/collision.cpp`, `src/rider.cpp`, `src/opponents.cpp`, `src/traffic.cpp`

**Interfaces:**
- Produces: `void collide_vehicles(World&)`, called where the original performs object-object tests. Find those in the move functions or a separate pass.

- [ ] **Step 1:** Trace rider-car, rider-rider and car-car checks. Document the bounding sizes per kind and how impact changes speed and crash state.
- [ ] **Step 2:** Capture `city-crash` (deliberately hit cars head-on and from behind). Add it to `oracle_actors_test`; it fails.
- [ ] **Step 3:** Port, pass all actor goldens, commit: `sim: vehicle collisions`.

---

### Task 5: Combat

**Files:**
- Create: `src/combat.hpp`, `src/combat.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  enum class Attack : uint8_t { none, punch_left, punch_right, kick_left, kick_right, weapon_left, weapon_right, backhand };  // confirm against control bits
  struct CombatState { Attack attack = Attack::none; int32_t timer = 0; int32_t stamina = 0; uint8_t weapon = 0; };
  void combat_step(World&, Object& attacker);          // attack start/timing from control bits or AI
  bool resolve_hit(World&, Object& attacker, Object& target);
  }
  ```

- [ ] **Step 1: Trace**

Map the attack control bits (plan 04 `ControlMask`, from the tables at `0x00465fb9`/`0x00476c00`) to attack types. Trace reach, timing windows, damage and stamina, weapon grab/steal, and knock-off thresholds. Sound triggers (`PUNCHHEA.WAV`, `KICK_MW4.WAV`, `CLUBIMPA.WAV`, `CHAINIMP.WAV`, grunts) are recorded for plan 07 as events: `World` gets `struct GameEvent { uint16_t kind; uint16_t object; int32_t a, b; }; std::vector<GameEvent> events;` (the same definition plan 07 Task 5 consumes), and the original's sound calls become events with `kind` values listed in `docs/ACTORS.md`.

- [ ] **Step 2:** `napa-fight` and `napa-split-fight` goldens fail, then pass after the port.
- [ ] **Step 3:** Commit: `sim: combat and weapons`.

---

### Task 6: Police

**Files:**
- Create: `src/police.hpp`, `src/police.cpp`

**Interfaces:**
- Produces: `void cop_think(World&, Object&)`, `bool check_arrest(World&, const Object& cop, const Object& player)`, and a race-outcome flag `RaceOutcome::busted` (Task 7).

- [ ] **Step 1:** Trace cop spawn (which courses and levels, `LOCOP.DAT` sprites), chase behaviour and the arrest condition. The six `BUSTED*.AVI` files suggest per-context arrest videos; record the selection rule for plan 07.
- [ ] **Step 2:** `city-cops` golden fails, then passes.
- [ ] **Step 3:** Commit: `sim: police and arrests`.

---

### Task 7: Finish, order and outcome

**Files:**
- Create: `src/results.hpp`, `src/results.cpp`, `tests/results_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  enum class RaceOutcome : uint8_t { running, finished, wrecked, busted, quit };
  struct Standing { int object; int32_t finish_tick; int place; };
  struct RaceResult { RaceOutcome outcome; std::vector<Standing> standings; int32_t player_place; };
  RaceResult race_result(const World&);   // finish line from course length and RSEC/CESR events (plan 03 Task 5)
  }
  ```
  Plan 06 consumes `RaceResult` for cash and progression.

- [ ] **Step 1:** Trace finish detection, place computation (including ties) and bike damage/wreck. **Synthetic tie test:** two objects crossing on the same tick get the order the original gives.
- [ ] **Step 2:** `napa-race-full` must produce the same final standings as the original's results screen. Dump the results-screen data in the frame hook when that screen shows.
- [ ] **Step 3:** Commit: `sim: finish detection and race results`.

---

### Task 8: Drawing actors

**Files:**
- Modify: `src/scene.cpp` (dynamic actor nodes), `tests/oracle_display_test.cpp` (include dynamic nodes), `tests/oracle_frame_test.cpp` (all frames)

- [ ] **Step 1:** Port node creation for each kind: rider, bike, shadow, car, cop, weapon. Sprite, frame, flip and remap per the original node dump.
- [ ] **Step 2:** `oracle_display_test` passes for all nodes in all actor scenarios. `oracle_frame_test` reaches 100% identical pixels on every frame of `napa-race-full`.
- [ ] **Step 3:** `rr-game` now runs a full race. Update docs and commit: `render: riders, vehicles and police`.
