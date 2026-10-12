# Opening Napa race implementation

Requested October 10, 2026: continue until the entire opening Napa race is
ported to the shared native macOS/Android game. The user confirmed that this
means the opening Napa race, rather than all five level-1 courses.

Completion requires the original Napa course from start to finish, with
terrain/scenery/horizon, road materials, riders/animations/dashboard, traffic
and police, combat/crash/recovery, race sounds/music and the menu/results flow.
Imported original resources must remain outside the APK/source. Native and
Android builds must pass; hardware checks must be distinguished from software
verification. Approximations cannot be silently described as original matches.

## Current work

- The playable Napa renderer uses imported indexed road, hills, horizon, static
  scenery, player sprite and full dashboard. Both branch routes pass 2,509
  raster views, and all 6,471 scenery objects resolve imported frames.
- Free traffic is live on Napa (session 23): the original spawn timers,
  placement and vehicle library from `DATA/CARS/NAPA.CAR`, with movement,
  honking and drawing as documented approximations. See `docs/ACTORS.md`.
- Animation decoding covers 1,663 family animations and 6,503 frames. Roadside
  placement matches 1,024 direct original-function samples.
- Combat remainder landed (October 12: `ba7e127`, `42e3e4d`): shared
  `knock_off` routing (`0x004052c0`) on every contact branch, weapon hits at
  descriptor frames instead of the tick-7 stand-in, nitro (`0x0040b110`
  family), roadside-hazard collision, animation advanced in-tick, and
  crash-impulse ordering. Evidence: `race` (`tests/race_test.cpp`), `crash`
  (`tests/crash_test.cpp`), `sim_fields` replay checks. Full-race
  combat equality against original captures is still open (car/police/
  animation dump fields not yet in the proxy).
- Courses census landed (`e7dc0a6`): 26 course/level-combo census with a
  level-tagged finish table (`finish_event_sample`, `make_route_choices`).
  Evidence: `course` (`tests/course_test.cpp`). Finish-crossing latch work
  exists only as uncommitted notes (`analysis/course-finish-notes.md`) and
  the finish golden is still open.
- Race music landed (`fcaf29b`, `7edce12`): MIDS song parser, SoundFont 1
  banks, factory presets for unmapped programs, streaming PCM queue in the
  mixer, race hooks and jukebox video triggers. Evidence: `music`
  (`tests/music_test.cpp`). Live `game_main.cpp` wiring stays with the
  playtest-visual-fixes session owner; hardware A/V latency unverified.
- Render infrastructure landed (`cea33a2`): Q16-preserving wide path,
  signed mip selection, MMPX cache, band-threaded raster. The 640x480
  baseline stays bit-identical; enhancements are opt-in. Evidence: `mmpx`,
  `upscale_cache`, `texture`, `raster`, `view` tests.
- Replay harness landed (`ce8e1eb`, `e4b32a8`): driver-axis
  (`human`/`demo`) replay independent of solo, shared race factory
  prerequisites, strict comparator over the required field set. Evidence:
  `oracle-sim` (`tests/oracle_sim_test.cpp`), `sim_fields`, `golden`.
  Single-anchor acceptance still needs the finish-crossing golden, the
  15-rider golden, and proxy captures of car/police/animation fields.
- The 66-test native and sanitizer suites pass, and both Android variants build.
- Rider-car collisions, DZHR scripted vehicles and police, the original car
  animation/scaling laws, split stitching, full rider animation/shadows,
  traffic/police behaviour parity and the race soundtrack remain open.
  Random-call ordering, billboard ground interpolation, native fonts and needle
  rasterization still have known parity limitations.

## Owner playtest (October 11, 2026)

The owner played Napa and reported missing traffic collision, a glitching right-hand dashboard meter (tachometer), and graphical glitches on bikes. Root causes and fixes are in [plans/11a-napa-playtest-fixes.md](plans/11a-napa-playtest-fixes.md). The owner also found that `./run-macos.sh` launches the old research viewer and that the release `.app` is stale (October 10); 11a Task 4 fixes both.

October 11 (session 25) progress: Task 4 (one game, every launch path) is done —
`run-macos.sh` launches `rr-game`, the research viewer is renamed `rr-viewer`
with its own `run-viewer.sh`, and the release bundle is stamped with its build
commit and verified against HEAD. Task 2's tachometer is fixed: `0x004349c0` is
ported as `update_engine_rpm` and the engine pitch and dashboard needle now
read the same `+0x2dc`. The distance readout is traced but not yet ported.
Tasks 1 (rider–car collision) and 3 (bike/shadow nodes) remain.

## Acceptance tracking

- [ ] Native scene construction and terrain/road/sky composition.
- [ ] Scenery placement and original resource selectors across the Napa route.
- [ ] Rider animation, shadows and dashboard presentation.
- [ ] Level-1 Napa traffic/police and corresponding gameplay outcomes.
- [ ] Combat/crashes/recovery and finish/qualification/results flow verified.
- [ ] Race soundtrack and all needed sound/video triggers.
- [ ] Complete automated Napa race and regression checks.
- [ ] Refreshed standalone macOS and asset-free Android packages.

Slice evidence landed October 12 (partial — no box above ticks, per plan 11
Task 8 Step 4, because the full-race `napa-acceptance` golden does not exist yet):
- Combat/crash item: `race`, `crash`, `sim_fields` pass
  (`ba7e127`, `42e3e4d`; notes `analysis/plan16-combat-notes.md`,
  `analysis/combat-anim-notes.md`).
- Scenery/course item: `course` passes, 26-combo census + finish table
  (`e7dc0a6`); finish-crossing golden still open.
- Soundtrack item: `music` passes, MIDS/SF1 parser + streaming player
  (`fcaf29b`, `7edce12`; notes `analysis/music-notes.md`,
  `analysis/music-next-notes.md`).
- Renderer item: `mmpx`, `upscale_cache`, `texture`, `raster`, `view` pass
  (`cea33a2`; note `analysis/render-infra-notes.md`).
- Automated-checks item: `oracle-sim`, `sim_fields`, `golden` pass with the
  driver-axis harness (`ce8e1eb`, `e4b32a8`; notes
  `analysis/replay-notes.md`, `analysis/replay-prereq-notes.md`).
  Still open: finish-crossing golden, 15-rider golden, proxy captures of
  car/police/animation fields.

No Android device is connected as of the start of this work. Physical Android
rendering, input and picker/import verification remain pending.

October 11 update: family/cel-group parsing and selection are verified across the
full family corpus and 176 direct original selections. Near hill subdivision
matches 128 original fixtures. Hill-band construction matches all 1,164 emitted
quads in 256 fixtures. These are shared native modules; live composition and
full-race acceptance remain unchecked. The one-frame Napa scene oracle is
complete; a longer bootstrap capture faults in original surface restoration
and cannot be used as a completed golden run.

Course integration prerequisites now include decoded CESR marking/event bytes
and MSRR family requests. The other roadway in every split/merge window retains
its own terrain section/local reference. All 7,949 Napa event samples and all
five courses' 3,586 dual references are verified. Shoulder strips match 527
original quads. The current 46-test native and sanitizer suites pass, and both
asset-free Android variants build.

The multi-frame bootstrap fault was traced to entering the credits screen
before starting a course: its timer survived after race setup freed the menu
surfaces. The bootstrap now uses the original course-menu exit directly. A
complete 121-frame capture succeeds, but the camera remains stationary during
that interval. Its five sampled display lists replay exactly. A longer capture
with explicit original control input is underway for moving-scene verification.

Playable rendering update: Napa now draws its imported indexed road materials,
hills and horizon in the shared game loop. Both branches pass 2,509 geometry
views and 503 sampled raster views. Moving display-list replays match every
pixel in four captured frames, including 130 nodes at frame 750. Full native
scene equality is not established: buildings, marked terrain, split stitching,
scenery, horizon motion and random/resource timing remain unfinished. The
acceptance checkboxes remain open.

Dashboard continuation: imported full-size dashboards, recovered meter/needle
geometry and Badloc text now appear in the playable game. Original animation,
hotspots, shadows, mini dashboard/opponent indicators and post-dashboard golden
comparison remain open. All 2,509 sampled branch windows render; process-specific
out-of-resource raster reads are guarded with zero rather than unsafe host reads.
A bounded autopilot run finishes Napa first at 9:49, while full race-system
acceptance remains incomplete.
