# Opening Napa Race Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires plan 10 Tasks 1–4. Tasks 4–7 use plan 10's tick goldens for acceptance.

**Goal:** Finish the opening Napa race (level 1, Big Game start) so that every system in [../NAPA_IMPLEMENTATION.md](../NAPA_IMPLEMENTATION.md) is ported and measured against the original. That covers scene, scenery, riders, traffic, police, combat, crashes, sound and music, dashboard and results. The work is done in the shared native game for macOS and Android.

**Architecture:** The race keeps its current structure:
- `rr::Race` owns riders and steps them;
- `rr::TrafficSystem` owns cars;
- `rr::CourseScene` builds the static display list;
- the game composes the frame.

Two new modules close the remaining gaps:
- `rr::ActorAnimation`: the RRAN player (`0x0040ac70` set, `0x0043ae90` advance) for riders, cops and cars;
- `rr::ActorNodes`: builds rider, bike, shadow, car and cop display nodes and merges them into the scene list in the original's order. This replaces drawing traffic and riders as separate passes after the scene.

Police live in `src/police.*`, driven from `Race::step` at the original point in the tick.

**Tech Stack:** C++17, SDL3, CTest, oracle (`oracle/aweman_proxy.c`), plan 10 tick goldens, existing scene dump v3 (`tests/scene_dump.hpp`).

**Spec:** [../NAPA_IMPLEMENTATION.md](../NAPA_IMPLEMENTATION.md) (acceptance list), [00-INDEX.md](00-INDEX.md).

## Global Constraints

- Napa is original course index 4 (`0x0046df58`) and `rr-game` CLI course 0. Level 1 is `level = 0` in `Race` (`0x004b8a12`).
- Every behaviour change is accepted by a plan 10 tick golden, and every drawing change by an original display-list or pixel golden. Synthetic tests cover boundaries only.
- Original game data stays outside source, tests and the APK. Goldens live in `analysis/oracle/`.
- A stand-in listed in PROGRESS.md "Assumptions" may be removed only together with the evidence that replaces it.
- Asset-free Android debug and release builds must still assemble after every task (`tools/ci.sh`).

## Review Focus

1. **Crash during a split:** a rider who crashes inside a split/merge window must remount on the road they were on. Task 4 includes `napa-crash-split`.
2. **Traffic and riders overlapping at the same depth:** draw order must follow the original, or a car pops in front of a rider. Task 2's pixel golden includes a frame with a car alongside a rider.
3. **Arrest while crashed or while running back to the bike:** Task 6's `napa-cops-crashed` scenario covers it.
4. **Race end while crashed or busted:** the race must still end 0xb4 ticks after it is decided (`0x00416010`), and results must show the original outcome. This is covered by Task 8.
5. **Renderer reset mid-race on Android** (screen off/on): actor textures and the scene texture must recreate. Task 2 extends the existing reset path and its smoke test.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/traffic.hpp/.cpp` | Existing; committed in Task 1; drawing moves to `actor_nodes` in Task 2 |
| `src/actor_animation.hpp/.cpp` | RRAN playback state per object (`0x0040ac70`, `0x0043ae10`, `0x0043ae90`) |
| `src/actor_nodes.hpp/.cpp` | Rider/bike/shadow/car/cop nodes and their merge into the scene list |
| `src/course_scene.hpp/.cpp` | Split stitching, marked terrain, building marking transitions |
| `src/race.hpp/.cpp` | Walls, crash/remount, overtaking, fight states, police hook |
| `src/police.hpp/.cpp` | Cop spawn, chase, arrest (Napa scope) |
| `src/sound.cpp` | Car engines, sirens, horns (event 18), speech |
| `src/hud.cpp` | Mini dashboard and opponent-distance indicator |
| `tests/actor_animation_test.cpp`, `tests/actor_nodes_test.cpp`, `tests/police_test.cpp` | Synthetic tests |
| `tests/oracle_scene_test.cpp` | Extended: natively built lists compared with v3 dumps |

---

### Task 1: Land the in-progress traffic module

The working tree has uncommitted `src/traffic.*`, `tests/traffic_test.cpp` and hooks in `race.*`, `game_main.cpp` and `CMakeLists.txt`. They build, and `ctest -R traffic` passes.

**Files:**
- Modify: `docs/REVERSE_ENGINEERING.md` (new "Free traffic" section: `0x0044db80`, `0x0044dd00`, `0x0044dd90`, `0x0044df60`, `0x004465f0`, `0x0044efa0`, the SNAC mask chain `0x0040b5f0`/`0x0040b620`, the 16-object pool `0x00438b70`)
- Modify: `docs/PROGRESS.md` (Traffic row: "spawn/placement/vehicle choice ported; tick order and drawing unverified"), `docs/VERIFICATION.md`, `docs/NAPA_IMPLEMENTATION.md`

- [ ] **Step 1:** Read `src/traffic.cpp` end to end. Check that every rule has an address comment and every container read is bounds-checked. `tests/traffic_test.cpp` must include truncated `SNAC`/`CARS` inputs. If one is missing, add it, then run it and confirm it fails before fixing.
- [ ] **Step 2:** Run the regression gate (00-INDEX), including `build-ubsan` and `tools/ci.sh`.
- [ ] **Step 3:** Smoke test: `build/rr-game --assets ../cdrom/ROADRASH --race --ticks 3600 --autopilot --screenshot analysis/napa-traffic.bmp`. Inspect the image for cars on the road.
- [ ] **Step 4:** Commit:

```bash
git add src/traffic.hpp src/traffic.cpp tests/traffic_test.cpp src/race.hpp src/race.cpp src/game_main.cpp CMakeLists.txt docs/
git commit -m "traffic: original free-traffic spawning, placement and vehicle choice"
```

---

### Task 2: Actor animation and merged actor nodes

**Files:**
- Create: `src/actor_animation.hpp/.cpp`, `src/actor_nodes.hpp/.cpp`, `tests/actor_animation_test.cpp`, `tests/actor_nodes_test.cpp`
- Modify: `src/game_main.cpp` (one `render_nodes` call over the merged list), `src/rider_sprites.cpp`, `src/traffic.cpp` (remove `traffic_draw_nodes` after migration), `CMakeLists.txt`

**Interfaces:**
- Consumes: `rr::Animation` / `AnimationFrame` (`src/animation.hpp`), `rr::DrawNode` (`src/world_renderer.hpp`), `CourseScene::draw_nodes`, rider sprite pages (`src/rider_sprites.hpp`).
- Produces:
  ```cpp
  namespace rr {
  struct AnimationState { uint16_t animation = 0; uint16_t frame = 0; uint8_t timer = 0; bool done = false; };
  void set_animation(AnimationState&, uint16_t code);                         // 0x0040ac70
  bool advance_animation(AnimationState&, const std::vector<Animation>&);     // 0x0043ae90; true on a hit/event frame
  struct ActorView { const std::vector<RouteSample>* route; const std::vector<RoadSlice>* slices; Camera camera; const DepthTable* depth; };
  // All dynamic nodes (riders, bikes, shadows, cars, cops) merged into the static scene list in the original order.
  std::vector<DrawNode> merge_actor_nodes(std::vector<DrawNode> scene, const Race&, const TrafficSystem*, const ActorView&);
  }
  ```

- [ ] **Step 1: Trace.** Read `0x0040ac70`, `0x0043ae10` and `0x0043ae90`. Then find where rider and car objects are inserted into the display list relative to scenery: the scene build `0x00443af0` subtree and its per-slice object chains. Record the order rule (per slice, far to near; object order within a slice) in REVERSE_ENGINEERING.md.
- [ ] **Step 2: Capture.** Run `napa-actors`: drive=2, level 1, opponents and traffic on, `RR_ORACLE_PIXELS=1`, `RR_ORACLE_SHOT_EVERY=30`, 1,800 frames. Pick frames where a car and a rider overlap in depth and list them in SIM_SCENARIOS.md.
- [ ] **Step 3: Write the failing tests.**
  - **Synthetic animation test** for loop flags, durations and hit-frame events, built from a hand-made `Animation` with three frames (durations 2, 1, 3; loop flag on the last).
  - **Golden test:** extend `oracle_scene_test` with a `--native-actors` mode. It rebuilds the actor nodes from the dump's object snapshot and compares node count, order, resource, frame, flip and coordinates with the original list. Run both and confirm they fail.
- [ ] **Step 4: Port.**
  - Port animation playback and node creation for riders, bikes, shadows (ids `0xde`/`0xdf`/`0xe0`, `0x00443820`) and cars.
  - Replace the frame choice "by inspection" (PROGRESS.md Assumptions: lean 3–11, attack frames, crash 21) and the depth-512 sprite-scale stand-in with the traced rules.
  - Feed attack hit-frame events to `Race` combat. This replaces `attack_hit_tick = 7` and `attack_ticks = 18`, and must keep the plan 10 Task 6 combat goldens passing.
- [ ] **Step 5: Accept.** Native actor nodes match every node of the captured frames, and the merged list renders every pixel equal to the original (`oracle-scene` pixel mode) on the overlap frames. Renderer reset: run `rr-game --race --ticks 600 --reset-renderer-at 300` (add this debug flag if it does not exist) and confirm it exits 0 with a valid screenshot.
- [ ] **Step 6:** Update the docs and commit: `render: original actor animation and merged actor display nodes`.

---

### Task 3: Remaining Napa scene construction

**Files:**
- Modify: `src/course_scene.cpp`, `src/building_panels.cpp`, `src/terrain_panels.cpp`, `tests/course_scene_test.cpp`, `tests/oracle_scene_test.cpp`

**Open items (from PROGRESS.md and NAPA_IMPLEMENTATION.md):**
- split stitching (both roadways drawn through split/merge windows);
- marked terrain;
- building marking transitions;
- the `DLSR` `+16`/`+17` bytes and the per-sample `+0x148`/`+0x149` selectors;
- terrain/scenery random-call order;
- billboard ground interpolation.

- [ ] **Step 1: Capture.** Run `napa-split-scene`: drive to each Napa split with `RR_ORACLE_PIXELS=1`, every 10 frames through the split and merge windows, on both branches (use the plan 10 `napa-solo-split-left/right` control files).
- [ ] **Step 2: Write the failing test.** Extend `oracle_scene_test` with `--native-scene`. It builds `CourseScene::draw_nodes` for the dumped camera and compares static nodes with the dump's list. Do this only after Task 2, when the dump's actor nodes can be separated out. Run it on `napa-drive` and `napa-split-scene` and record the first differing node per frame.
- [ ] **Step 3: Port each item** from its original function, re-running after each one. The random-call order item must make native terrain/scenery generation consume `rand()` in the original order. Verify that against the dump's per-section values, not by inspection.
- [ ] **Step 4: Accept.** All captured `napa-drive` and `napa-split-scene` frames match node-for-node and pixel-for-pixel with natively built lists. Both branches still render all 2,509 sweep views (`course_scene-test`).
- [ ] **Step 5:** Commit per item, e.g. `scene: split stitching through Napa split windows`.

---

### Task 4: Walls, crashes, remount and running back to the bike

**Files:**
- Modify: `src/race.cpp`, `src/race.hpp`, `src/sound.cpp`, docs

**Starting point:**
- wall/terrain collision `0x0043a610` (wall grunt, sound event 5);
- crash/knock-off sequence `0x00403d60`;
- landing `0x0043a190`;
- wreck `0x0040a600`;
- contact handler `0x004049c0`, whose non-bike branches cover rider against car and rider against scenery.

The current stand-in clamps at the shoulder, crashes above speed 4000 and remounts after one second.

- [ ] **Step 1: Capture plan 10 scenarios:**
  - `napa-crash-wall`: steer hard into the wall at speed;
  - `napa-crash-car`: drive=0 straight into oncoming traffic;
  - `napa-crash-split`: crash inside a split window;
  - `napa-crash-wreck`: repeated crashes until the bike is wrecked.

  Run them and record the failures.
- [ ] **Step 2: Port** `0x0043a610`, the crash sequence and its states (flying rider, sliding, standing, running back, remount), and the contact branches against cars. The rider's animation codes come from Task 2's `set_animation`.
- [ ] **Step 3: Accept.** All four scenarios pass `oracle-sim` with zero divergence. Remove the "Walls and crashes" stand-in from PROGRESS.md.
- [ ] **Step 4:** Commit: `sim: original wall collision, crash and remount sequence`.

---

### Task 5: AI overtaking and fight states

**Files:**
- Modify: `src/race.cpp`, `src/race.hpp`, docs

**Starting point:** `0x00407bd0` state machine. State 1 (racing) is ported. Port overtaking/avoidance `0x00408e40` and fight states 2–5. Remove the "attacks alongside every 45 ticks" stand-in.

- [ ] **Step 1:** Capture `napa-field-fight-l1`: seven opponents with the player idling alongside the pack (drive=2 with throttle released every other second), 3,600 ticks. Confirm it fails.
- [ ] **Step 2:** Port the states. Weapon grabs (`0x004096f0` backhand shove and the grab rule) replace "1 in 4".
- [ ] **Step 3:** Accept: `napa-field-fight-l1` and plan 10's `napa-fight` pass. Commit: `sim: AI overtaking and fight states`.

---

### Task 6: Police on Napa

**Files:**
- Create: `src/police.hpp`, `src/police.cpp`, `tests/police_test.cpp`
- Modify: `src/race.cpp` (call at the original point), `src/actor_nodes.cpp` (cop nodes from the 45 cop frames), `src/sound.cpp` (sirens: slots 30/31, event 19 from `0x00435ae0`), `src/game_main.cpp` (busted outcome → results/video), docs

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct Cop { Rider rider; uint8_t state = 0; int32_t timer = 0; };
  class Police {
  public:
      Police(const std::vector<uint8_t>& exe, int level, int course_index);   // spawn rules from the EXE tables
      void step(Race&, const std::function<uint32_t()>& random);              // spawn/chase/arrest
      const std::vector<Cop>& cops() const;
  };
  }
  ```
  `Race` gains `bool busted() const` (outcome code 4, `0x00407f30`).

- [ ] **Step 1: Trace** cop spawn (which levels and courses, and whether Napa level 1 has cops at all), the cop think/move, and the arrest condition (`0x00407f30`), including arrest while the player is crashed. Record the BUSTED video selection rule for plan 13 Task 4. **If Napa level 1 has no cops in the original, record that with evidence.** The task then covers the module and Napa at levels where cops appear.
- [ ] **Step 2:** Capture `napa-cops` (a level where cops appear on Napa, or the attract demo if it shows cops) and `napa-cops-crashed`. Confirm both fail.
- [ ] **Step 3:** Port. The synthetic `police_test` covers the arrest condition boundaries found in Step 1.
- [ ] **Step 4:** Accept: both scenarios pass `oracle-sim`, and a cop frame matches pixels in `oracle-scene`. Commit: `sim: police, sirens and arrests`.

---

### Task 7: Race sounds, dashboard extras and race music hook

**Files:**
- Modify: `src/sound.cpp`, `src/sound.hpp`, `src/hud.cpp`, `src/hud.hpp`, `src/game_main.cpp`, `tests/sound_test.cpp`, `tests/hud_test.cpp`

- [ ] **Step 1: Car engines.** The two vehicles nearest the camera (`0x004350e0`) use slots 4/6 for cars. Car horn is event 18 from `TrafficSystem::take_sound_events`. Add a synthetic test: with two cars and two bikes at chosen distances, `nearest_vehicles` picks the same four slots the original rule picks.
- [ ] **Step 2: Mini dashboard and opponent-distance indicator.** Trace the low-detail/mini dashboard path in `0x00416310` and the indicator draw. Add the 24-combination style test that `hud_test` already uses, extended to the mini dashboard.
- [ ] **Step 3: Race music hook.** Call plan 13's `MusicPlayer::play_race(course_index, alias)` at race start and stop it at race end (`0x004160c0` → `0x00402f50`, `0x00433bb0`). If plan 13 has not landed, the hook calls a no-op implementation behind the same interface, and NAPA_IMPLEMENTATION.md keeps "race soundtrack" unchecked.
- [ ] **Step 4:** Commit: `audio: car engines and horns; hud: mini dashboard`.

---

### Task 8: Napa acceptance run

**Files:**
- Modify: `docs/NAPA_IMPLEMENTATION.md`, `docs/PROGRESS.md`, `docs/VERIFICATION.md`
- Create: `tests/napa_acceptance_test.cpp` (CTest `napa-acceptance`, skip 77 without assets)

- [ ] **Step 1: Full-race golden.** Capture `sim-napa-full-l1`: a complete level-1 Napa race from the career start, with seed 1, recorded human-like controls (drive=2 plus scripted attacks) and opponents, traffic and police at original settings. Pixels every 300 frames.
- [ ] **Step 2: Acceptance test.** Write `napa_acceptance_test`. It runs `make_race_setup` (plan 10) for the full-race golden, replays every tick, compares every field, renders the sampled frames and compares pixels. It then checks that the final standings, outcome and prize/fine values from `career.cpp` equal the original results-screen values captured in the dump (add those fields to the hook when the results screen appears).
- [ ] **Step 3: Gate.** Run the regression gate, `tools/ci.sh`, refresh the macOS `.app`/ZIP (`tools/build_macos_app.sh`, `tools/verify_macos_app.py`), and assemble the Android debug and release builds.
- [ ] **Step 4: Close the checklist.** Tick each NAPA_IMPLEMENTATION.md item only with its evidence line (test name and log path). Commit: `napa: full opening race verified against original`.

## Exit criteria

- Every NAPA_IMPLEMENTATION.md checkbox is ticked with evidence. Android hardware items may stay explicitly "pending device" (plan 14).
- `napa-acceptance` passes. PROGRESS.md "Assumptions" no longer lists any Napa stand-in, or lists it with the reason it remains.
