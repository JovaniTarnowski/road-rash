# Race Simulation Implementation Plan

> **Status (October 11, 2026 review):** Physics, movement, AI racing and camera were ported directly into `rr::Race`. The `World`/`Object` design below was **not** built. Nothing is tick-verified yet; see [plan 10](10-simulation-oracle.md). Walls and crash/remount are in [plan 11](11-napa-race-completion.md) Task 4. This plan is kept as background for addresses and findings; follow the newer plans for remaining work.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires plan 01 (oracle with deterministic replay) and plan 02 Tasks 4–5 (terrain and scene data used by collisions).

**Goal:** Port the original fixed-tick race simulation for a single rider on an empty course: world object list and scheduler, player bike physics from its spec, controls, road-edge and scenery collisions, crash and remount, camera. Verified tick by tick against the original with recorded inputs.

**Architecture:** The original per-frame work, in order:
1. `0x00442e10(steps)` pumps messages.
2. `0x0044b2e0(steps)` runs `steps` × `0x0044b3b0()`.
3. `0x00438450` then updates the camera.

Each step `0x0044b3b0` does three things:
- updates timers and scheduled objects (`0x0044b0e0`, `0x00438170` → `0x00437e60` per object)
- advances the road ring (`0x00454120`), spawns traffic (`0x0044dd90`) and accumulates horizon heading (`0x004b7ef8`)

Objects live in a doubly linked list at `0x0046dcc8`, sorted by position `+0x1c` (Q8 samples). Each object carries:
- `+0x0c` next update tick, `+0x10` last update tick, `+0x14` update interval
- function pointers `+0xe0` (think), `+0xe4` (move), `+0xe8` (slice change)
- a type mask at `+0xc4`

The native port mirrors this with a `World` holding `Object` records and the same scheduler, and with behaviours as C++ functions selected by type instead of raw pointers.

**Tech Stack:** C++17, CTest, oracle (plan 01).

**Spec:** [00-INDEX.md](00-INDEX.md), [../REVERSE_ENGINEERING.md](../REVERSE_ENGINEERING.md).

## Global Constraints

- The simulation is a pure function of (initial state, per-tick control bitmask, RNG seed). It performs no wall-clock reads, no floating point and no allocation in the tick path.
- The RNG is an exact port of MSVC `rand` (`seed = seed*0x343fd + 0x269ec3; return (seed>>16) & 0x7fff`) behind `rr::Random`. `0x00441750` returns a fixed value when `DAT_004753b0 == 3`; port that too.
- The tick rate is the original's (Task 1 measures it). Rendering interpolates or repeats; it never changes simulation results.
- Every behaviour function cites the original function it ports.

## Review Focus

1. **Crossing a split or merge while riding** (the player's lateral position decides the branch at `0x00439390`): the golden scenario `napa-split-ride` crosses both, and Task 6 compares every tick.
2. **Very long sessions:** 32-bit wraparound of position and tick counters in a 20-minute race. Task 3 includes a synthetic test that advances the scheduler across a wrap.
3. **Frame-rate independence:** the native game at 30, 60 and 144 Hz display rates must produce identical simulation state for the same input log. Task 7 tests this.
4. **Holding contradictory controls** (accelerate and brake together, left and right together): the bitmask path must resolve them as `0x0044fa80` does. The recorded scenario `napa-mash` exercises it in Task 5.
5. **Race start and finish boundaries:** the first tick after the countdown and position past the course end. Task 6 covers them.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/random.hpp` | `rr::Random` (MSVC `rand` port) |
| `src/controls.hpp/.cpp` | Control bitmask definitions and the key→action mapping from `0x0044fa80` |
| `src/bike_spec.hpp/.cpp` | `SPEC` records from `BIKESPEC.RSC` and their application (`0x00405e30`, `0x00405ea0`) |
| `src/world.hpp/.cpp` | Object list, scheduler (`0x00438170`, `0x00437e60`), step (`0x0044b3b0`) |
| `src/rider.hpp/.cpp` | Player bike/rider behaviour: think `0x00407d40` and the move/slice functions it installs |
| `src/collision.hpp/.cpp` | Road-edge, terrain and scenery collision |
| `src/race_camera.hpp/.cpp` | Camera (`0x00438450`, `0x00437d80`, `0x00437c30`) |
| `src/race.hpp/.cpp` | Race setup (`0x004163d0` preamble) and the per-frame driver |
| `tests/random_test.cpp`, `tests/controls_test.cpp`, `tests/world_test.cpp`, `tests/oracle_sim_test.cpp` | Tests |

---

### Task 1: Oracle object and timing dumps

**Files:**
- Modify: `oracle/aweman_proxy.c`

**Interfaces:**
- Produces per tick record (the plan 01 tick hook at `0x0044b324`):
  - `tick.counter` (`0x004b7ecc`)
  - `controls` (the value returned by `0x0044fa80` during that tick; hook its call site(s) found with `grep -n "calll\s\+0x44fa80" analysis/original-disassembly.txt`)
  - `ring.position` (`0x004b6e5c`), `horizon.heading` (`0x004b7ef8`), `camera.source.*` (10 dwords at `0x004b7ee0`)
  - For each object in the list at `0x0046dcc8` (next pointer `+4`): `obj.<i>.addr`, `obj.<i>.type` (`+0xc4`), `obj.<i>.pos` (`+0x1c`), `obj.<i>.next` (`+0x0c`), `obj.<i>.last` (`+0x10`), `obj.<i>.interval` (`+0x14`), `obj.<i>.think`/`move`/`slice` (`+0xe0/+0xe4/+0xe8`), and raw dwords `obj.<i>.raw.<k>` for k in 0..191
  - `player.addr` (`*(uint32_t*)0x004642d8`) and `player.sub` (`player+0x2fc`)
- Frame records: `steps` (the argument of `0x00442e10`) and the frame's wall-clock milliseconds (`GetTickCount`) for Task 2.

- [ ] **Step 1:** Implement the dumps and rebuild. Record input once and capture, with a fixed seed:
  - `napa-solo` (the Napa race: accelerate, steer, brake, crash into scenery once, remount)
  - `napa-split-ride` (ride through the sample 700 split on each side)
  - `napa-mash` (hold contradictory keys)
- [ ] **Step 2:** Verify the determinism script (plan 01 Task 7) still passes with the new dumps.
- [ ] **Step 3:** Commit: `oracle: object list, controls and timing dumps`.

---

### Task 2: Tick rate and frame stepping

**Files:**
- Modify: `docs/REVERSE_ENGINEERING.md`
- Create: `src/race.hpp` (constants only in this task)

**Interfaces:**
- Produces: `constexpr int rr::ticks_per_second` and `constexpr int rr::max_steps_per_frame` (the `0x14` cap seen in `0x004163d0`), with the source of each documented.

- [ ] **Step 1:** Read `0x004163d0` around the `0x00442e10` call. The step count comes from a timer (`timeSetEvent` or performance counter); find its period. Cross-check against `napa-solo` by regressing `tick.counter` against the frame wall-clock ms.
- [ ] **Step 2:** Write the constants with a comment citing the addresses and the measured rate.
- [ ] **Step 3:** Commit: `race: document original tick rate`.

---

### Task 3: RNG, controls and world scheduler

**Files:**
- Create: `src/random.hpp`, `src/controls.hpp`, `src/controls.cpp`, `src/world.hpp`, `src/world.cpp`
- Create: `tests/random_test.cpp`, `tests/controls_test.cpp`, `tests/world_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  class Random {                                        // 0x00455ea0 srand / 0x00455eb0 rand
  public:
      explicit Random(uint32_t seed = 1) : seed_(seed) {}
      void seed(uint32_t s) { seed_ = s; }
      int next() { seed_ = seed_ * 0x343fdu + 0x269ec3u; return int((seed_ >> 16) & 0x7fff); }
      uint32_t state() const { return seed_; }
  private:
      uint32_t seed_;
  };

  using ControlMask = uint32_t;                          // bits from the table at 0x00476c00
  struct KeyBindings { std::array<uint8_t,256> action_of_key{}; };   // 0x004c5d90: virtual key -> action code
  ControlMask controls_from_keys(const KeyBindings&, const std::array<bool,256>& held);  // 0x0044fa80

  enum class ObjectKind : uint8_t { player, rider, car, cop, pedestrian, scenery, other };  // refined in Task 4
  struct Object {
      int32_t next_tick = 0, last_tick = 0, interval = 1, delta = 0;   // +0x0c +0x10 +0x14 +0x18
      int32_t position = 0;                                          // +0x1c, Q8 samples
      uint32_t flags = 0;                                            // +0xc4
      ObjectKind kind = ObjectKind::other;
      // Behaviour hooks: ports of the +0xe0/+0xe4/+0xe8 targets; set by the owning subsystem.
      void (*think)(struct World&, Object&) = nullptr;
      void (*move)(struct World&, Object&) = nullptr;
      void (*slice_change)(struct World&, Object&) = nullptr;
  };
  struct World {
      int32_t tick = 0;                  // 0x004b7ecc
      int32_t ring_position = 0;         // 0x004b6e5c
      int32_t horizon_heading = 0;       // 0x004b7ef8
      Random random;
      std::vector<Object> objects;       // kept sorted by position, descending, like 0x0046dcc8
      void step();                       // 0x0044b3b0
  };
  void update_scheduled(World&);         // 0x00438170 + 0x00437e60 scheduling (interval rules from 0x00437e60)
  }
  ```

- [ ] **Step 1: Failing tests**

`tests/random_test.cpp`:
```cpp
#include "random.hpp"
#include <iostream>
int main() {
    rr::Random r(1);
    // MSVC rand() with default seed 1 produces 41, 18467, 6334, 26500, 19169.
    const int expected[] = {41, 18467, 6334, 26500, 19169};
    int failures = 0;
    for (int e : expected) if (r.next() != e) ++failures;
    rr::Random wrap(0xffffffffu);
    wrap.next();   // must not trap on overflow; unsigned wraparound
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
```

`tests/controls_test.cpp`: with a synthetic `KeyBindings` (key 0x26 → action A, 0x28 → action B), holding both returns the OR of both masks, as `0x0044fa80` ORs every matching action. An unbound key contributes nothing.

`tests/world_test.cpp`:
- An object with interval 3 is updated on ticks 0, 3, 6 when the camera is near.
- The interval changes with distance per the `0x00437e60` rule: `d = (pos − ring)/256` truncated; `d < 4` gives 1, except `d < −2` gives `1 − d`; otherwise `(d+1)/2`. Assert 1 for d=0, 3 for d=−2… as documented in Step 2.
- Advancing `tick` from `INT32_MAX − 2` across the wrap keeps scheduling correct in 32-bit signed arithmetic, as the original does. Wraparound itself is the expected behaviour.

- [ ] **Step 2: Port**

Port `0x00437e60` interval rules and `0x00438170`'s four distance bands (`+0x1f00`, `+0xf00`, `−0x400`, rest) exactly. Port `0x0044b3b0`'s timers (`0x00475610` period 2, `0x00475614` period 3, `0x004b7f04` bits) and the horizon-heading accumulation.

- [ ] **Step 3: Pass, commit**

Commit: `sim: RNG, controls mapping and object scheduler`.

---

### Task 4: Bike specs and the player object

**Files:**
- Create: `src/bike_spec.hpp`, `src/bike_spec.cpp`, `src/rider.hpp`, `src/rider.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct BikeSpec { /* 356-byte SPEC record fields, named in Step 1 */ };
  std::vector<BikeSpec> read_bike_specs(const std::vector<uint8_t>& bikespec_rsc);   // 15 records, 0x00405e30
  struct RiderState { /* fields from the player struct at *0x004642d8 and its +0x2fc sub-object */ };
  void apply_spec(RiderState&, const BikeSpec&, int32_t mass_half, uint8_t variant);  // 0x00405ea0(p, spec+0xc / 2, spec+5)
  void player_think(World&, Object&);   // 0x00407d40
  }
  ```

- [ ] **Step 1: Name the fields by correlation**

Using `napa-solo` `obj.*.raw.*` and `player.*`, find the fields that change with throttle (speed), steering (lateral position and velocity), crash (state flags) and slope (vertical). Read `0x00405ea0`, `0x00407d40` and the move/slice functions it installs. Record a full field table in `REVERSE_ENGINEERING.md`: offset, meaning, units, evidence.

- [ ] **Step 2: Failing test**

Add `tests/oracle_sim_test.cpp`:
1. Load `napa-solo.tsv`.
2. Build the native `World` from the race setup of Task 6, or temporarily from the first tick's dumped state via `RiderState from_dump(const golden frame&)`. The helper must be removed when Task 6 lands.
3. Feed each tick the dumped `controls`.
4. Compare the named fields every tick. Print the first 20 differences with tick number and field.

It skips with 77 when the golden is absent.

- [ ] **Step 3: Port**

Port `apply_spec` and `player_think` (plus the functions it calls for acceleration, braking, steering, gear/RPM and slope) line by line until the named fields match on every tick of `napa-solo` before the first crash.

- [ ] **Step 4: Commit**

`sim: bike specs and player riding physics`.

---

### Task 5: Collisions, crash and remount

**Files:**
- Create: `src/collision.hpp`, `src/collision.cpp`
- Modify: `src/rider.cpp`

**Interfaces:**
- Consumes: `rr::RouteSample` (road edges and shoulders), `rr::TerrainSample` (plan 02), `rr::SceneryObject` (plan 03)
- Produces: `bool collide_rider(World&, Object& rider, const std::vector<RouteSample>& route)`, called from the rider's move function at the point where the original calls its collision check.

- [ ] **Step 1:** Trace the collision calls reached from the player's move/slice functions. Classify them: road edge, terrain wall, scenery object, vehicle (vehicles come in plan 05). Document.
- [ ] **Step 2:** Extend `oracle_sim_test` to the whole of `napa-solo` (including the crash, run-back and remount) and `napa-mash`. Watch it fail after the crash tick.
- [ ] **Step 3:** Port until both pass.
- [ ] **Step 4:** Commit: `sim: scenery and edge collisions, crash and remount`.

---

### Task 6: Race setup, branch choice and camera

**Files:**
- Create: `src/race.cpp`, `src/race_camera.hpp`, `src/race_camera.cpp`
- Modify: `src/course.cpp`: `make_route`'s committed-rider assumption becomes a runtime decision driven by the rider's lateral position (`0x00439390`), exposed as `class Traversal` (port of `0x00453030` state), replacing the precomputed windows for the live game. Keep `make_route` for the viewer and the existing tests.

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct RaceConfig { int course; int level; int bike; uint32_t seed; };
  struct Race { World world; /* traversal, camera, route state */ };
  Race start_race(const Assets&, const RaceConfig&);   // 0x004163d0 preamble, 0x004552e0 ring setup
  void run_steps(Race&, int steps, const std::vector<ControlMask>& per_step_controls);
  Camera race_camera(const Race&);                     // 0x00438450 / 0x00437d80
  }
  ```

- [ ] **Step 1:** Remove the temporary `from_dump` helper. `oracle_sim_test` now starts from `start_race` with the golden's course and seed, and must match from tick 0.
- [ ] **Step 2:** Port `Traversal` (the live version of `0x00453030` including collapse on commit) and the commit rule `0x00439390`. `napa-split-ride` must match every tick, including `camera.*`.
- [ ] **Step 3:** Add a native-versus-oracle projection check: for every golden frame, `project_road` fed by `race_camera(race)` equals the dumped slices (reuse the plan 01 `oracle_road_test` comparison code; move it to a shared `tests/road_compare.hpp`).
- [ ] **Step 4:** Commit: `race: setup, live branch traversal and camera`.

---

### Task 7: Playable solo ride

**Files:**
- Create: `src/game_main.cpp` (new executable `rr-game`; desktop and Android `main`)
- Modify: `CMakeLists.txt`: Android now builds `rr-game` as `libmain.so`; the research viewer stays desktop-only as `rr-viewer`.

**Interfaces:**
- Consumes: `start_race`, `run_steps`, `race_camera`, `build_scene`/`draw_world` (plan 02), `rr::Framebuffer`
- Produces: `rr-game --assets DIR --course N --seed S [--replay FILE] [--record FILE] [--hz 30|60|144]`

- [ ] **Step 1: Frame-rate independence test**

`tests/replay_hz_test.cpp` runs the same recorded controls through the fixed-step accumulator at 30, 60 and 144 simulated display Hz, using a virtual clock with no SDL, and requires identical final `World` state (compare with `operator==` on a serialised snapshot).

- [ ] **Step 2: Implement the main loop**

The loop must:
- accumulate real time
- run `ticks_per_second`-rate steps, capped at `max_steps_per_frame`
- read controls from SDL keyboard state via `KeyBindings` (defaults from the original's default bindings table)
- draw with `draw_world`

- [ ] **Step 3: Manual check**

Ride Napa end to end on macOS. Record controls with `--record`, replay with `--replay`, and confirm the final position is identical.

- [ ] **Step 4: Docs and commit**

Update docs (PROGRESS: "Solo ride playable"). Commit: `game: playable solo ride with original physics`.
