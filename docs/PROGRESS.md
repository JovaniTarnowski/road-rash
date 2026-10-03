# Port progress

Status of the native macOS ARM64 / Android ARM64 port of Road Rash (1996 Windows rerelease). The port is C++17 with SDL3 and uses no Wine or x86 emulation. It is a **playable race** (`rr-game`): the player and seven AI opponents on an original course, with the original bike physics, AI driving, rider contact and combat, drawn with the original rider sprites. Road shading is a placeholder, and there is no traffic, police or gameplay menus yet. Audio/video codecs and an explicit video playback path are implemented; race sound triggers and music selection remain open.

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
| Bike specs (`BIKESPEC.RSC`) | Decoded and applied (`0x00405ea0`) | `src/bike.cpp` |
| Player bike physics | Ported from the executable: throttle, brake, steering, gears, drag, traction, curve drift, movement along the course, vertical motion/jumps; not oracle-verified | `src/bike.cpp`, `src/race.cpp` |
| Walls, crashes, remount | Simplified stand-ins | `src/race.cpp` |
| Playable race (`rr-game`) | Single course, solo, keyboard/gamepad/touch, countdown, HUD, finish/restart | `src/game_main.cpp` |
| Rider sprites (`BIKERS/*.DAT`, colour remaps) | Decoded; drawn in the game; frame choice by inspection | `src/bob.cpp`, `src/game_main.cpp` |
| AI opponents | Racing state ported (targets, curve speed, steering/speed control, re-plan, rubber band); overtaking and fight states not ported | `src/race.cpp` |
| Rider contact | Detection and separation ported; contact response is the bike branch only | `src/race.cpp` |
| Combat | Hit test, damage, stamina, bike condition, knock-offs, steering knocks, weapon grabs; attack timing and AI attack rule are stand-ins | `src/race.cpp` |
| Traffic, police, gameplay menus, saves | Not started | — |
| macOS build | Standalone ARM64 `.app`/ZIP with static pinned SDL, system-only dependencies, ad-hoc signature; 23 release tests pass | `tools/build_macos_app.sh` |
| Android build | Debug and optimized development release APKs build/sign; runtime rendering/import/controls still unverified on hardware | `android/` |
| Platform lifecycle and timing | Pause on background/focus loss/controller removal, explicit resume, texture reset recovery, rational fixed-step clock | `src/session.cpp`, `src/game_main.cpp` |
| Multi-touch | Independent finger tracking, cancellation and brake priority | `src/input.cpp`, `tests/input_test.cpp` |
| Original road oracle | 660 original CITY frames match compared projection fields; indexed frame/palette capture works; other courses/splits unverified | `oracle/`, `tests/oracle_road_test.cpp` |
| Disc import | ISO/folder reader, SHA-256 verification of 341 files, first-run picker/progress UI | `src/importer.cpp`, `src/import_ui.cpp` |
| Android data packaging | Normal APK excludes game data; guarded developer assets are opt-in | `android/app/build.gradle` |

| Audio decoding/mixing | All 72 effects and 14 RRA tracks decode; queued stereo mixer, resampling, pause and device reopening; original triggers/priority/music streaming open | `src/wav.cpp`, `src/mixer.cpp` |
| Video | AVI scan, Cinepak and IMA: all 59 videos match FFmpeg RGB/audio; native playback via `--video`; frontend selection and hardware A/V sync open | `src/avi.cpp`, `src/cinepak.cpp`, `src/video_player.cpp` |

| Flat polygon raster | Normalized integer quad edge/span stage matches 64 original-function fixtures exactly; normalization, textures and scene integration open | `src/raster.cpp`, `tests/oracle_raster_test.cpp` |

## Timeline

Sessions 1–6: September 28, 2026. Later dates are listed per session.

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

### Session 5: playable Napa
- Initialised git (`main` holds the baseline; work on `port/playable-napa`) and added `rr::Assets` and `rr::Framebuffer` (plan 01 Task 2).
- Recovered 56 functions Ghidra had missed because they are only reached through behaviour tables (`tools/ghidra/ExportPointerTargets.java`), among them the bike think `0x00407a90` and the player rider think `0x00434660`.
- Traced the rider objects:
  - `0x00405990` builds a bike + rider pair from tables `0x00463ba8`/`0x00463c10`. Table words 3–6 become `+0xdc`/`+0xe0` (think)/`+0xe4` (move)/`+0xe8` (slice change).
  - The player's control word is `0x004b7ed0`, from `0x0044f800` (keyboard + joystick).
  - The simulation runs at 60 ticks/s (`0x004163d0`: counter frequency/`0x3c`), at most 20 steps per frame.
- Ported the riding physics and the race movement (`src/bike.cpp`, `src/race.cpp`), with tests (`bike_test`, `race_test`). A headless keep-to-centre bot finishes Napa in 7.2–10.0 minutes depending on the bike.
- Added `rr-game`, a playable race on any course, and switched the Android build to it.

### Session 6: opponents and combat
- Decoded the rider sprites. `0x004436a0` reads `Data/Bikers/{Hi|Lo}<name>.DAT`: per frame a width byte, a height byte, then indices. There are 326 bike frames and 45 cop frames; shadows use the same layout. Each rider's colours come from a 256-byte remap table at `0x0046e158` (one per rider), applied by `0x00443820`. The player now rides the original sprite with lean frames.
- **Opponents:**
  - Ported the AI racing state: `0x00407bd0` → `0x00409000` → `0x004090a0`/`0x004091f0` target choice, the `0x00409020` curve speed limit, `0x00409f70` steering/speed control, and the once-a-second `0x004082f0` re-plan with the `0x004659d4` rubber band.
  - The field comes from personalities at `0x00464558`, bikes from `0x004641c8`, rider values and stats from `0x00463c78`, and the level throttle scale from `0x004642a0`.
  - Headless: 7 opponents finish Napa at level 1 in 599–703 s.
- **Rider contact:** detection from `0x004368f0` and separation/impulses from `0x00436a40`, with the bike-against-bike branch of the contact handler `0x004049c0`.
- **Combat:**
  - Ported the hit test (`0x00409c10`), damage (`0x00409ad0`/`0x004052c0`), stamina (`0x004050c0`) and bike condition (`0x004051e0`), plus knock-offs, steering knocks and the backhand shove (`0x004096f0`).
  - Weapons: fists, club, chain and kick, with reach 5/7/8/5 × `0x1000` and damage multipliers 256/320/384/64.
- The game now has a 7-rider field, placings, attack keys (Z/X/C, gamepad X/Y/RB, touch top third) and stamina bars.

### Session 7: Android lifecycle and multi-touch (October 1, 2026)

- Added shared `SessionState` and `TickClock` for desktop and Android. SDL's lifecycle event watch updates atomic state immediately; renderer and race work stay on the game thread.
- Background/focus loss pauses the race, discards accumulated time and clears touch controls. Foreground/focus gain leaves a visible pause prompt; Enter, Start or a tap resumes. Android Back opens pause. P/Esc/Start toggles pause; Q quits on desktop.
- Active gamepad removal pauses. Render-device/target reset invalidates the sprite cache so sprites are recreated on demand. An unrecoverable render-device loss exits with an error.
- Replaced the single touch point with per-device/per-finger tracking: simultaneous steering and punching, independent releases, finger cancellation, and braking priority over touch auto-throttle.
- Replaced truncated nanosecond tick intervals with a rational 60 Hz accumulator and retained the 20-step stall cap. Tests cover ten seconds at 30/60/120/144 Hz, pauses and very large elapsed times.
- Android manifest declares optional gamepad/touch hardware and a resizable landscape activity.
- Native and sanitizer tests pass (9/9); Android ARM64 debug APK rebuilt. Pause screenshot inspected at `analysis/native-game-paused.png`.
- No Android device connected. Physical input, lifecycle and graphics reset recovery still need hardware validation. Plan 08 remains partial: import, settings/rebinding, saves/audio lifecycle and release packaging are not implemented.

### Session 8: original oracle and verified data import (October 2, 2026)

- Completed AWEMAN32 forwarding and in-memory frame hooks at four verified original call sites. The reference EXE stays unchanged in a separate oracle install. All sites are preflighted before any write; projected pointers are checked before logging.
- Captured 600 original frames. The scenario is named `napa-attract`, but selector 0 is **CITY** in the original (`0x0046df58`), unlike native CLI ordering. The initial apparent shoulder mismatch was a comparison-course mapping error; no road-code change was needed.
- `oracle-road` compares slice kind/sample, depth, scale, centre, surface row, road edges and outer shoulder edges. All 600 frames match exactly. Camera Q8 position spans 12,544–19,159 (samples 49–74). This does not verify other courses, splits, terrain, AI, physics or pixels.
- Added bounds-checked ISO 9660 parsing, SHA-256 and a transactionally staged importer. Path/hash metadata comes from the existing inventory; original file bytes are never embedded. Missing/mismatching files prevent commit; existing nonempty destinations are preserved.
- Imported all 341 files from the real ISO into `.tools/imported-game`; viewer smoke passed using that copy.
- Added first-run ISO/folder selection and asynchronous progress/error display. Android uses SDL's content-URI picker; physical SAF operation remains unverified. Screenshot/`--first-run-smoke` verifies the empty-data screen without opening a picker.
- Default APKs exclude research assets. Assembly checks actual APK entries. `-PdevAssets=true` permits local debug research only; release checks always forbid game data.
- 16/16 native tests pass. Android ARM64 debug builds and passes the no-game-data check. No device connected. Plans 01–08 remain active.

## Assumptions and approximations in current code

- **Rider commit:** after a split the preview assumes the rider has already committed to the selected branch, so the other road is dropped 82 samples in. In the game this depends on the bike's lateral position (`0x00439390`).
- **Collapse onto half 1:** the next bend comes from the kept road; the original reads a stale ring sub-slice.
- **Merge snapshot:** lane alignment uses the last sample's width instead of the terminal `NALR` record.
- **Camera:** fixed at height 150 on the centre of the route's own road. The original follows the bike.
- **Not oracle-verified:** the physics was ported from decompiled code and disassembly without an oracle comparison.
- **Walls and crashes:** the wall collision (`0x0043a610`) and the crash/remount sequence (`0x00403d60`) are simplified stand-ins: the rider is clamped at the shoulder edge, crashes above speed 4000, and remounts one second after stopping.
- **Terrain:** terrain type and flags are treated as 0 (building-style sides), because the terrain chunks are not decoded yet.
- **Not ported yet:** nitro (`0x0040b110`), attacks, the joystick analog path, and the original key bindings (`0x0040e5f0`). The defaults are Up/Down/Left/Right = accelerate/brake/`0x4`/`0x200`.
- **Race layout:** the player starts in grid slot 0 (`0x00464218`), and the race runs the whole route to the course end node.
- **HUD:** the speed shown is the internal speed ÷ 100; its real unit is not established.
- **Sprite frames** are chosen by inspection: lean frames 3–11 by steering; attack wind-up/strike frames per weapon (fists 44/45 and 51/52, kick 60/66, club 140–143, chain 101/102 and 105/106); crash frame 21. The original picks frames through the `RRAN`/`RRFD` animation system (`0x00445a60`, `0x0043ae90`), which is not ported.
- **Sprite scale:** opponents scale by depth relative to depth 512, capped at the player's size and baseline. The original sprite scale law and raster are not ported.
- **AI:** overtaking/avoidance (`0x00408e40`) and fight states 2–5 are not ported. AI riders attack the player when alongside, every 45 ticks (a stand-in rule).
- **Attack timing:** an attack lasts 18 ticks and connects at tick 7 (a stand-in for animation-driven hit frames).
- **Weapon grabs** use a simplified trigger: a fist hit on an armed rider succeeds 1 time in 4.
- **Contact response:** only the bike-against-bike branch of `0x004049c0`; the knock-off decision `0x004052c0` is not applied on contact.
- **Shading:** sky, verge, shoulder, road and lane-marking colours and the 4-sample stripe are placeholders. Painter's back-to-front drawing replaces the original's occlusion row (`0x4c10c8`).

## Not verified

- **No pixel comparison with the original's output.** Numeric road fields match the recorded starting stretch of CITY; frame pixels and other systems remain unverified.
- **Android has never been observed running on hardware.** Session 1's device was locked, and no device was connected in sessions 2–5. The APK contains native code; default builds import disc assets at runtime.

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

Import follow-up (October 2): nonseekable ISO input now caches inside the owned staging directory and the cached ISO is removed before commit. A synthetic FIFO import and cancellation/cleanup tests pass. First-run UI is now a CTest smoke check. Hardware SAF remains unverified.

### Session 9: original pixels and native audio/video (October 2, 2026)

- Extended the isolated original oracle with a verified hook at `0x004439f5` → `0x00450420`. Capture occurs while the 8-bit surface is locked, before presentation/dashboard. It writes raw view pixels, the active palette, coordinate shift and sub-slice fields. Original EXE bytes remain unchanged.
- `city-pixels` captures 60 frames plus raw frames 0 and 30. Both are 640×480 with pitch 640 and coordinate shift 15. Frame 30 was converted and visually inspected. All 60 compared road projections match exactly; this establishes capture, not native pixel equality.
- Corrected renderer tasks: `0x0044b630` and related functions prepare scenery; polygon fill is `0x0044d620`, spans `0x0044db30`, traversal/dispatch `0x0044d300`. `0x0040ffc0` converts coordinates rather than dispatching node types.
- Added strict PCM8/PCM16 and WAV IMA readers, including chunk padding, fact trimming, block/group validation and malformed/truncated input tests. All 72 effects and 14 RRA files decode.
- Added an SPSC queued stereo mixer with Q32 linear rate conversion, pan/volume/pitch, saturating sums, pause-preserved cursors, safe sound retirement and SDL audio stream/device reopening. Oldest-voice stealing remains provisional until original priorities are traced.
- Added AVI physical packet scanning, bounded RGB Cinepak codebooks/V1/V4/intra/inter decoding and WAV IMA audio. Missing or corrupt indexes do not affect sequential scanning. A truncated INTRO retains its complete packets. The 15 bike showcases are silent 320×172, unlike the 44 cutscenes at 320×192 with audio.
- Full disc codec comparison against FFmpeg passed for every RGB frame in all 59 videos and all 67,840,002 int16 samples in 44 audio streams. FFmpeg is a test oracle only and is never linked.
- Added native video playback via `rr-game --video VIDEO/INTRO.AVI`, SDL RGB texture output, skip, pause, lifecycle muting, audio cursor timing and silent-video wall-clock fallback. Native intro screenshot at ten seconds was visually inspected. Playback buffers are released only after their mixer references are retired.
- Original MCI presentation `0x00402820`: normal native-size mode uses x=160, y=240-height/2; doubled mode uses x=0, y=240-height, or y=20 when flag bit 0 is set. The current explicit player uses native-size mode. Frontend mode/clip selection and hardware A/V latency remain unverified.
- This milestone does not implement game sound triggers, original voice stealing priorities, streamed RRA selection/fades, MIDI, frontend cutscene selection, scenery/terrain raster, full AI/traffic/police or career/save behavior. The whole-port goal remains active.

### Session 10: standalone and release packaging (October 2, 2026)

- Added opt-in `RR_BUNDLE_SDL`: official SDL 3.4.8 source tarball pinned to SHA-256 `e9fff7467fb60f037e6708da18b25560649e4c63edc2a69bb871b960d9cbfbba`, static SDL, ARM64/macOS 13 deployment target and application plist.
- Built `build-release/Road Rash Native.app` (3 MB) and ZIP. Post-build copies licences/notices and ad-hoc signs the app. Verifier confirms ARM64, 22 Apple system libraries/frameworks only, strict signature validity and no game data. All 23 release tests pass. Bundle first-run screen was captured and inspected; executable also runs from `/tmp`.
- Added Android release signing configuration from owner-supplied properties/environment, tag-derived version name with research fallback, explicit `-PreleaseDebugSigning=true` for local optimized development APKs, and always-packaged SDL/stb licence notices. Default owner release remains unsigned without key configuration.
- Debug and optimized release builds pass APK game-data exclusion. Local development release signature verifies (v1/v2); only two licence assets are present. No production signing key was created or claimed, and no device is connected.
- Added `tools/ci.sh`, standalone build/ZIP script, bundle verifier and current-development `docs/USER_GUIDE.md`. Local CI reports `CI OK`.
- Video gamepad Start now pauses/resumes; active controller disconnect pauses, and desktop Q quits. Physical gamepad behavior remains unverified.
- Packaging is software-verified. Hardware testing, Developer ID/notarization, full original frontend/gameplay/audio behavior and final whole-port acceptance remain open.

### Session 11: original flat polygon filler (October 2, 2026)

- Ported the normalized edge/span stage of `0x0044d620` and the byte-fill behavior of `0x0044db30` to an indexed framebuffer. Q16 slopes, signed floor, wrap, inclusive bottom/exclusive span right, shared endpoints and degenerate points follow measured original behavior.
- Added synthetic coverage and an isolated original-function oracle mode. Loader mode `RR_ORACLE_RASTER=2` runs the pure original filler on a private buffer before graphics setup, records the original-normalized vertices, restores modified globals and closes a complete golden. Live mode 1 can run during the first locked draw. These fixtures are component tests, not full race images.
- 64 original fixtures compare with **zero pixel mismatches**: rectangles, diamonds, horizontal spans, negative clipping, fractional/rotated trapezoids, duplicate-point triangles and large ±16,384 coordinates. Golden images stay in ignored `analysis/oracle/`.
- Native and address/undefined sanitizer suites pass **25/25**. Resource/viewer smoke and all-course sweep pass; Android ARM64 debug builds with asset exclusion.
- Timed oracle cleanup now targets only the process it launched, without killing every Wine process in the prefix. Old scenario raw outputs are removed before capture to prevent stale fixtures from being mistaken for new output.
- Full polygon normalization, original scene/texture/scenery/terrain drawing and full-frame equality remain open. The game still uses the placeholder scene renderer.
