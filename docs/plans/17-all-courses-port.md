# All Courses Port Implementation Plan (CITY, HIWAY, MEDLY, CANYN; levels 1–5)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. **Supersedes [12-all-courses.md](12-all-courses.md)**; plan 12's tasks are folded in here and plan 12 stays only as history. Tasks 1–3 can start as soon as Napa's scene pipeline is stable (plan 11 Task 3); Tasks 4–8 need plan 11's acceptance (`napa-acceptance`) so per-course divergences are course data, not shared bugs.

**Goal:** Port the four remaining courses (CITY, HIWAY = Sierra Nevada, MEDLY = Peninsula, CANYN = Pacific Coast Highway) at all five levels, to the same standard as Napa, and delete the placeholder race renderer. Every course becomes **playable early** (Task 2, marked "unverified" in the game's docs) so the owner can play all maps while verification proceeds course by course.

**Architecture:**
- `rr::CourseScene` is already parameterised by course and level, but `Game::load_course` (`src/game_main.cpp`) builds the scene, HUD, actors and crash config only for `NAPA`; every other course falls back to `rr::draw_road`. Task 2 removes the special case and moves course setup into one function, `make_race_setup`, used by the game, the tests and the oracle replay.
- Per-course and per-level behaviour (traffic density, cops, opponent field, race start/finish, scripted vehicles) comes from tables in the user's EXE and course files. Native code has **no course-specific branches** unless the original has the same branch (cite the address).
- Each course has its own checklist in a new tracker, `docs/COURSES_IMPLEMENTATION.md`, modelled on `NAPA_IMPLEMENTATION.md`.

**Tech Stack:** C++17, SDL3, CTest, Wine oracle, plan 10 tick goldens, scene dump v3.

**Spec:** the owner's request of October 11, 2026 ("complete Napa, then port all remaining maps"), [00-INDEX.md](00-INDEX.md) definition of done item 2.

## Global Constraints

- **Course order.** Original order (`0x0046df58`): 1 CITY, 2 HIWAY, 3 MEDLY, 4 NAPA, 5 CANYN. Horizon order (`0x00467aa0`): City, Sierra, Penin, Napa, PCH. `RR_ORACLE_COURSE` takes the original index. `rr-game --course` takes the CLI order NAPA 0, CITY 1, HIWAY 2, CANYN 3, MEDLY 4. Always convert explicitly; Task 2 adds `rr::original_course_index(cli)` and every new call site uses it.
- **Same bar as Napa.** Each course must pass: the full-route geometry and raster sweep on both routes; resource resolution at all five levels; natural-frame pixel goldens; tick goldens for the field at levels 1 and 5; a bounded autopilot finish; the soak/determinism test (plan 16 Task 7).
- **Course order of work:** CITY → HIWAY → MEDLY → CANYN. CITY first because it is the first course of the original's order and the most different from Napa (dense traffic, buildings both sides, negative hill boundaries); HIWAY next because it brings mountain terrain and nitro bikes.
- The port rule, data rule and evidence rule of 00-INDEX apply to every task.

## Review Focus

1. **CITY's negative hill boundaries and inverted initial interval, HIWAY/MEDLY's out-of-order wall boundaries** (session 13): the scene must traverse them without sorting or clamping. Task 3's sweep lists those sections explicitly.
2. **Courses with more splits:** every branch route of every course is swept and driven, not just route A. Task 3 enumerates routes from the course graph, and Task 4–7's captures include one run per branch.
3. **Dense traffic exhausting the 16-object pool** on CITY: spawning must stop and resume exactly as the original does. Task 4's `city-rush` golden.
4. **Level 5 speeds** reaching positions where `uint32_t` wrap and the depth table's far end matter: `<course>-field-l5` exists for every course.
5. **Per-level race start and finish:** levels use different race lengths (`0x00468f58` course lengths differ by level). A race must start and end on the original's nodes for each level, not run the whole route. Task 1 traces this and Task 8 tests all 25 combinations.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/race_setup.hpp/.cpp` | `make_race_setup` (plan 10's `RaceConfig` interface, extended): everything a race needs, moved out of `game_main.cpp` |
| `src/course_scene.cpp` + panel modules | Per-course resolution (horizon, families, terrain types, materials) |
| `src/course.cpp`/`course.hpp` | Per-level start/finish nodes; route enumeration for every branch |
| `src/traffic.cpp`, `src/police.cpp`, `src/race.cpp` | Per-course/level parameters from EXE tables; nitro |
| `src/game_main.cpp`, `src/viewer.cpp`, `src/road_draw.*` | Remove the Napa branch and the placeholder renderer |
| `tests/course_scene_test.cpp`, `tests/oracle_scene_test.cpp`, `tests/oracle_sim_test.cpp`, `tests/race_setup_test.cpp`, `tests/race_soak_test.cpp` | Tests |
| `docs/COURSES_IMPLEMENTATION.md` | Per-course acceptance checklist |
| `docs/SIM_SCENARIOS.md` | Per-course scenarios |

---

### Task 1: Course census, per-level race bounds and the tracker

**Files:**
- Create: `docs/COURSES_IMPLEMENTATION.md`
- Modify: `docs/REVERSE_ENGINEERING.md`, `src/course.hpp`, `src/course.cpp`, `tests/course_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct RaceBounds { uint32_t start_position_q8 = 0, finish_position_q8 = 0; int route_hint = 0; };
  // Original per-course/per-level start and finish (trace in Step 1). Throws std::runtime_error if the tables are out of range.
  RaceBounds race_bounds(const std::vector<uint8_t>& exe, const Course&, int original_course_index, int level);
  int original_course_index(int cli_course);   // NAPA 0→4, CITY 1→1, HIWAY 2→2, CANYN 3→5, MEDLY 4→3
  }
  ```

- [ ] **Step 1: Trace** how the original places the start grid and the finish for each level: course load `0x00441940`, race start `0x004160c0`, finish `0x00416010`, the course-length table `0x00468f58` (tenths of miles, `[course*5 + level]`). Record per course and level: start node/position, finish node/position, and whether the route is the same with a different finish or a different section of the course. PROGRESS.md's assumption "the race runs the whole route to the course end node" is replaced by this finding.
- [ ] **Step 2: Census.** For each course, list in REVERSE_ENGINEERING.md: horizon file, MSRR family set, terrain types used (Napa uses type 0 sides; mountains are type 1), materials, the `.CAR` vehicle count and masks, DZHR scripted vehicle records (if any), police presence per level (from `PoliceConfig`), and every resource the original treats as optional with its fallback.
- [ ] **Step 3: Write the failing test.** In `tests/course_test.cpp`: `original_course_index` maps all five CLI values as above and throws on 5 and −1; with the disc, `race_bounds` for all 25 combinations gives `start < finish` on the route it names, and the distance between them, converted with the original's units, equals `0x00468f58[course*5 + level]` tenths of a mile within the original's own rounding (state the rounding found in Step 1). Run; expected FAIL.
- [ ] **Step 4: Implement** `race_bounds` and `original_course_index`. Run; expected PASS.
- [ ] **Step 5: Create the tracker** `docs/COURSES_IMPLEMENTATION.md` with one section per course (CITY, HIWAY, MEDLY, CANYN) and the same checklist for each:
  ```markdown
  - [ ] Resources resolve at all five levels (course-scene-test)
  - [ ] Full-route sweep, every branch (course-scene-test)
  - [ ] Natural-frame pixel goldens: <course>-drive (oracle-scene)
  - [ ] Tick goldens: <course>-solo, <course>-field-l1, <course>-field-l5 (oracle-sim)
  - [ ] Traffic and scripted vehicles match (oracle-sim)
  - [ ] Police match at the levels where they appear (oracle-sim)
  - [ ] Bounded autopilot finish at all five levels (race-soak)
  - [ ] Owner playtest notes addressed
  ```
- [ ] **Step 6:** Commit: `course: per-level race bounds, course census and tracker`.

---

### Task 2: One race setup for every course (all maps playable)

**Files:**
- Create: `src/race_setup.hpp`, `src/race_setup.cpp`, `tests/race_setup_test.cpp`
- Modify: `src/game_main.cpp` (`Game::load_course`, `Game::restart` call `make_race_setup`), `tests/oracle_sim_test.cpp` (replace its own race construction with `make_race_setup`, so the test and the game cannot build different races), `CMakeLists.txt`

**Interfaces:**
- Consumes: `CourseScene`, `Hud`, `ActorNodes`, `VehicleLibrary`/`TrafficSystem`, `read_crash_config`, `read_police_config`, `original_opponents`/`read_personalities`, `race_bounds` (Task 1).
- Produces (plan 10 Task 4 specified this factory but it was never extracted: `src/race_setup.*` does not exist and `oracle_sim_test` builds its own race. Create it here with plan 10's names, extended; if another session has created it by then, extend that one instead of adding a second):
  ```cpp
  namespace rr {
  struct RaceConfig {
      std::string course;            // original file name: "CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"
      int level = 0, bike = 0, character = 0;
      bool career = true;
      int opponents = 7;             // 0..7, for quick races and solo tests
      bool high_resolution = true;   // Settings::high_resolution (plan 18 Task 4)
      bool metric = false;
  };
  struct RaceSetup {
      Course course;
      std::unique_ptr<CourseScene> scene;
      std::unique_ptr<Hud> hud;
      std::unique_ptr<ActorNodes> actors;
      std::unique_ptr<VehicleLibrary> vehicles;
      std::unique_ptr<TrafficSystem> traffic;
      CrashConfig crash;
      PoliceConfig police;
      RaceBounds bounds;
      std::unique_ptr<Race> race;
  };
  // Builds everything for one race. Throws std::runtime_error naming the resource on failure;
  // the caller shows it and returns to the menu (never a crash).
  RaceSetup make_race_setup(const Assets&, const std::vector<uint8_t>& exe, const RaceConfig&);
  }
  ```

- [ ] **Step 1: Write the failing test** `tests/race_setup_test.cpp` (skips 77 without the disc): for every course name and level 0..4, `make_race_setup` returns a setup with a non-null scene, HUD, actors and race; traffic is non-null when `DATA/CARS/<name>.CAR` exists; the race's player starts at `bounds.start_position_q8`. Then step each race 600 ticks with zero controls and build one frame's draw nodes; no throw. Run; expected FAIL (missing header).
- [ ] **Step 2: Implement** `make_race_setup` by moving the body of `Game::load_course`/`restart` into it without the `course_name=="NAPA"` condition. The race starts and finishes at `race_bounds`. Unresolved resources throw with the resource name.
- [ ] **Step 3:** Run the test. Every failure is a course-data gap: record each (course, level, resource or function) in COURSES_IMPLEMENTATION.md, fix the resolution in `course_scene.cpp` or the panel module from its original function, and re-run until the test passes. Do **not** clamp coordinates or skip nodes to pass; reads beyond imported storage stay zero-guarded as in session 22, and each guard is documented.
- [ ] **Step 4: Play check.** `build/rr-game --assets ../cdrom/ROADRASH --race --course <n> --level 1 --ticks 1800 --autopilot --screenshot analysis/<course>-l1.bmp` for n = 1..4 exits 0 and the screenshot shows the original-style road, scenery and dashboard. In-game and in USER_GUIDE, note that these courses are "playable, not yet verified".
- [ ] **Step 5:** Run the regression gate. Commit: `game: original renderer and race setup on all courses`.

---

### Task 3: Geometry and raster sweep on every course and branch

**Files:**
- Modify: `tests/course_scene_test.cpp`, `src/course_scene.cpp` and panel modules as failures require, `docs/VERIFICATION.md`

- [ ] **Step 1: Write the failing test.** Extend the Napa sweep (every 5th sample, full raster) to all five courses, enumerating **every** branch route from the course graph (Review Focus 2), at level 0 and level 4. Explicitly include the session 13 sections: CITY's negative hill boundaries and inverted initial interval, HIWAY/MEDLY's out-of-order wall boundaries (Review Focus 1); list their sample ranges in the test with a comment citing session 13. Run; record each failure (raster error, unresolved frame, throw).
- [ ] **Step 2: Fix** each failure from its original function.
- [ ] **Step 3: Accept.** All routes of all courses render without errors. Record view and node counts per course and route in VERIFICATION.md. Commit: `scene: full-route sweep passes on every course and branch`.

---

### Task 4: CITY to original parity

**Files:**
- Modify: `docs/SIM_SCENARIOS.md`, `tests/oracle_scene_test.cpp`, `tests/oracle_sim_test.cpp` (scenario lists), source modules as divergences require, `docs/COURSES_IMPLEMENTATION.md`

- [ ] **Step 1: Capture scene goldens.** `city-drive`: original course 1, level 1, `RR_ORACLE_DRIVE=2`, `RR_ORACLE_PIXELS=1`, `RR_ORACLE_SHOT_EVERY=30`, 3,000 frames through at least one split, with `RR_ORACLE_RECORD` writing `city-drive.controls`. Add the exact command to SIM_SCENARIOS.md:
  ```sh
  RR_ORACLE_COURSE=1 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_PIXELS=1 RR_ORACLE_SHOT_EVERY=30 RR_ORACLE_RECORD='C:\city-drive.controls' RR_ORACLE_FRAMES=3000 oracle/run.sh city-drive
  ```
- [ ] **Step 2: Capture tick goldens** (single anchor at green): `city-solo` (`RR_ORACLE_SOLO=1`, the `city-drive.controls` input), `city-field-l1`, `city-field-l5`, and `city-rush`, a dense-traffic stretch where the 16-object pool fills (Review Focus 3). One branch-B run: `city-solo-split-b`.
- [ ] **Step 3: Write the failing tests.** Register the five scenarios. Run `oracle-scene-test --native-scene --native-actors city-drive` and `oracle-sim-test` on each; record the first divergence per scenario in COURSES_IMPLEMENTATION.md.
- [ ] **Step 4: Fix** each divergence in the owning module from its original function (expected: building materials, traffic density from the `0x0044db80` density argument, per-course police, field composition).
- [ ] **Step 5: Accept.** All CITY scenarios pass; CITY's checklist in COURSES_IMPLEMENTATION.md is ticked with evidence lines. Extend `race-soak` (plan 16 Task 7) to CITY. Commit: `city: scene and simulation match original`.

---

### Task 5: HIWAY (Sierra Nevada) to original parity

**Files:**
- Modify: `docs/SIM_SCENARIOS.md`, `tests/oracle_scene_test.cpp`, `tests/oracle_sim_test.cpp` (scenario lists), `tests/race_soak_test.cpp`, source modules as divergences require (most likely `src/course_scene.cpp`, `src/terrain_panels.cpp`, `src/building_panels.cpp`, `src/traffic.cpp`, `src/police.cpp`), `docs/COURSES_IMPLEMENTATION.md`

- [ ] **Step 1: Capture scene goldens** `hiway-drive` (original course 2), same parameters:
  ```sh
  RR_ORACLE_COURSE=2 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_PIXELS=1 RR_ORACLE_SHOT_EVERY=30 RR_ORACLE_RECORD='C:\hiway-drive.controls' RR_ORACLE_FRAMES=3000 oracle/run.sh hiway-drive
  ```
  Pick shot frames that include mountain walls (terrain type 1) and snow/rock materials not used by Napa.
- [ ] **Step 2: Capture tick goldens:** `hiway-solo`, `hiway-field-l1`, `hiway-field-l5`, `hiway-solo-split-b`.
- [ ] **Step 3: Write the failing tests.** Register the four scenarios in `oracle-sim-test` and `hiway-drive` in `oracle-scene-test`. Run `oracle-scene-test --native-scene --native-actors hiway-drive` and `oracle-sim-test` on each; record the first divergence per scenario in COURSES_IMPLEMENTATION.md.
- [ ] **Step 4: Fix** divergences. Expected new work: terrain type 1 sides in `terrain_panels.cpp`, wall boundaries out of order (Review Focus 1).
- [ ] **Step 5: Accept,** tick the checklist, extend `race-soak`, commit: `hiway: scene and simulation match original`.

---

### Task 6: Nitro

Nitro bikes (the Super class) can be bought from level 3 and are first likely used on HIWAY's long straights. This was plan 12 Task 4 Step 3.

**Files:**
- Modify: `src/bike.cpp`, `src/race.cpp`, `src/input.cpp` (nitro control bit), `src/sound.cpp` (events 14/21), `src/hud.cpp` (nitro count if the dashboard shows it), `tests/bike_test.cpp`, docs

- [ ] **Step 1: Trace** `0x0040b110`/`0x0040b170`: the control bit, the charge count per bike spec, the boost law, the sound events 14 and 21, and any dashboard indicator.
- [ ] **Step 2: Capture** `hiway-nitro`: a nitro bike (`RR_ORACLE_BIKE` if the oracle supports it; otherwise add it to `aweman_proxy.c` the same way `RR_ORACLE_COURSE` works) with nitro pressed on three straights.
- [ ] **Step 3: Write the failing tests:** the scenario in `oracle-sim-test`; a synthetic `bike_test` that the charge count never goes negative when nitro is held for 1,000 ticks.
- [ ] **Step 4: Port**, then remove "nitro" from PROGRESS.md "Not ported yet".
- [ ] **Step 5: Accept** and commit: `sim: nitro boost`.

---

### Task 7: MEDLY (Peninsula) and CANYN (Pacific Coast Highway) to original parity

Two courses, one task each in practice; do them in this order and commit separately.

**Files:**
- Modify: `docs/SIM_SCENARIOS.md`, `tests/oracle_scene_test.cpp`, `tests/oracle_sim_test.cpp` (scenario lists), `tests/race_soak_test.cpp`, source modules as divergences require (most likely `src/course_scene.cpp`, `src/terrain_panels.cpp`, `src/building_panels.cpp`, `src/traffic.cpp`, `src/police.cpp`), `docs/COURSES_IMPLEMENTATION.md`

- [ ] **Step 1 (MEDLY): Capture** with `RR_ORACLE_COURSE=3`:
  ```sh
  RR_ORACLE_COURSE=3 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_PIXELS=1 RR_ORACLE_SHOT_EVERY=30 RR_ORACLE_RECORD='C:\medly-drive.controls' RR_ORACLE_FRAMES=3000 oracle/run.sh medly-drive
  ```
  then `medly-solo`, `medly-field-l1`, `medly-field-l5`, `medly-solo-split-b`. Register, run, fix, accept. Tick MEDLY in COURSES_IMPLEMENTATION.md, extend `race-soak`. Commit: `medly: scene and simulation match original`.
- [ ] **Step 2 (CANYN): Capture** with `RR_ORACLE_COURSE=5`:
  ```sh
  RR_ORACLE_COURSE=5 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_PIXELS=1 RR_ORACLE_SHOT_EVERY=30 RR_ORACLE_RECORD='C:\canyn-drive.controls' RR_ORACLE_FRAMES=3000 oracle/run.sh canyn-drive
  ```
  then `canyn-solo`, `canyn-field-l1`, `canyn-field-l5`, `canyn-solo-split-b`. Expected new work: coastal cliff/drop terrain sides and the PCH horizon. Register, run, fix, accept. Tick CANYN, extend `race-soak`. Commit: `canyn: scene and simulation match original`.

---

### Task 8: All 25 course × level races, and the career

**Files:**
- Modify: `tests/race_soak_test.cpp`, `tests/career_test.cpp`, `docs/VERIFICATION.md`

- [ ] **Step 1: Write the test.** `race-soak` runs all five courses × five levels: each race starts at `race_bounds.start`, the autopilot finishes, the finish happens at `race_bounds.finish` (Review Focus 5), and the run is deterministic. Record the 25 finishing times and hashes in VERIFICATION.md.
- [ ] **Step 2: Career test.** Extend `career_test`: a scripted Big Game career, with race results injected as "won first place" for each race, qualifies on all five courses at a level, advances through the level-up screen, and reaches the finale at level 5. The course order and screens visited follow the original (`0x00421df0`). Use the frontend scripted flow, not the race, so the test is fast.
- [ ] **Step 3:** Commit: `test: every course and level finishes; career reaches the finale`.

---

### Task 9: Remove the placeholder renderer

**Files:**
- Modify: `src/game_main.cpp`, `src/viewer.cpp`
- Delete: placeholder-only code in `src/road_draw.*` and the `rr::draw_road` fallback in `game_main.cpp`

- [ ] **Step 1:** `grep -rn "draw_road\|road_draw" src tests tools` and list every caller. Switch the viewer (`rr-viewer`) to `CourseScene`; keep anything a tool still needs.
- [ ] **Step 2:** Delete the placeholder code. Run the regression gate and `tools/ci.sh`.
- [ ] **Step 3:** Delete the "Shading" assumption from PROGRESS.md. Commit: `game: remove placeholder road shading`.

## Exit criteria

- All five courses × five levels are playable through Thrash and Big Game, with no placeholder renderer.
- Every COURSES_IMPLEMENTATION.md checkbox is ticked with evidence. All Task 4–8 goldens pass with a single RNG anchor.
- `race-soak` covers 25 course × level combinations and passes in Debug, UBSan and Release trees.
