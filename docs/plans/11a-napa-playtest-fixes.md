# Napa Playtest Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. This plan runs **before** the rest of plan 11. Each task is independent.

**Goal:** Fix the defects the owner found while playing the Napa race on October 11, 2026, and make sure every way of launching the game runs the same current build:
1. Traffic collision is not implemented: riders pass through cars.
2. The meter on the right of the dashboard glitches.
3. Bikes show graphical glitches.
4. There appear to be two versions: "the Napa demo" and `rr-native`, and `rr-native` lacks the updates.

**Architecture:** No new subsystems. Each defect is fixed at its root cause in the existing modules:
- `rr::Race` handles contact and crashes;
- the bike tick owns the RPM field the dashboard reads;
- `ActorNodes` builds the rider, shadow and bike display nodes;
- the launch scripts and app bundle are fixed so they all run `rr-game`.

**Tech Stack:** C++17, SDL3, CTest, the Wine oracle (`oracle/run.sh`, scene dump v3, plan 10 sim dumps).

**Spec:** the owner's playtest report (October 11, 2026), [../NAPA_IMPLEMENTATION.md](../NAPA_IMPLEMENTATION.md).

## Findings from the review (October 11, 2026)

**Defect 1: traffic collision.**
- `TrafficSystem` steps inside `Race::step` (`src/race.cpp`, the `traffic_->step(...)` call), but nothing tests riders against cars.
- `Race::collide`/`contact` handle rider pairs only. `crash.cpp` ports the crash entry `0x00403d60` with reason-specific animations, but no car-contact reason ever reaches it.
- Car collision dimensions come with the imported CANS descriptors (`actor_descriptors.cpp`), so the data needed for a hit test is already loaded.

**Defect 2: right-hand meter (tachometer).**
- `Hud::state` (`src/hud.cpp:44`) derives the RPM needle from bike `+0x2dc`. Only `bike.cpp:117` writes that field, with its initial value `0x2000`.
- In the original, `0x004349c0` updates `+0x2dc` every engine update: from the gear ratio and speed, limited to ±0x2000 per step, +0x1000 while airborne, −0x1000 after a crash, and storing `+0x2e8`/`+0x2e0`.
- The port reimplemented that rule privately inside `sound.cpp` (the `rpm_` array), so the dashboard never sees it. Depending on code paths the needle sits still or jumps.
- A related defect: the centre distance readout uses bike `+0x44` (`hud.cpp:45`), which nothing native writes. Two screenshots taken 1,500 ticks apart both show `25.5`.
- Needles are drawn by a native 3×3 Bresenham line, not the original pen routine.
- Speed angle wrap is untested for reverse speed (negative velocity).

**Defect 3: bike graphics.** Autopilot screenshots at ticks 900 and 2400 (`analysis/playtest/` once captured; see Task 3) show:
- a doubled or offset lower bike behind the rider;
- faint dithered blocks beside the bike.

Both are consistent with the shadow node (`ActorNodes::rider` emits the shadow bank `bank+2` first, positioned at terrain ground with `clip_ground=false`) being drawn at the wrong position, with the wrong frame, or opaquely. `DrawNode` has no shading or translucency mode, and the native rider nodes have **never been compared with an original display list containing the player**. Earlier oracle replays rendered the original list, not native-built actor nodes.

**Defect 4: "two versions".** There are three launch paths, and only one is current:

| Launch | Binary | State |
| --- | --- | --- |
| `./run-macos.sh` | `build/rr-native` | The old **asset research viewer** (`src/viewer.cpp`): placeholder road shading, no race systems |
| `build/rr-game …` | `build/rr-game` | The current game: front end, Napa scene, actors, traffic, crashes |
| `build-release/Road Rash Native.app` | release `rr-game` | **Built October 10**, before the whole Napa scene/actor/traffic work |

Android builds `rr-game` and is current whenever it is rebuilt.

## Global Constraints

- Every behaviour fix is accepted by an original comparison: a plan 10 tick golden, an isolated original-function fixture, or a scene/pixel dump. A screenshot that "looks right" is not acceptance.
- Defects are fixed at their source module. No HUD-side or renderer-side workaround may hide a simulation or node-construction error.
- Rebuild and recheck the regression gate in [00-INDEX.md](00-INDEX.md) after each task. Other sessions may be committing in parallel: run `git status` and `git log -3` before starting, and never revert their files.

## Review Focus

1. **Car contact while the rider is already crashed or walking back:** the original crash entry rejects an existing crash. Task 1 has a fixture with a crashed rider touching a car.
2. **Car contact during a split or merge window:** rider and car on different roadways must not collide. Task 1 has a synthetic test with a car on the other route at the same position.
3. **Tachometer at gear change, airborne and crashed:** Task 2's fixture covers all three branches of `0x004349c0`.
4. **Shadow while airborne:** a jump separates the shadow from the body. Task 3 compares a frame captured mid-jump.
5. **Stale artefacts:** after Task 4, running `./run-macos.sh` with no arguments must start the current game. The release bundle must be rebuilt from the same commit, or the verification script fails.

---

### Task 1: Rider–car collision

**Files:**
- Modify: `src/race.cpp`, `src/race.hpp`, `src/traffic.cpp`, `src/traffic.hpp`, `src/crash.cpp` (only for the car-contact reason), `tests/actor_traffic_test.cpp`, `tests/crash_test.cpp`
- Modify docs: `REVERSE_ENGINEERING.md` ("Rider–vehicle contact"), `VERIFICATION.md`, `PROGRESS.md`, `ACTORS.md`

**Interfaces:**
- Consumes: `TrafficSystem::cars()`, `Race::contact` and `collide` (rider pairs), the crash entry in `crash.cpp`, and the car collision dimensions from `actor_descriptors`.
- Produces:
  ```cpp
  // race.hpp (private): rider-against-car pass at the original point in the tick.
  void collide_traffic(Rider&);
  // traffic.hpp: mutable access for the contact response (impulse, car state).
  std::vector<TrafficCar>& cars_mut();
  ```

- [ ] **Step 1: Trace.**
  - Find where the original tests the player and AI riders against cars: the contact detector `0x004368f0` over the scheduler's object list, which includes cars, then the `0x004049c0` branches for bike against car.
  - Record the hit-box sizes per kind, the impulse and speed transfer, the crash reason code passed to `0x00403d60`, the car's own reaction (stop, swerve, horn) and the sound events (crash codes 0/1/2, grunts 3/4).
  - Write it into REVERSE_ENGINEERING.md with addresses.
- [ ] **Step 2: Capture an original fixture.** Use the same isolated-call technique as the crash fixtures (`analysis/oracle/rea-crash-entry.json` workflow). Call the contact branch with a scratch rider and a scratch car over 256 cases:
  - head-on and from behind;
  - lateral offsets across the hit-box edge;
  - speeds below and above the crash threshold;
  - a rider already crashed.

  Save to `analysis/oracle/napa-car-contact.tsv`.
- [ ] **Step 3: Write the failing tests.**
  - `actor_traffic_test` gets two synthetic cases: a car exactly at the hit-box boundary (no contact one unit outside, contact on the boundary per the traced comparison), and a car on the other split roadway at the same position, which gives no contact.
  - `crash_test` gets a golden mode that runs the 256 fixture cases and compares every tracked field on rider, bike and car.

  Run both and confirm they fail.
- [ ] **Step 4: Port.** Call `collide_traffic` for every rider (player and AI) at the traced point in `Race::step`. Use the fixture-verified response, and enter the crash through the existing crash module with the car reason. Queue the original sound events.
- [ ] **Step 5: Accept.**
  - The fixture matches every case.
  - A live check (`rr-game --race --ticks 6000` with a debug flag `--steer-into-traffic` that forces the autopilot towards the oncoming lane; add it if missing) shows at least one crash with reason "car" in the log.
  - Plan 10's `napa-crash-car` tick scenario is recorded as the remaining parity check.
- [ ] **Step 6:** Commit: `sim: rider and car contact with original crash response`.

---

### Task 2: Dashboard tachometer and distance readout

**Files:**
- Modify: `src/bike.cpp`, `src/bike.hpp` (or `src/race.cpp`, wherever the per-tick engine update belongs; see Step 1), `src/sound.cpp` (read the bike field instead of its private `rpm_`), `src/hud.cpp`, `tests/bike_test.cpp`, `tests/hud_test.cpp`, `tests/sound_test.cpp`

**Interfaces:**
- Produces: `void update_engine_rpm(Bike&)`, a literal port of `0x004349c0`, in `bike.cpp`:

```cpp
// 0x004349c0: engine rpm (+0x2dc) from the gear ratio and speed, at most
// +-0x2000 per update; +0x1000 while airborne (height +0x20 >= 0x65), -0x1000
// after a crash. +0x2e8 caches the speed; +0x2e0 is the fixed volume 0x9ffe.
void update_engine_rpm(Bike& b) {
    const int32_t speed = b.i32(0xec);
    if (b.i32(0x2e8) == speed && speed != 0) return;
    int32_t next;
    if (b.i32(0x20) < 0x65 || speed == 0) {
        b.i32(0x2e8) = speed;
        const int32_t ratio = b.i32(0x1cc + b.i32(0x23c) * 0x10);
        int32_t target = int32_t(uint32_t(ratio) * uint32_t(speed));       // 32-bit wrap as the original imul
        target = ((target + ((target >> 31) & 0xff)) >> 8) + 0x2000;      // sar rounding toward zero
        const int32_t current = b.i32(0x2dc);
        if (b.i32(0x2f4) == 0 || current < 0x2001) {
            next = current + 0x2000;
            if (target <= next) { next = target; if (target < current - 0x2000) next = current - 0x2000; }
        } else {
            next = current - 0x1000;
        }
    } else {
        next = std::min(b.i32(0x2dc) + 0x1000, 0xffff);
    }
    b.i32(0x2e0) = 0x9ffe;
    b.i32(0x2dc) = next;
}
```

  Check the airborne branch against the disassembly before trusting the decompiled text above. In the decompile, the `else` branch (speed unchanged path / airborne) does **not** update `+0x2e8`; keep that.

- [ ] **Step 1: Find the call site.** Find where the original calls `0x004349c0`: per tick from the bike/rider think, or from the sound update `0x00434ce0`/`0x00435030`. The native call must sit at the same point, because `+0x2e8` caching makes the result depend on call frequency.
- [ ] **Step 2: Write the failing tests.**
  - `bike_test` drives `update_engine_rpm` over a synthetic bike for: gear change at constant speed; acceleration from 0; airborne (`+0x20 = 0x65`) with the 0xffff cap; crashed with rpm > 0x2001; and speed unchanged with nonzero speed (no change).
  - If an isolated original-function fixture is practical (same technique as the crash fixtures), capture 256 cases into `analysis/oracle/engine-rpm.tsv` and compare all three fields.
  - `hud_test` asserts that `Hud::state(...).rpm_angle` follows `+0x2dc`.
- [ ] **Step 3: Implement.** Call it at the traced point. Delete `sound.cpp`'s private `rpm_`/`rpm_speed_` state and read `+0x2dc` for the player and opponents, so the engine pitch and the needle cannot disagree. Then run `sound_test`.
- [ ] **Step 4: Distance readout.** Trace the dashboard draw (`0x00416310` layout, the routine that prints layout entry 30) for which object and field it reads. `+0x44` on the player's bike is evidently wrong or unmaintained. Port the source and add a `hud_test` case with a synthetic value.
- [ ] **Step 5: Accept against the original.** Capture `napa-dash`: `RR_ORACLE_PIXELS=1` **after** the dashboard is drawn. Add a post-dashboard capture point to the oracle hook if the existing one is pre-dashboard (it is: `0x004439f5` captures before presentation). Compare the dashboard region over 120 frames:
  - needle endpoints and the distance text must match the original;
  - needle pen pixels may differ only where the native line routine differs.

  Document either result. If the pen rasterisation is ported, remove that exception.
- [ ] **Step 6:** Commit: `hud: tachometer from the original engine rpm update; distance readout source`.

---

### Task 3: Rider, bike and shadow display nodes

**Files:**
- Modify: `src/actor_nodes.cpp`, `src/actor_render.cpp`, `src/world_renderer.cpp` / `src/texture.cpp` (only if a shadow raster mode is traced), `src/rider_sprites.cpp`, `tests/oracle_scene_test.cpp`, `tests/actor_nodes_test.cpp`
- Create: `analysis/playtest/` (git-ignored via `.gitignore`, added in the October 11 replan; never commit its screenshots)

- [ ] **Step 1: Reproduce and record.** Capture native frames showing the glitches into `analysis/playtest/napa-t<tick>.bmp` with `build/rr-game --assets ../cdrom/ROADRASH --race --autopilot --ticks <t> --screenshot …` for t = 300, 900, 1500, 2400. Add a debug flag `--dump-nodes FILE` that writes the merged node list (resource, frame, bank, view points, flip) for the screenshot frame.
- [ ] **Step 2: Get the original nodes for the same situation.** The v3 scene dumps (`napa-drive`, frames 0/150/300/750) contain the original player, bike and shadow nodes. Add `--native-actors` to `oracle-scene-test`. It rebuilds the player's and opponents' actor nodes from the dumped object state (position, lateral, height, animation selection) and compares them with the dump's actor nodes:
  - node order;
  - bank (body/shadow);
  - frame;
  - all four view points;
  - flip;
  - texture/palette selection (`0xde`/`0xdf`/`0xe0`).

  Run it and confirm it fails on at least the defect frames' equivalents. **If the existing dumps lack the object state needed**, extend dump v3 with the rider/bike object bytes (the plan 10 field table already lists the offsets) and recapture `napa-drive`.
- [ ] **Step 3: Fix at the root**, guided by the first differing field. Candidate causes, in the order to check:
  1. shadow projection (`clip_ground=false`, ground without vertical, wrong depth or fraction);
  2. view/subframe selection (`select_actor_view` angles; yaw sign; the rider `depth*2` rule);
  3. frame bank mismatch between body and shadow (`bank+2` versus the 0xe0 sheet);
  4. a missing shadow raster mode: if the original draws shadows through a darkening or translucency table rather than plain index-zero transparency, trace it in `0x0044d300` dispatch and add the mode to `DrawNode` and `render_nodes` with a raster fixture;
  5. a stale page cache key in `ActorNodes::texture` (`pages_` keyed by bank, frame and palette) after a renderer reset.

  Also check the weapon frame: a club appeared in the player's hand while the autopilot was not attacking (tick 2400 screenshot). Confirm the player's starting weapon and the riding-animation selection against the original.
- [ ] **Step 4: Accept.**
  - `oracle-scene-test --native-actors` matches every actor node in every captured frame, and pixel mode matches the full frame.
  - Recapture the Step 1 screenshots, inspect them, and add a frame mid-jump (Review Focus 4).
- [ ] **Step 5:** Commit: `render: rider, bike and shadow nodes match original display lists`.

---

### Task 4: One game, every launch path

**Files:**
- Modify: `run-macos.sh`, `README.md`, `docs/RUN_NATIVE.md`, `docs/USER_GUIDE.md`, `tools/ci.sh`, `tools/verify_macos_app.py`
- Optionally rename the target: `rr-native` → `rr-viewer` (CMake target, smoke commands in docs and `tools/ci.sh`, plans 00/10–15)

- [ ] **Step 1: Write the failing check.** Add a test to `tools/verify_macos_app.py`. It reads a build stamp from the app (add `RRBuildCommit` to `Info.plist` from `git rev-parse --short HEAD` in `tools/build_macos_app.sh`) and fails if it differs from `HEAD`. Run it against the current October 10 bundle and confirm it fails.
- [ ] **Step 2:** Make `run-macos.sh` build and launch `rr-game` (passing `--assets` when a mounted disc exists, otherwise the first-run import), and add `run-viewer.sh` for the research viewer. Rename the viewer target to `rr-viewer` and update every reference: `grep -rn "rr-native" --include='*.sh' --include='*.md' --include='*.txt' --include='*.py' .`. Keep the app bundle's display name "Road Rash Native".
- [ ] **Step 3:** Rebuild the release bundle with `tools/build_macos_app.sh`, run `tools/verify_macos_app.py`, and confirm it passes with the stamp check.
- [ ] **Step 4:** Have `tools/ci.sh` rebuild the bundle (or fail if it is stale), so it cannot fall behind again.
- [ ] **Step 5:** README "Run" section: one command to play (`./run-macos.sh`), with the viewer listed separately as a developer tool. Commit: `build: run scripts and app bundle launch the current game; viewer renamed rr-viewer`.

## Exit criteria

- All four tasks are committed, the regression gate passes, and PROGRESS.md and NAPA_IMPLEMENTATION.md record each fix with its evidence.
- The owner replays Napa from `./run-macos.sh` and from the rebuilt `.app` and confirms all three playtest defects are gone. Record the date and the owner's confirmation in VERIFICATION.md.
