# Port progress

Status of the native macOS ARM64 / Android ARM64 port of Road Rash (1996 Windows rerelease). The port is C++17 with SDL3 and uses no Wine or x86 emulation. It is a **research viewer, not a playable game**.

Detailed evidence is in [REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md) and test results in [VERIFICATION.md](VERIFICATION.md). Build and run instructions are in [../README.md](../README.md).

## Status by area

| Area | Status | Where |
| --- | --- | --- |
| Executable analysis (Ghidra, 1,585 functions exported) | Done; types and names not manually validated | `tools/decompile.sh`, `analysis/decompiled/` |
| Disc inventory (341 files, SHA-256) | Done | `tools/inventory.py`, `analysis/inventory.json` |
| Resource container directory (`CRSR`/`LBTR`) | Done; all 13 containers validated | `src/resource.cpp`, `tests/resource_test.cpp` |
| Menu images (`.RRI` JPEG), dashboard and horizon `.BOB`, palette | Done (display only; no scrolling/cropping) | `src/viewer.cpp` |
| Course graph (`NOD `: roads, splits, merges, ends) | Done; all 5 courses | `src/course.cpp` |
| Course path (`HTPR` bend/rise) and lanes (`NALR`) | Done | `src/course.cpp` |
| Road projection (33 slices, original depth table) | Done; integer port | `src/road.cpp`, `tests/road_test.cpp` |
| Split/merge second roadway | Done, with a documented rider-commit assumption | `make_route`, `project_road` |
| Road sides/shoulders (`DLSR`) | Decoded; shoulder widths rendered, heights carried but not drawn | `src/course.cpp`, `src/road.cpp` |
| Terrain type per section (building / mountain / hill) | Type mapping traced; profiles not decoded | REVERSE_ENGINEERING.md |
| Other section chunks (`BORR`, `MSRR`, terrain type, `DZHR`, `CESR`) | Mapped to iterators only | REVERSE_ENGINEERING.md |
| Road colours/textures, horizon placement, scenery (`CEL `), sprites | Not started; viewer uses placeholder flat shading | — |
| Bike specs (`BIKESPEC.RSC`) | Directory only | — |
| Physics, AI, combat, race logic, audio, video, menus, saves | Not started | — |
| macOS build | Builds, tests pass, viewer runs | `run-macos.sh` |
| Android build | APK builds; runs to `SDL_main` on the device; rendering never checked on hardware | `android/` |

## Timeline

All dates September 28, 2026.

### Session 1: foundations
- Imported and analysed `ROADRASH.EXE` in Ghidra and exported address-keyed pseudocode, disassembly and a string index.
- Inventoried the disc and wrote a bounds-checked resource-directory reader with a malformed-input test.
- Traced the image and palette loaders and built the shared SDL3 viewer: 6 menu screens, dashboard, 2 horizons.
- Set up the Android Gradle/NDK project with the SDL3 AAR (hash-verified). The APK installed on a MANGMI AIR X and reached `SDL_main`; the device was locked, so rendering and touch were not checked.
- Started course decoding (`src/course.cpp`): section rows, `HTPR`, `NALR` and the node graph, validated on all five courses.

### Session 2: road geometry
- Recovered the semantics of the path bytes: bend is curvature (integrated twice), rise is a height delta (integrated once).
- Traced the road pipeline: slice ring (`0x0049e820`) → per-slice fill (`0x00454240`) → 33-slice projector (`0x00443d70`) → per-slice projection (`0x00444630`). Found the static non-linear depth table (`0x0046f098`), the fixed 320×240 view with horizon on row 90, and the camera height of 150.
- Ported the single-road projection in integer arithmetic (`src/road.cpp`). The depth table is read from the user's executable at runtime and never copied into the source.
- Added five course screens to the viewer, with auto-drive, speed and branch controls, `--position` and `--screenshot`.
- Added a synthetic projection test and a full-corpus sweep in `rr-course --exe`.
- Android now packages the 5 courses and `ROADRASH.EXE`; APK rebuilt.

### Session 3: splits and merges
- Mapped split/merge node fields and the lookahead traversal state machine (`0x00453030`): two cursors, separation slope/offset, and events.
- Also mapped the load-time 81-sample walk (`0x00453b60`) that rewrites window lengths and stores merge snapshots, and the collapse rule (`0x00452fd0`) with its rider-commit flag (`0x00476cd4`, set by `0x00439390`).
- Replayed that traversal along each route in `make_route` and ported the dual-road paths of `0x00443d70`. Verified that separation closes to zero at every merge on every course.
- Found that branches have independent height profiles and the original never tracks vertical separation. As a result, only the rider's own road lays ground bands in the viewer.
- Extended the tests and the corpus sweep; APK rebuilt.

### Session 4: section chunks and shoulders
- Mapped all eight per-section row chunks to their iterator init/step functions (table in REVERSE_ENGINEERING.md).
- Decoded the `DLSR` sides layout: pair A is shoulder width and pair B is shoulder height, both in world units with Q16 interpolation. Validated on all 389 records.
- Consolidated documentation (this file).
- Replaced the closed-form lane lookup with `walk_intervals`, an exact simulation of the original iterator step (`0x00452920`/`0x00452b70`), shared by lanes and sides. Lane widths changed on 6 samples (HIWAY section 6), now matching the original stepping.
- Shoulders are carried through projection (`RoadSurface::shoulder_left/right`) and drawn by the viewer; a projection test was added.
- Traced the terrain-type dispatch: building → type 0, mountain → type 1, hill → type 4. Only type 0 applies shoulder heights.
- APK rebuilt (5.3 MB).

## Assumptions and approximations in current code

- **Rider commit:** after a split the preview assumes the rider has already committed to the selected branch, so the other road is dropped 82 samples in. In the game this depends on the bike's lateral position (`0x00439390`).
- **Collapse onto half 1:** the next bend comes from the kept road; the original reads a stale ring sub-slice.
- **Merge snapshot:** lane alignment uses the last sample's width instead of the terminal `NALR` record.
- **Camera:** fixed at height 150 on the centre of the route's own road. The original follows the bike.
- **Shading:** sky, verge, shoulder, road and lane-marking colours and the 4-sample stripe are placeholders. Painter's back-to-front drawing replaces the original's occlusion row (`0x4c10c8`).

## Not verified

- **No comparison with the original's output.** No Wine capture at a matching position has been diffed against the viewer.
- **Android rendering has never been observed on hardware.** Session 1's device was locked, and no device was connected in sessions 2–4.

## Next steps

The complete agent-executable plans to finish the port are in [plans/00-INDEX.md](plans/00-INDEX.md) (plans 01–09, with order and dependencies). The immediate items below are the start of plan 02.

1. Decode the terrain-type payloads (`RBLD` per-sample byte pairs, `RMTN`/`RDWD` wall profiles `0x00451710`, `RHIL` control points `0x004517e0`) and the matching projection cases in `0x00444630`. Then draw shoulder heights for type 0.
2. Work out the `DLSR` `+16`/`+17` flag bytes and the per-sample `+0x148`/`+0x149` bytes, which are likely colour or texture selectors.
3. Find the original road/verge colours (renderers `0x0040b8e0`, `0x0040ccb0`, `0x0040ccf0`) and horizon placement.
4. Capture the original under Wine (`../play-roadrash.sh`) at known positions and diff against `rr-native --screenshot`.
5. Run the APK on the device once it is connected and unlocked.
6. Later milestones (REVERSE_ENGINEERING.md): scenery `CEL `, sprites, bike physics, AI/combat, audio, menus, packaging.

## Tool and command reference

| Command | Purpose |
| --- | --- |
| `./run-macos.sh` | Build and launch the viewer on macOS |
| `ctest --test-dir build --output-on-failure` | `resource-bounds` and `road-projection` tests |
| `build/rr-native --assets ../cdrom/ROADRASH --smoke` | Decode and render every screen once |
| `build/rr-native --screen 9 --position 960 --screenshot out.bmp` | Capture one course frame (screens 9–13: NAPA, CITY, HIWAY, CANYN, MEDLY) |
| `build/rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE ../cdrom/ROADRASH/DATA/COURSES/*.CRS` | Course graph, split/merge windows and positions, full projection sweep |
| `build/rr-inspect FILE.RSC` | Dump a resource directory |
| `python3 tools/check_corpus.py ../cdrom/ROADRASH` | Validate all resource containers |
| `python3 tools/prepare_android_assets.py ../cdrom/ROADRASH` then `android/gradlew -p android assembleDebug` | Build the APK (needs `JAVA_HOME` set to JDK 21) |
