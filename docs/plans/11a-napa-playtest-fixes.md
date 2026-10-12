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

- [x] **Step 1: Trace (done).** REVERSE_ENGINEERING.md "Rider–vehicle contact": `0x004368f0` detector (+0x10 tick match, +0xc0 6/5 walks, +0x178/+0x174/+0x17c overlap, +0x36 shared roadway), `0x00436a40` separation (masses 256/1800), bike `0x004049c0` + car `0x0044efa0` horn branches, hit-boxes/impulse/reason codes/sound events recorded.
- [x] **Step 2: Capture (done).** `analysis/oracle/napa-car-contact.tsv` (git-ignored): 256 isolated `0x004049c0` calls, 0 faults — 32 fast-contained crashes (reason 2, anim 29), 32 slow-contained speed transfers, 64 side shoves, 128 crashed-rider rejections.
- [x] **Step 3: Write the failing tests (done).** `traffic_test` synthetic boundaries (hit-box edge ±1, split-roadway exclusion) fail-then-pass; fixture case comparison done ad-hoc, not yet an in-tree golden.
- [x] **Step 4: Port (done).** `collide_traffic` for player and AI at the traced point in `Race::step`; crash entry with the car reason; original sound events queued.
- [x] **Step 5: Accept (done, with open remainders).** Live `--steer-into-traffic` 6000-tick run shows `car-crash tick 1249 reason 2` (plus a second crash at 5408). Fixture golden comparison (256 cases in-tree) and the `napa-crash-car` tick scenario remain open (plan 10 oracle work).
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

- [x] **Step 1: Find the call site (done).** `update_engine_rpm` (`0x004349c0` literal) lives in `bike.cpp`; called per tick from the race tick and from `SoundSystem::update` (player always refreshed, even with sound off — `46e08c0`).
- [x] **Step 2: Write the failing tests (done).** `bike_test` covers gear change, acceleration, airborne cap, crashed rpm, speed-unchanged; `hud_test` asserts `rpm_angle` follows `+0x2dc`. No isolated fixture captured.
- [x] **Step 3: Implement (done).** `sound.cpp` reads bike `+0x2dc` (no private rpm state); `sound_test` green.
- [x] **Step 4: Distance readout (done).** Entry 30 prints route distance from `+0x44` (`+0x1c` snapshot): `(pos>>8)-50)/33` tenths (metric `(pos>>5)-400)/165`), 0.0 at grid sample 50; `hud_test` synthetic case (50+33*12 → 12).
- [ ] **Step 5: Accept against the original.** Capture `napa-dash`: `RR_ORACLE_PIXELS=1` **after** the dashboard is drawn. Add a post-dashboard capture point to the oracle hook if the existing one is pre-dashboard (it is: `0x004439f5` captures before presentation). Compare the dashboard region over 120 frames:
  - needle endpoints and the distance text must match the original;
  - needle pen pixels may differ only where the native line routine differs.

  Document either result. If the pen rasterisation is ported, remove that exception.

---

### Task 3: Rider, bike and shadow display nodes

**Files:**
- Modify: `src/actor_nodes.cpp`, `src/actor_render.cpp`, `src/world_renderer.cpp` / `src/texture.cpp` (only if a shadow raster mode is traced), `src/rider_sprites.cpp`, `tests/oracle_scene_test.cpp`, `tests/actor_nodes_test.cpp`
- Create: `analysis/playtest/` (git-ignored via `.gitignore`, added in the October 11 replan; never commit its screenshots)

- [x] **Step 1: Reproduce and record (done, NodeCompare session).** Fresh native frames recaptured t=300..6000 (`analysis/playtest/napa-t*.bmp`, git-ignored). Club-at-2400 explained: autopilot punches every 40 ticks; player starting weapon is 0. No mid-jump frame: drive-2 never gets airborne. `--dump-nodes` not added (debug rebuilds served its role).
- [x] **Step 2: Get the original nodes (done: `0541981`, `480022a`).** Scene dump v4 carries full `0x340` bike+rider-half objects per scheduler node; `--native-actors` compares per-bb-node against dispatch records (`actor.sel.*`/`actor.bb.*` from `0x0043af60`/`0x0044ba40` call-site hooks). Failed as required, then 17/21.
- [x] **Step 3: Fix at the root (done: `dad61b3`).** Closed: shadow projection/order/bank, no shadow raster mode (`0x0044d300` same textured path), animation selection on rider halves, colour rule, bb-lateral inputs, 250-node streaming buffer mapping. The one remaining defect was body-roll rotation `0x00450ad0`: q3 sign, S/C slot mapping, SAR divide — all traced to decompile text and golden-verified. Test harness fixes in the same unit: sel-byte descriptor match (dispatch/dump `+0x81` skew), dispatch `+0xbc` roll override.
- [x] **Step 4: Accept (done: `c86d566`).** `oracle-scene-test --native-actors` matches every actor node in every captured frame (25/25 over frames 0/150/300/750 of `napa-drive-v5`); pixel mode matches the full frame (0 differing pixels). Empty-SHADOW.DAT frames (~130/326, `0x004436a0` null pixel pointer) emit geometry with an empty texture. Screenshots reinspected. NOT done: frame mid-jump (no jump-capable capture exists) — tracked as remaining.
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
