# All Courses and Levels Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires plans 10 and 11.

> **Superseded (October 11, 2026, session 26):** use [17-all-courses-port.md](17-all-courses-port.md). Every task below is folded into plan 17 (Task 1 → 17 Tasks 1–2, Task 2 → 17 Task 3, Task 3 → 17 Tasks 4–7, Task 4 → 17 Tasks 4–7 and nitro in 17 Task 6, Task 5 → 17 Task 9). Kept as history only.

**Goal:** Extend the verified Napa pipeline to CITY, HIWAY (Sierra Nevada), MEDLY (Peninsula) and CANYN (Pacific Coast Highway) at all five levels, and delete the placeholder race renderer.

**Architecture:** `rr::CourseScene` is already parameterised by course name and level, but `game_main.cpp` only constructs it for `NAPA`. This plan:
- removes that special case;
- resolves each course's horizon, materials, families (MSRR requests) and terrain types;
- fixes whatever the per-course oracle captures expose.

Behaviour that varies by course or level (traffic density, police presence, opponent field and rubber band) is accepted with plan 10 tick goldens per course.

**Tech Stack:** C++17, SDL3, CTest, Wine oracle.

**Spec:** [00-INDEX.md](00-INDEX.md), definition of done item 2.

## Global Constraints

- Original course order (`0x0046df58`): 1 CITY, 2 HIWAY, 3 MEDLY, 4 NAPA, 5 CANYN. Horizon order (`0x00467aa0`): City, Sierra, Penin, Napa, PCH. `RR_ORACLE_COURSE` takes the original index.
- No course-specific branches in native code unless the original has the same branch (cite its address).
- Each course must pass the same checks Napa passes:
  - the full-route geometry and raster sweep;
  - natural-frame pixel goldens;
  - tick goldens;
  - a bounded autopilot finish.

## Review Focus

1. **CITY's negative hill boundaries and inverted initial interval, and HIWAY/MEDLY's out-of-order wall boundaries** (session 13): the scene must traverse them without sorting. Task 2's sweep must include those sections; the session 13 tests list them.
2. **Courses with more splits:** both branch routes must be swept for every course, not just route A.
3. **Dense traffic on CITY:** pool exhaustion (16 objects) must behave as the original does. Task 3's `city-rush` tick golden covers it.
4. **Level 5 speeds:** at the highest AI throttle and rubber band, riders reach positions where `uint32_t` position wraparound and the depth table's far end matter. `*-field-l5` scenarios exist for every course.
5. **Missing optional course resources:** if a course lacks a resource the original treats as optional, the game must behave as the original does, not crash. Task 1 lists every optional resource and its fallback.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/course_scene.cpp` | Per-course resolution (horizon, families, terrain types) |
| `src/game_main.cpp` | Remove the `course_name=="NAPA"` special case and the placeholder renderer path |
| `src/road_draw.*`, `src/viewer.cpp` | Delete placeholder shading once no caller remains (viewer switches to `CourseScene`) |
| `tests/course_scene_test.cpp` | Sweep all five courses on both routes |
| `docs/SIM_SCENARIOS.md` | Per-course scenarios |

---

### Task 1: Per-course resource census

**Files:**
- Modify: `docs/REVERSE_ENGINEERING.md`, `tests/course_scene_test.cpp`

- [ ] **Step 1: Write the failing test.** For each of the five courses, `course_scene_test` must:
  - construct `CourseScene(assets, exe, course, name, 1, level)` for levels 0..4;
  - resolve every MSRR family request;
  - resolve every hill, building and scenery material over every sample of both routes.

  Register it so that it skips (77) without assets. Expected: failures on non-Napa courses (unresolved materials or throws).
- [ ] **Step 2: Trace and fix the resolution.** Use `0x00441940` (course load), `0x00416310` (horizon/dashboard selection) and the MSRR loader. Record per course: horizon file, family set, terrain types used, and any optional resources with the original's fallback.
- [ ] **Step 3:** The test passes for all five courses and both routes. Commit: `scene: resolve resources for all five courses`.

---

### Task 2: Geometry and raster sweep on every course

**Files:**
- Modify: `tests/course_scene_test.cpp`, `src/course_scene.cpp` and the panel modules as needed

- [ ] **Step 1:** Extend the Napa sweep (every 5th sample, both routes, full raster) to all courses. Run it and record failures: raster errors, unresolved frames, throws.
- [ ] **Step 2:** Fix each failure from its original function, keeping the session 22 guard rules. Reads beyond imported storage stay zero-guarded and documented. Never clamp coordinates.
- [ ] **Step 3:** Accept: all routes of all courses render without errors. Record view and node counts per course in VERIFICATION.md. Commit: `scene: full-route sweep passes on all courses`.

---

### Task 3: Natural-frame pixel goldens per course

**Files:**
- Modify: `tests/oracle_scene_test.cpp` (scenario list), `docs/SIM_SCENARIOS.md`

- [ ] **Step 1:** For each course, capture `<course>-drive`: drive=2, level 1, `RR_ORACLE_PIXELS=1`, every 30 frames, about 3,000 frames, through at least one split. CITY can reuse `city-moving` if it covers a split. Use the plan 10 control record so later tick goldens share inputs.
- [ ] **Step 2:** Run `oracle-scene-test --native-scene --native-actors` on each. Record and fix the first differing node per course. Common expected sources are terrain types not used by Napa (mountain walls, type 1) and course-specific building materials.
- [ ] **Step 3:** Accept: every captured frame of every course matches every pixel. Commit per course, e.g. `scene: CITY natural frames match original`.

---

### Task 4: Tick goldens per course and level

**Files:**
- Modify: `docs/SIM_SCENARIOS.md`, `src/race.cpp`, `src/traffic.cpp`, `src/police.cpp` as divergences require

- [ ] **Step 1:** Capture `<course>-field-l1` and `<course>-field-l5` for all five courses (opponents, traffic and police on), plus `city-rush`, a dense-traffic stretch of CITY. Also capture `hiway-nitro`: a nitro-equipped bike with the nitro control held, which covers `0x0040b110`/`0x0040b170` and sound events 14/21 (nitro is not ported yet). That is 12 scenarios.
- [ ] **Step 2:** Run `oracle-sim` on all of them and fix the divergences: per-course traffic density (`0x0044db80` density argument), level-dependent police, opponent field composition. The PROGRESS.md "Race field" assumption (always personalities 1..7, 15-row standings) must be replaced by the traced rule.
- [ ] **Step 3:** Port nitro for `hiway-nitro`. Accept: all 12 pass. Commit: `sim: nitro; all courses and levels match original tick traces`.

---

### Task 5: Switch every course to the original renderer and delete the placeholder

**Files:**
- Modify: `src/game_main.cpp`, `src/viewer.cpp`
- Delete: placeholder-only code in `src/road_draw.*` (keep anything still used by tools; check with `grep -rn` before deleting)

- [ ] **Step 1:** Remove the Napa-only branch. Every course builds `CourseScene`, `VehicleLibrary`/`TrafficSystem`, `Police` and the HUD through `make_race_setup`.
- [ ] **Step 2:** Bounded autopilot finish on every course: `build/rr-game --assets ../cdrom/ROADRASH --race --course <n> --ticks 60000 --autopilot --screenshot analysis/<course>-finish.bmp` exits 0 with `finished`. Record the finishing times in VERIFICATION.md.
- [ ] **Step 3:** Run the regression gate and `tools/ci.sh`. Commit: `game: original renderer on all courses; remove placeholder road shading`.

## Exit criteria

- All five courses × five levels are playable through Thrash and Big Game, with no placeholder renderer.
- All Task 3 and Task 4 goldens pass, and PROGRESS.md shows every course as oracle-verified, with the scenario names.
