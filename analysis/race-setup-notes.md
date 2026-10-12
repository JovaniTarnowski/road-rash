# Race setup extraction (plan 17 Task 2) — implementation deltas

Created `src/race_setup.{hpp,cpp}` with plan 10's factory names, extended
with plan 17's fields (`RaceConfig` gains `opponents`, `high_resolution`,
`metric`; `RaceSetup` gains `hud`, `actors`, `crash`, `police`, `bounds`).

## Deviations from the spec

1. `RaceBounds` is a placeholder, not the Task 1 trace. `race_bounds()` /
   `original_course_index()` (Task 1) do not exist yet, so the factory uses
   the grid slot-0 position (`0x00464088` slot 0 = `0x3200`) as
   `start_position_q8` and route end as `finish_position_q8`, wired into the
   race via `set_finish_q8`. Task 1 replaces this with the traced per-level
   nodes; the `RaceBounds` struct shape already matches the spec.
2. `oracle_sim_test.cpp` NOT migrated (assignment allowed leaving it).
   Its `make_race_bundle` is not a clean swap: it carries replay-only
   machinery with no counterpart in the spec'd `RaceConfig` interface —
   solo scheduler freeze + nulled traffic pool, per-golden `road_base_q8`
   anchor via `race_road_base_q8`, demo-branch brain seeding from golden
   headers, `set_ride_library`/attack codes, and nitro setup. Migrating
   would change replay behavior. The game side (`game_main.cpp`) is owned
   by the live session and untouched.
3. Errors are strict, not fallback. The spec's Step 2 says unresolved
   resources throw with the resource name; the old `Game::load_course`
   caught scene failures (log + flat debug road) and traffic failures
   (traffic disabled). The factory throws for scene/HUD/actors/traffic
   (traffic only when the `.CAR` exists); `game_main.cpp` keeps its old
   fallback behavior until its owner switches it to the factory.
4. `CareerTables`/`character_rider` drive bike selection when
   `career=true` (level_bike slot `level + rider*5`, 0x004641c8); quick
   races (`career=false`) use `config.bike`. Default `character=0`
   resolves to character 1's rider. `opponents` passes straight to
   `original_opponents` (0..7 enforced there; the factory validates
   level 0..4 itself).

## Course-data gaps found (Task 2 Step 3)

`CourseScene::draw_nodes` first loop, `src/course_scene.cpp` ~lines
140-143: `depths.scale.at(size_t(fd&~1)/2)` with `fd = slice.depth+256`.
On the last projection slice (depth 8448) `fd = 8704`, index 4352, but
the table has 4352 entries (0..4351) — throws `std::out_of_range`.
CITY L0 and MEDLY L0 hit it (type-0 terrain at the far slice);
HIWAY/NAPA/CANYN L0 pass.
Bounds-break evidence for the course_scene owner (via Main):
(1) Original basis: 0x00444630 reads the depth table UNCHECKED
(`DAT_0046f098 + (depth&~1)*2`, no bounds test in decompile) — the native
.at() throw is a hardening ADDITION, not original behavior. (2) Table:
road.cpp reads exactly depth_entries=4352 ("last positive entry before
zero padding"); scale.size()==4352, max valid index 4351. (3) The +256
lookahead hits 8704→index 4352 exactly at the last projection slice
(depth 8448) with type-0 terrain — reachable on CITY/MEDLY L0, not a
corrupt input. Measured EXE words past the table: 0, 0 (zero padding),
then unrelated words (3000, 2500, ...). (4) Precedent: road.cpp:96,
scene.cpp:56,93, scenery.cpp:64 all throw "outside table";
actor_render.cpp:24 throws; traffic.cpp:272 `continue`s (skips the car).
No in-tree precedent for clamping. Decision for the owner: either extend
the table read with the original's zero padding (matches original
unchecked reads AND fixes CITY/MEDLY — recommended) or throw with the
course name like the siblings. NOT touched here: the visual session owns
course_scene.cpp.
Test + factory verdict: all 25 setups build, all step 600 ticks; only the
frame-build on CITY/MEDLY is blocked. Recorded for
`docs/COURSES_IMPLEMENTATION.md` (Task 1 tracker, not yet created).
