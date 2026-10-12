# Port progress

Status of the native macOS ARM64 / Android ARM64 port of Road Rash (1996 Windows rerelease). The port is C++17 with SDL3 and uses no Wine or x86 emulation. It is a **playable game** (`rr-game`). The original front end (menus, Thrash and Big Game, characters, bike shop, Schmooze, restroom options, results, saves and credits) leads into races against seven AI opponents on the original courses. Races use the ported bike physics, AI driving, rider contact, combat and free traffic; none of these has yet been compared with the original tick by tick (plan 10 is building that oracle). Napa draws with the original scene pipeline: road, hills, buildings, shoulders, scenery, horizon scroll and full dashboard. The other four courses still use the placeholder renderer. There are no police. Effects, engines, the RRA soundtrack and all videos play; race MIDI is not done. Remaining work is planned in [plans/00-INDEX.md](plans/00-INDEX.md) (plans 10–15).

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
| Terrain profiles (building / mountain / hill) | All 109 sections decoded; stateful iterator matches 864 original slices; live cache/random integration open | `src/terrain.cpp` |
| Other section chunks (`BORR`, `MSRR`, `DZHR`, `CESR`) | Mapped to iterators only | REVERSE_ENGINEERING.md |
| CEL / PLUT and PC mip textures | 42 CEL wrappers, 117 indexed mip blocks match original loader; legacy packet drawing/table selection open | `src/cel.cpp` |
| World rendering, colours, horizon and scenery (`CEL `) | Napa: original scene construction (road, hills, shoulders, buildings, curbs, static scenery, horizon scroll) in the playable game, verified piecewise; split stitching/marked terrain open. Other courses and the viewer: placeholder shading | `src/course_scene.cpp`, `src/world_renderer.cpp` |
| Bike specs (`BIKESPEC.RSC`) | Decoded and applied (`0x00405ea0`) | `src/bike.cpp` |
| Player bike physics | Ported from the executable: throttle, brake, steering, gears, drag, traction, curve drift, movement along the course, vertical motion/jumps; not oracle-verified | `src/bike.cpp`, `src/race.cpp` |
| Walls, crashes, remount | Simplified stand-ins (plan 11 Task 4) | `src/race.cpp` |
| Playable race (`rr-game`) | Single course, solo, keyboard/gamepad/touch, countdown, HUD, finish/restart | `src/game_main.cpp` |
| Rider sprites (`BIKERS/*.DAT`, colour remaps) | Decoded; drawn in the game; frame choice by inspection | `src/bob.cpp`, `src/game_main.cpp` |
| AI opponents | Racing state ported (targets, curve speed, steering/speed control, re-plan, rubber band); overtaking and fight states not ported | `src/race.cpp` |
| Rider contact | Detection and separation ported; contact response is the bike branch only | `src/race.cpp` |
| Combat | Hit test, damage, stamina, bike condition, knock-offs, steering knocks, weapon grabs; attack timing and AI attack rule are stand-ins | `src/race.cpp` |
| Language strings (`TEXT/*.DLL`) | All five languages, original table assembly and continuations | `src/pe_strings.cpp` |
| Menu text | Disc TrueType fonts (Badloc, Futura); GDI hinting not reproduced | `src/text.cpp` |
| Front end (menus, Thrash/Big Game flow, shop, Schmooze, restroom, results, credits) | Ported from the EXE screen tables and handlers; mano-a-mano, jukebox, sound mix and controls screens not ported; not pixel-compared | `src/frontend.cpp` |
| Career rules and save files | Prize, fines, repairs, shop, level-up and the original 0xa0-byte save format; no original save round-tripped yet | `src/career.cpp` |
| Settings | Registry values as a preference file | `src/settings.cpp` |
| Traffic, police | Traffic spawning/placement ported with the original timers and vehicle library (not oracle-verified); movement/honk/drawing are documented approximations; collisions, DZHR scripted vehicles and police not started | `src/traffic.cpp`, `docs/ACTORS.md` |
| macOS build | Standalone ARM64 `.app`/ZIP with static pinned SDL, system-only dependencies, ad-hoc signature; 35 release tests pass | `tools/build_macos_app.sh` |
| Android build | Debug and optimized development release APKs build/sign; runtime rendering/import/controls still unverified on hardware | `android/` |
| Platform lifecycle and timing | Pause on background/focus loss/controller removal, explicit resume, texture reset recovery, rational fixed-step clock | `src/session.cpp`, `src/game_main.cpp` |
| Multi-touch | Independent finger tracking, cancellation and brake priority | `src/input.cpp`, `tests/input_test.cpp` |
| Menu gamepad navigation | D-pad/buttons and left stick with dead zone/repeat; existing controllers detected; interruption requires stick neutral; physical Android validation pending | `src/input.cpp`, `src/game_main.cpp` |
| Original road oracle | 660 original CITY frames match compared projection fields; indexed frame/palette capture works; other courses/splits unverified | `oracle/`, `tests/oracle_road_test.cpp` |
| Disc import | ISO/folder reader, SHA-256 verification of 341 files, first-run picker/progress UI | `src/importer.cpp`, `src/import_ui.cpp` |
| Android data packaging | Normal APK excludes game data; guarded developer assets are opt-in | `android/app/build.gradle` |
| Audio decoding/mixing | All 72 effects and 14 RRA tracks decode; queued stereo mixer, resampling, pause and device reopening | `src/wav.cpp`, `src/mixer.cpp` |
| Race sounds and soundtrack | Original effect slots, event codes at ported trigger points, engines with Doppler/pan/rpm, volume curve; RRA soundtrack cycling in menus; race MIDI (plan 13), sirens and car engines (plan 11) not done | `src/sound.cpp` |
| Video | AVI scan, Cinepak and IMA: all 59 videos match FFmpeg RGB/audio; native playback via `--video`; frontend selection and hardware A/V sync open | `src/avi.cpp`, `src/cinepak.cpp`, `src/video_player.cpp` |
| Flat polygon raster | Normalization matches 2,048 original calls; full flat path matches 64 indexed fixtures; scene integration open | `src/raster.cpp`, `src/polygon.cpp` |

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

### Session 20: front end, career and saves (October 2, 2026)

- Language DLL string tables: native PE resource reader and the `0x0042f5d0` table assembly (descending load, `0xff` continuations). All five languages give 985 slots. Language follows the original rule (`0x0042f340`), mapped from the SDL locale.
- The `TEXT/BADLOC|FUTB|FUTD|FUTR.DLL` files are TrueType fonts. Menu text now uses them through stb_truetype (pinned, unmodified, MIT/public domain).
- Career rules ported:
  - game state block `0x004b8a10`, mode `0x004753b0`, characters and starting cash;
  - default bikes; prize money for every finisher by place × (level+1);
  - fines (400 × (level+1)), repairs (price/10), game over when unpaid;
  - qualification bits and level-up/finale; bike trade-in at half price.
- Original save files ported: 0xa0 bytes with the `0x0042efc0` checksum and the relationship triples.
- Front end, driven by the EXE's own screen/item/popup tables:
  - navigation bytes, handlers, focus pictures, title/description/price boxes, darkened surfaces, button colours;
  - restroom Y/N enabling and popups (mode, level, name, resolution, biker detail, videos);
  - character bios with original units, Olley's showroom with bike videos, Schmooze lines from the relationship values;
  - results, penalties, standings rows, level-up, finale, and the scrolling credit rolls with colour codes and tab stops.
- `rr-game` now runs menus → videos (Intro, Start, results, bikes) → career races → results. The race ends 0xb4 ticks after it is decided (`0x00416010`). Quitting a race counts as wrecked, and opponents are named from `0x00464188`.
- Tests: 42/42 pass, including `career`, `frontend` (full scripted menu flow, rendered frames), `text`, `settings`. Android ARM64 debug APK builds without game data.
- Not done or not verified:
  - no pixel comparison with the original menus;
  - no original save round trip;
  - mano-a-mano disabled;
  - jukebox/sound mix/controls screens inert;
  - no attract demo;
  - native file list instead of Windows file dialogs;
  - interactive play-through on hardware not performed.

### Session 20 (continued): sound, resource unpacking, natural-frame renderer equality

- Race sound and soundtrack added (see the Audio rows in the status table and REVERSE_ENGINEERING.md).
- PKWARE-implode resource unpacking with CRC checks covers all 163 FAM records. Cel groups (CLGP) and dynamic rider textures are decoded.
- The oracle `dump_scene` writes the original's display list for each captured frame. Rendering that list natively reproduces 7 natural CITY frames with 0 differing pixels.
- The oracle DLL now has dump format v3: a memory snapshot before `0x00443d70`, one after the build, and the node list. It is built but **not yet captured with**. A `city-moving` capture (v1 DLL, 900 frames) was still running when the session ended.
- **Next:** port the scene build (`0x00443af0` subtree, `0x00443d70`/`0x00444630`, ring fill `0x00454240`) as a literal port over an emulated address space. Families need to load and relocate as `0x0044aa80` does, with templates resolved through `0x0044acd0`/`0x0044a870`. Compare emitted nodes against v3 dumps, then replace the placeholder race renderer. After that: HUD/dashboard, traffic/police (plan 05), race MIDI, controls/sound-mix screens, and device validation.

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
- **HUD:** the speed shown is the internal speed ÷ 100; its real unit is not established. The original dashboard is not drawn yet.
- **Race field:** seven opponents (personalities 1..7) regardless of the player's character; the original standings hold 15 rows (`0x004642d4` = 16) and the field composition is plan 05 work.
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

The plans to finish the port are indexed in [plans/00-INDEX.md](plans/00-INDEX.md), which was reviewed and rewritten on October 11, 2026. In order:

1. **Plan 10:** seeded per-tick simulation dumps from the original, and a native replay test; then fix physics, AI, combat and traffic until they match. Tasks 1–2 are done; Task 3 is in progress.
2. **Plan 11:** finish the opening Napa race:
   - actor animation and merged actor nodes;
   - remaining scene items;
   - crashes;
   - AI fight states;
   - police;
   - car engines and the mini dashboard;
   - a full-race acceptance test.
3. **Plan 12:** the other four courses and all levels; delete the placeholder renderer.
4. **Plan 13:** race MIDI with SBK banks, the sound mix/controls/jukebox screens, the attract demo, cutscene audit, save and menu verification.
5. **Plan 14:** Android on the MANGMI AIR X: device check script, frame-time benchmark, the `HiRes` setting, touch overlay, import/controls/lifecycle checklist, owner signing. Task 1 runs as soon as a device is attached.
6. **Plan 15:** acceptance script from a clean clone, macOS distribution, upgrade path, multiplayer decision, final docs.

## Tool and command reference

| Command | Purpose |
| --- | --- |
| `./run-macos.sh` | Build and launch the viewer on macOS |
| `ctest --test-dir build --output-on-failure` | `resource-bounds` and `road-projection` tests |
| `build/rr-viewer --assets ../cdrom/ROADRASH --smoke` | Decode and render every screen once |
| `build/rr-viewer --screen 9 --position 960 --screenshot out.bmp` | Capture one course frame (screens 9–13: NAPA, CITY, HIWAY, CANYN, MEDLY) |
| `build/rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE ../cdrom/ROADRASH/DATA/COURSES/*.CRS` | Course graph, split/merge windows and positions, full projection sweep |
| `build/rr-inspect FILE.RSC` | Dump a resource directory |
| `python3 tools/check_corpus.py ../cdrom/ROADRASH` | Validate all resource containers |
| `python3 tools/prepare_android_assets.py ../cdrom/ROADRASH` then `android/gradlew -p android assembleDebug` | Build the APK (needs `JAVA_HOME` set to JDK 21) |
| `npx rea-agents@latest function "../cdrom/ROADRASH/ROADRASH.EXE" <address> --format md` | Independent Ghidra cross-check of one function (needs Node 24 and `GHIDRA_INSTALL_DIR`; setup and limits in [TOOLING.md](TOOLING.md)) |

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

### Session 12: original view coordinate mapping (October 2, 2026)

- Added `ViewMode` from `0x00448870`: 320×240 low, 640×480 high, or a low view centered at (160,120) in a 640×480 display.
- Recovered `0x00410160` / `0x0040ffc0`: high mode shifts Q16 by 15 and masks with `0xfffffffe`. Vertices therefore lie on even pixels; simply shifting by 15 was insufficient. Low mode uses signed floor by 16 without masking.
- The isolated oracle calls the original conversion helper for twelve signed/boundary coordinates in each mode. All 24 mappings match. This is helper evidence; low-resolution race pixels remain unmeasured.
- Expanded raster input bounds to the complete converted high-mode range −65,536..65,535. A new extreme rectangle remains exact against the original; all 64 raster fixtures still match.
- Native and address/undefined sanitizer suites pass 26/26. Android ARM64 debug builds and asset exclusion pass. Scene integration remains open; the whole-port goal continues.

### Session 13: stateful terrain decoding (October 2, 2026)

- Added bounded `DLBR`, `NTMR` / `DWDR`, and `LIHR` payload decoding. All 109 sections in five real courses decode and traverse in both directions. `CourseSection` owns immutable terrain definitions rather than precomputing random samples.
- Ported original terrain initializer/step/export behavior: building selector pairs, two mountain wall profiles with three jitter values each, eight hill control points with interval slope generation, and six exported points/shading indices. Randomness is supplied by the caller, preserving state across steps.
- Added 864 original-function component comparisons: three terrain types, three fixed random values, forward and backward traversal over 48 samples. All exported selectors, wall values, hill points and gradients match without tolerance.
- Disc records require preserving physical order. CITY contains negative hill boundaries and an inverted initial 0..−6 interval; HIWAY/MEDLY contain out-of-order wall boundaries. The original advances only one boundary per call. Sorting or rejecting those records would alter the course.
- Structural tests reject truncated payloads, invalid extents/counts/tags, mismatched path samples and invalid hill parameter bounds. Native CTest passes 27/27; Android ARM64 debug build and asset exclusion pass. Sanitizer follow-up is recorded in VERIFICATION.md.
- Terrain is decoded and independently validated, but original traversal/cache random-stream integration, split road transforms, scene projection/texturing and full race pixel equality remain open. No complete terrain-renderer claim is made.

### Session 14: original hill projection (October 2, 2026)

- Added `project_hills` from `0x0041d6d0`: six Q16 vertices and maximum projected row, using explicit signed 32-bit wrap. At scale ≥65,536 the original applies ±0x40000000 x sentinels when wrapped signs indicate overflow; lower scales use direct wrapped projection.
- Added 96 original-helper fixtures covering six scales (including the branch boundary and synthetic signed extremes), signed world/camera offsets, odd/even view widths and six varied points per case. Every coordinate and maximum row matches exactly.
- Synthetic tests cover ordinary projection and the near overflow sentinels. This pure module is the start of scene projection, not an integrated world renderer. Original terrain state, occlusion, split transforms and texturing still require integration.

### Session 15: polygon normalization and full flat path (October 2, 2026)

- Ported `0x0044cd70` signed/wrapped cross classification, degenerate-edge propagation, and `0x0044ce60` vertex repairs, orientation, rotation, axis sorting and texture-flip mode. The original adjacency lookup is implemented as cyclic-edge logic rather than embedded table bytes.
- All 2,048 original normalization fixtures match vertices, acceptance byte and flip state. Coverage includes arbitrary quads, duplicates, collinear points, large wrapped cross products and flag modes 0/8/4.
- Added `draw_flat_quad` that normalizes before the verified filler (horizontal spans bypass normalization as in the original). All 64 raw fixture images match from both original input vertices and already normalized vertices.
- Hill projection milestone passes 28/28 native/sanitizer tests and Android build. Combined normalization verification is recorded in VERIFICATION.md. Textured spans, pathological rejected polygons and world/display-list integration remain open.

### Session 16: original indexed texture scan conversion (October 2, 2026)

- Ported opaque and index-zero-transparent spans for texture strides 1..256. All 1,008 original fixtures match, including all four output alignments, short/unrolled lengths, fixed/moving row coordinates and signed fractional increments.
- Ported `0x004184a0` edge interpolation and `0x00418760` scan conversion: reciprocal truncation, wrapped Q16 products, inclusive edge rows, negative/top/bottom/right clipping, endpoint replacement, per-row UV interpolation and transparency. Added the original two-axis minimum mip selection.
- All 64 original textured quad images match exactly, covering sloped edges, offscreen clipping, transparency and three distinct mip levels. This compares 19,660,800 indexed pixels. Mip data/UV selection is caller-owned; original resource/display-list integration remains open.
- Direct disassembly resolved a decompiler argument-order ambiguity: spans consume V as row and U as column. No golden expectation was changed to hide this mismatch.
- Golden reader now rejects integer overflow, duplicate fields/headers and noncontiguous frame IDs, with malformed-input checks.
- Polygon milestone passed 29/29 native/sanitizer tests and Android asset-free build. Combined texture checks are recorded in VERIFICATION.md. The full world renderer, original scene/resource preparation and sprite integration remain open.

Session 16 final validation: 30/30 native and sanitizer tests pass, Android builds asset-free, and viewer/resource smoke plus five-course sweep pass. The whole-port goal remains active.

### Session 17: CEL wrappers and indexed mip resources (October 2, 2026)

- Added bounded CCB/PLUT/chunk readers and the PC indexed `PDAT` / `RPDT` / `RRPD` resource layout. Dimension flags are interpreted using original `0x0044fb60`; mip pointers use the +28 relative base from `0x0040b7b0`.
- All 42 CEL resources decode: 17 indexed textures, 117 mip blocks, 25 retained legacy four-byte pixel words. Pixels in the PC texture blocks are already palette indices; a hypothetical 3DO packed-pixel conversion would be incorrect for these blocks.
- The original loader `0x0040b800` now runs on copied owned-disc resources in the isolated oracle. All CCB dimensions/flags, texture flags/levels and every pixel in all 117 relocated mip blocks match native output. Small legacy words match exactly but their final drawing semantics remain open.
- Tests reject every truncated length of a synthetic CEL plus invalid mip offsets and non-power-of-two stride. The loader oracle restores the original dimension table; it never patches disc bytes.
- Original course CEL selector tables, legacy packet interpretation, CCB transform semantics, palette use outside indexed mip blocks, animation and scene/resource integration are still pending. The whole-port goal remains active.

Session 17 final validation: 31/31 native and sanitizer tests pass; Android builds asset-free. Whole-port completion is not yet achieved.

### Session 18: original display-list rendering (October 2, 2026)

- Added `render_nodes`: original Q16 view conversion, ordered node traversal, bounding-box rejection, quality-mask flat dispatch, normalization/UV flips, inset UV coordinates, mip selection and indexed scan conversion. Callers own texture storage and display-list construction.
- CEL texture bounds are now decoded as signed shorts and verified against the original loader for all 17 indexed resources.
- The isolated oracle runs the actual `0x0040ff40` → `0x0044d300` path on 32 synthetic lists with 12 overlapping nodes each, both resolutions and quality masks 16/48/112. All **6,144,000 pixels match exactly**.
- An initial fixture incorrectly pointed nodes at a CCB wrapper. Disassembly shows `0x00410080` accepts the pixel-resource pointer directly. Correcting the fixture enabled actual textured dispatch; native expectations were not adjusted to conceal a renderer difference.
- Dynamic rider frame specialization (`0x00443820`), original scene construction/selector resolution, sky, natural full-frame equality and replacing the game's placeholder renderer remain open. Whole-port completion has not been achieved.

Session 18 validation: 32/32 native and address/undefined sanitizer tests pass. Synthetic dispatch tests also run without oracle captures. Android ARM64 debug build and asset exclusion pass; resource/viewer smoke and five-course projection sweep pass. No Android device pixel comparison was performed.

### Session 19: original road panels and imported materials (October 2, 2026)

- Ported `0x0043c170` lane-panel construction and `0x0043c0b0` material selection: byte lane counts, fractional Q8 lane widths, four-pixel outer expansion, Q16 row masking, marking direction, visibility/clipping, terrain-dependent PRE0 and quad UV flips.
- 128 original-function cases emit 343 panels. Every coordinate, material slot, PRE0 and flip matches exactly, including fractional lanes, reversed direction, negative steps, offscreen panels and hidden slices.
- Added bounded PE virtual-address data extraction. Road material IDs are read from the user's executable descriptor table at `0x0046dfb0`, then decoded from imported `GLOBAL.RSC`. The eight logical slots include the original `0x00444e30` marked-middle alias. No game tables/pixels are embedded in source.
- Combined original panel construction and actual imported CEL rendering match **24,576,000 indexed pixels** across 128 fixtures in both modes and all three quality masks. These are isolated synthetic road panels, not natural race-frame comparisons.
- Synthetic PE truncation/address checks and resource-binding/rendering checks run without game assets. Original scene cache/occlusion, terrain, scenery, horizon, player/sprite specialization and complete game renderer integration remain open. The whole-port goal remains active.

Session 19 final checks: 35/35 native, sanitizer and standalone release tests pass; Android debug and optimized development release build asset-free. Development APK v1/v2 signature verifies. Refreshed macOS `.app`/ZIP passes ARM64, system-library, signature and game-data checks. Viewer smoke and five-course projection sweep pass. No Android device is connected.

### Session 21: native and Android menu controls (October 10, 2026)

- Added left-stick menu navigation in the shared native/Android frontend: dominant-axis selection, 12,000 activation / 8,000 release thresholds, immediate direction changes, 350 ms initial repeat and 100 ms subsequent repeat. These are native usability choices, not recovered original behavior.
- Stick input requires neutral after lifecycle pause, controller removal, video playback or a race. Long frame stalls emit at most one repeat, avoiding a burst of menu navigation.
- Menus and videos now discover already-connected controllers, including controllers whose initial SDL added event was consumed by another screen. Menu button events are filtered to the active controller.
- Menu controller removal pauses and discovers a remaining controller. Returning from nested race/video loops also checks whether their consumed events disconnected the menu controller.
- Extended synthetic input coverage for signed axis limits, dead zones/hysteresis, diagonal dominance, reversal, repeat boundaries, stalls and neutral rearming. Updated the user guide to reflect implemented menu/career/audio behavior.
- Native Debug, address/undefined sanitizer and standalone Release suites each pass 44/44 tests. Viewer smoke, all-five-course sweep and frontend rendering smoke pass. Refreshed standalone macOS `.app`/ZIP passes packaging checks. Android debug and optimized development release APKs build with no game assets; release v1/v2 signatures verify. Details/logs are in VERIFICATION.md.
- Original disc mounted read-only for corpus verification. No Android device is connected; physical controls and picker/import behavior remain unverified. Full native scene construction, traffic/police and race MIDI remain open.

### Session 22: opening Napa family selection and hillside construction (October 10–11, 2026)

- User clarified the requested first level is the opening Napa race. Acceptance tracking lives in `NAPA_IMPLEMENTATION.md`; the complete race remains in progress.
- Added bounded native family hierarchy / CLGP decoding, preserving original resource offsets rather than relocated pointers. All 163 populated families decode into 577 groups and 786 indexed variants. Empty family records remain excluded. Terrain groups with the original unused-axis exponent sentinel decode without inventing an axis size.
- Captured the original Napa start through a test-only menu bootstrap, with an unmodified original executable. All 176 selections from 22 loaded groups match dimensions and mip-zero pixels. One-frame scene replay also matches all 15 emitted nodes and every final pixel; this replay does not prove live scene construction.
- Added the original near hill perspective subdivision (`0x0044bd40`) and hill-band construction / material-size calculations (`0x0041c390`, `0x0041cf60`, `0x0041d060`). All 128 subdivision fixtures match, and all 256 hill-band fixtures match 1,164 emitted quads and UV flips. Cases cover both sides, distance boundaries, terrain transitions, offscreen rejection, occlusion, seam corrections and imported overlap offsets.
- The bootstrap currently faults in the original surface-restoration path when attempting a multi-frame capture. Completed one-frame golden files are used for verification; the incomplete multi-frame capture is not accepted as evidence.
- Hill geometry and family selection are isolated shared modules. Live integration, shoulder/wall/building geometry, original scenery/traffic/police/music and full-race verification remain open. No Android device is attached.

Session 22 continuation:

- Ported shoulder construction (`0x0041d260`) and its distinct near subdivision (`0x0044c0e0`). All 256 original cases match all 527 quads and UV flips, including hill/wall transitions, edge expansion and quarter-pixel row masking.
- Added bounded CESR course events and MSRR family load/unload record decoding. All 7,949 Napa path samples match original event types, parameters and marking flags. Corpus decoding preserves empty event tables and signed negative starts used by other courses.
- Both branch routes retain the other road's source node, section and local sample. The five-course sweep validates 57,980 route samples and 3,586 dual-road references against their source sections; 1,200 family request records decode.
- The existing road projector also matches all 33 projected slices of the original Napa start (one frame, no mismatches). This still does not establish complete moving Napa scene equality.

- Corrected the original-game bootstrap to take the actual course menu exit. This removes the stale credits timer that caused the prior surface fault. The completed 121-frame stationary capture matches road projection and five display-list replays. A subsequent 901-frame driven capture advances camera position 12544→29004; all 901 road projections match. Moving display-list replay exposed an empty shadow frame producing invalid native UV storage; that integration regression is under repair.
- Added an owned `CourseScene` composition module: imported horizon, road materials, structural family resolution and deterministic native terrain generation. A full Napa branch sweep emits geometry for 2,509 views and resolves every hill material over all 7,589 hill samples. Its independent indexed preview renders the original road textures and Napa hills/horizon. Building/marked terrain, split stitching, resource timing and full-race random-call interleaving are still incomplete; this renderer is not yet substituted for the playable renderer.

Session 22 playable renderer continuation:

- Moving Napa display-list replay now matches every pixel in frames 0, 150, 300 and 750. Frame 750 has 130 original nodes; five empty shadow frames are safely omitted. Its original static scenery image `GRVP2SM2` revealed that non-power-of-two mip heights retain the final partial row. The decoder now rounds mip heights upward; a synthetic truncated-row check covers this boundary. Older CEL oracle files captured only floored rows and continue to verify that prefix; the oracle exporter now captures complete rows.
- A native route raster sweep exposed fractional UV underflow on a thin `HILL13A` quad at sample 825. A new original-function fixture reproduces its exact coordinates and confirms the original unsigned span address wraps to byte 65,535. Native mip objects retain available bytes from their owning resource (up to the original 16-bit address extent); no invented padding or coordinate clamping is used. The 33-list original raster fixture matches every pixel. This preserves reads within the imported backing resource; it cannot reproduce reads outside that owned resource or process-specific relocated pointer bytes.
- Both Napa branches now pass 2,509 geometry views; 503 sampled views also render fully without storage errors. All 7,589 hill samples resolve their materials. This validates safety/material coverage, not full original scene equality or complete building/scenery support.
- The playable Napa race now uses the imported indexed road/hill/horizon composition, uploaded through a nearest-filtered SDL streaming texture. Career starts and quick races load owned scene resources; renderer reset events recreate the texture. Other courses retain their previous renderer. The scene's fixed horizon scroll, buildings/marked terrain, split stitching, scenery placement, terrain collision integration, rider animation/dashboard and traffic/police/music still require work.
- A 600-tick autopilot gameplay screenshot succeeds with the new renderer (`analysis/napa-playable.bmp`). Full race acceptance remains unchecked. User scope remains the complete opening Napa race; no completion is claimed.

Further full-route raster coverage and dashboard work:

- Increased raster coverage from every 25th sample to every 5th sample: all 2,509 Napa branch views render. The denser sweep found an original-style unsigned read beyond the end of one imported family's allocation. Native backing storage now defines unavailable bytes as zero guards, while preserving available imported bytes. This is an explicit native safety behavior for process-dependent original heap reads; it is not claimed as pixel-identical outside owned resource storage. The earlier 'no invented padding' statement applies only to the verified thin-quad fixture whose addressed byte is present.
- A 60,000-tick bounded autopilot run reaches Napa's finish in first place at 9:49 with the live renderer. This covers simulation completion and a final rendered frame; it does not cover rendering every simulation frame, scenery/traffic or original completion parity.
- Added the original high-resolution full dashboard: actual imported RAT/SUPER/SPORT mph/metric artwork, complete-row height returned by the original image loader (RAT is 119 rows, despite the loader's 122-row maximum), imported layout table, player/place/distance text, stamina/condition/target meter selections, speed/RPM angle formulas and needle geometry. Font height/face are recovered from the original constructor (32-pixel Badloc). Font hinting and pen rasterization retain known native differences. Mini dashboard/opponent-distance indicator and original post-dashboard frame comparison remain open.
- The shared playable loop composites the transparent dashboard after riders and recreates its texture on renderer reset. Career units/player/opponent names feed the dashboard. Quick race defaults to mph. The player sprite now follows its projected course sample rather than a fixed screen-bottom baseline; full original animation/hotspot projection remains separate work.
- Synthetic meter boundaries and 24 imported dashboard combinations (three bike classes, two units, four meter levels) pass. The full Napa acceptance list remains unchecked while remaining systems are ported.

Animation and Napa scenery continuation:

- Added bounded RRAN/RRFD animation decoding, including indexed frames, legacy references, hotspots and conditional collision metadata. The imported family corpus decodes 1,663 animations and 6,503 frames (6,066 indexed frames); stale unused original collision pointers are retained without dereferencing them.
- Decoded BORR roadside profiles and DZHR placed-scenery records. Native roadside generation matches all 1,024 samples from 32 direct original-function cases. Static scenery uses the imported category-0/category-4 animation selectors, perspective scaling, hotspots and mirror flags. Moving DZHR records still require traffic integration.
- All 6,471 generated Napa scenery objects resolve indexed frames. Both branch routes render all 2,509 sampled windows, emitting 624,832 nodes without raster errors. Ground interpolation and isolated terrain/scenery random ordering remain integration approximations requiring original comparison.
- The playable screenshot `analysis/napa-scenery.bmp` shows imported trees/fences, course terrain, projected player and dashboard. Buildings, marked terrain, split stitching, horizon motion and remaining race systems are still open.
- Verification: 50/50 native tests and 50/50 address/undefined-sanitizer tests pass; Android debug and release assemble successfully. Logs: `analysis/napa-scenery-native-tests.log`, `analysis/napa-scenery-sanitizer-tests.log`, `analysis/android-napa-scenery-build.log`. Android hardware validation remains unavailable.

Napa buildings and ground continuation:

- Imported building material groups now provide their original nominal heights and roof-cut test. Added original facade, flat roof and exposed end geometry: all 128 direct original cases match 145 emitted nodes, including UV flags and offscreen/absent-building cases.
- Ported ordinary raised building shoulders and curbs using their original category-5 frames and perspective subdivisions. All 128 direct original cases match 429 nodes. Building marking transitions remain open; ordinary faces are connected to the playable Napa scene.
- Added building/hill terrain ground lookup and connected the imported terrain source to race physics and scenery grounding. Direct original ground fixtures are being checked; full race behavior and random-call ordering still require verification.

- Terrain grounding matches all 2,048 direct original samples, including hill-band interpolation, signed slopes, shoulder boundaries, absent buildings and marked flat ground. Both Napa branches still rasterize all 2,509 views after integration (642,765 emitted nodes).
- Added original camera heading accumulation and signed horizon scroll formulas to the live race. The horizon now moves as the course bends; original page-copy scroll direction is preserved. Integrated trajectory comparison remains open.

### Session 23: free traffic on Napa (October 11, 2026)

- Plan 05 task 2 (traffic): actor census in `docs/ACTORS.md` and the traffic foundation ported in `src/traffic.hpp/.cpp`:
  - `VehicleLibrary` loads the course's `DATA/CARS/<name>.CAR` exactly as `0x004465f0` does (ANIM/CANS pairing, sequential CANS, mask from the SNAC cel block). NAPA decodes 10 vehicles with 12 textured RRAN frames each; masks measured from the real cel flags.
  - Spawn timers `0x0044dd00`/`0x0044dd90` with the original integer arithmetic (rate 2500, density 6, reload 3515), ring cursors front/back (camera ±0x5000/0x3000), signed-char lane counts, exact placement `0x0044df60` (lateral, mask filter with the last-vehicle exclusion, four random bytes, serial) and the 16-object pool `0x00438b70(0x210,0x10)`.
  - Movement is a documented approximation of `0x0044e710`: cruise target `((25·phase)>>8)·100`, rate `phase·0x12>>8`, backward kind 2, lane-centre steering with the per-kind drift table, despawn outside `[back, front)`; honk `0x12` when the player blocks the car's path (`0x0044efa0` approximation). Obstacle following/lane changes are not ported.
  - Two original-arithmetic bugs found and fixed during verification: the cruise constant is 25 (`DAT_004b7e8c`), not the timer rate 2500, and the phase byte is a full int slot (192..319) whose uint8 truncation froze cars.
  - `Race::set_traffic` steps traffic inside the tick with the per-step camera advance; `rr-game` draws cars through the verified scenery projection between the composed scene and the rider sprites. Traffic runs on every course with a CARS container; composed drawing only on Napa.
- Verification: 53/53 native tests (`traffic` suite added: exact timer/lane/placement math, pool limit, movement, horn, corpus decode of the real NAPA.CAR and live spawn/draw on the Napa route) and 53/53 address/undefined-sanitizer tests pass. Autopilot screenshot `analysis/napa-traffic.bmp`; a frame diff against a CARS-less assets tree confirms the traffic path changes the render (the shared rand stream also shifts rider AI, as in the original). Android debug and optimized development release APKs build asset-free. No Android device is connected; hardware checks remain open.
- Traffic is **not** oracle-verified (no tick-level object dump exists yet); the census is static. Rider-car collisions, DZHR scripted vehicles/police, and the original car animation/scaling laws remain open.

### Session 23: port review and replanning (October 11, 2026)

- Reviewed the native and Android ports against the plans. Native Debug CTest passes 53/53, including the `traffic` test (traffic was uncommitted when the review started and was committed as `51cb898` by a parallel session during it). No Android device is attached (`adb devices` is empty).
- Findings:
  - Plans 02–07 assumed a `World`/`Object` architecture that was never built.
  - The oracle could not seed, replay inputs or dump objects per tick, so no simulation system is verified against the original. Plan 10 addresses this; a parallel session completed its Tasks 1–2 (`ab8cc79`, `37c4597`) during the review.
  - Only Napa uses the original renderer.
  - The race view ignores the `HiRes` setting.
  - Android has never run on hardware.
  - `/Users/archit/Dev/rr/android` is an older Wine-on-Android setup with a patched EXE, not part of the port.
- Rewrote [plans/00-INDEX.md](plans/00-INDEX.md) with current status, findings, task status per old plan and a new order. Added status banners to plans 01–09.
- Wrote new plans 10–15: simulation oracle, Napa completion, all courses, music and front end, Android on hardware, release and acceptance. Refreshed this file's header, status table and next steps.

### Session 23 (tooling addendum): REA setup and analysis notes

- `npx rea-agents@latest` (REA 6.4.0) now works on this Mac: Node 24 via nvm (the system Node 25.8.1 line is unsupported) and `GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec` with JDK 21. Setup steps, commands and evidence limits are in [TOOLING.md](TOOLING.md); `rea doctor` stays `healthy: false` overall because Hopper/IDA and agent registrations are optional and unconfigured, which does not block the Ghidra path.
- REA's Ghidra backend re-analyzed `0x0044dd90` and produced pseudocode identical to the project's own export — an independent confirmation of the `tools/decompile.sh` pipeline. REA is for spot-checks (one to two minutes per function); the project pipeline remains the bulk source.
- TOOLING.md also collects the session's transferable analysis lessons: Ghidra word-scaled pointer arithmetic, mis-decompiled helpers needing the raw disassembly (`0x0040b5f0`), int-sized object slots (the car phase wrap bug), reversed resource tags, the `CRSR`/`LBTR` fixture layout, runtime-initialised EXE words versus embeddable static tables, the single shared MSVC rand stream (and why feature-diff pixel comparisons diverge globally), and tracing constants to their exact address (`DAT_004b7e8c` = 25 versus the 2500 spawn rate).
- Workflow lesson recorded for concurrent sessions: stage with explicit pathspecs, not `git add -A` (two interleaved commits already happened: `37c4597` and `51cb898`), and check file mtimes for active writers before editing shared docs.

### Napa actor animation continuation with REA (October 11, 2026)

- At the user's request, used `npx rea-agents@6.4.0` with supported Node 24, JDK 21 and the Ghidra provider to analyze `0x0043ae90`. The canonical evidence file is `analysis/oracle/rea-actor-animation.json`; its pseudocode matches the existing original-function export. REA confirms static control flow, not runtime parity by itself.
- Added the owned actor animation playback module, separate from RRAN graphics decoding. It ports direct/fallback selection, queued animation, signed frame durations, unsigned accumulated deadlines, loop frame/count and the pre-advance frame retained for drawing. Missing/invalid native sequence references are bounded.
- Captured 64 private original-function scenarios of 32 steps each. All 2,048 animation states match, including deadline, current/queued sequence, current frame, loop count and retained draw frame. Fixture: `analysis/oracle/napa-actor-animation-fixtures.tsv`; verification: `build/actor_animation-test`.
- Graphics descriptor construction from CANS/EXE tables and live actor node integration remain open; hand-picked rider animation is not claimed replaced yet. Existing in-flight simulation replay corrections were preserved.

- Added bounded CANS descriptor construction (`0x0040b200` / `0x0040b6a0`) and imported rider/cop animation banks. All 194 descriptors in 59 original initializer bindings match every non-pointer metadata byte. The two uninitialized rider slots remain unavailable, as their original BSS descriptor draw pointers are zero. Graphics RRAN data is owned alongside the banks.
- The parser checks complete chunk extents and descriptor tables; truncated CANS and malformed source descriptors fail. Runtime animation/bank integration and merged actor nodes remain the next step.
- Animation playback regression gate: 56/56 native and 56/56 sanitizer tests passed, and asset-free Android debug/release assembled. Logs: `analysis/napa-actor-animation-native-tests.log`, `analysis/napa-actor-animation-sanitizer-tests.log`, `analysis/android-napa-actor-animation-build.log`. Those gate logs precede the added descriptor suite.

Actor graphics follow-up: the owned CANS decoder resolves all 672 view cells from the two rider/cop graphics banks to imported RRAN frames and flags. Captured `0x0040b6f0` calls match every cell. Native `select_actor_view` ports the yaw/elevation selection in `0x0043af60`; 9,312 original calls across all 194 descriptors and 48 angle combinations match the selected image, flags, roll correction and hotspots. Synthetic oracle calls restore shared CCB metadata. Actor playback remains awaiting live dispatch integration. Added the axis-aligned `0x0044ba40` actor projection implementation and a 256-case oracle probe; validation is underway. The previous descriptor gate passed 57/57 tests in native and ASAN/UBSAN builds, plus Android debug/release builds; it predates the view selector and projection additions.

Actor projection/orientation gate: 256 original `0x0044ba40` cases yield 235 accepted nodes with every corner matching. The same fixtures verify `0x00413ae0` camera-relative angles across lateral positions, negative Q8 division, road headings and rider depth doubling. Added `ActorNodes`, which owns 451 populated rider/cop/shadow Bob pages and skips 291 original empty frames; cached texture pointers remain stable across remap insertions. It selects the imported descriptor view, emits terrain-ground shadows first, then vertically displaced bodies, retaining RRAN logical dimensions while sampling HI Bob textures. Corpus exercise emits 311 nodes from all rider descriptors. This is a reusable node builder, not yet the live race draw pass; original triggers, render interpolation and merged scene ordering remain outstanding.

The actor-node regression gate passed 59/59 tests in native and ASAN/UBSAN builds; `tools/ci.sh` completed imported-resource smoke/course checks and asset-free Android debug, and Android release assembled. Logs: `analysis/napa-actor-nodes-ci.log`, `analysis/napa-actor-nodes-sanitizer-tests.log`, `analysis/android-napa-actor-nodes-release.log`. That gate predates normal riding trigger additions. The normal-riding branch of `0x00407f30` and queued fallback `0x0040aca0` are now implemented; 1,024 original player animation steps across speed/throttle/lean/boost thresholds and state transitions match state and selected descriptors. Crash-specific dispatch, combat event timing, and live integration remain open.

Live Napa rider integration: Game owns the imported actor library and simulation-tick animation states. Normal riders now render through indexed `ActorNodes` within each roadway slice, sorted by longitudinal position and inserted before/after midpoint scenery according to their Q8 fraction. Road projection now retains the original tangent angle used for view selection. Imported EXE attack code tables replace inspected attack frame numbers for normal riders. The existing combat timer currently queues return-to-riding through the original playback mechanism; original combat event timing remains unported. Crashed riders retain the old draw path pending crash actor dispatch. Traffic remains a separate pass pending a unified object chain, and split-road dispatch/interpolation still need original full-scene validation. Visually inspected grid and 3,600-tick moving captures. A 50,000-tick bounded run reaches FINISHED 1ST at 9:48 (`analysis/oracle/napa-live-actors-finish.png`), which proves the native course can finish, not overall original parity.

Live actor initializer correction: `0x00446060` sets AND queues riding code 2 before its initial advance. Preserved that queued state in Game so the first attack can return through the original descriptor control rather than loop indefinitely. The existing combat timer bridge remains for subsequent attacks until hit/event scheduling is ported. No original art or golden snapshots are bundled into the game.

Live rider checkpoint verification: 59/59 native and ASAN/UBSAN tests passed, CI imported-asset checks and Android debug passed, and Android release assembled. After the combat-return bridge and queued initializer correction, native/ASAN 3,600-tick rendered smoke runs and both Android variants rebuilt successfully. Final logs: `analysis/napa-live-actors-ci.log`, `analysis/napa-live-actors-sanitizer-tests.log`, `analysis/napa-live-actors-final-sanitizer-smoke.log`, `analysis/android-napa-live-actors-final-build.log`. No Android device is attached; hardware behavior remains unverified.

Actor checkpoint committed as `fda8b2a` (original playback and live Napa normal rider display nodes). Preserved the other session's pending simulation changes. Vehicle follow-up: `0x004465f0` missing-CANS fallback reads the previous animation pair's CANS pointer (`0x474db0 + vehicle*8`), rather than leaving a zero mask. Added that inheritance and optional imported EXE construction of the three vehicle descriptors (`Cars`, `Car1`, `Car2` at `0x474da0/8/b0`) plus owned CANS view groups. A new private original capture checks all ten Napa vehicle masks, view cells and descriptors; validation is underway. Existing synthetic traffic tests retain the lightweight mask-only loader, while the live game supplies the imported executable for full descriptor validation.

Vehicle graphics runtime validation completed: all 207 imported CANS cells, all 30 descriptors, all ten masks and 1,440 original view selections match. Live Napa cars now use those descriptors, original camera-distance code selection (0/1/2), and `0x00446830`'s size-0x500 actor billboard path. Game merges normal riders and cars by longitudinal position within each road slice and around midpoint scenery; the separate traffic draw pass is removed from Napa presentation. Inspected `analysis/oracle/napa-live-car-nodes.png`. Current traffic movement still approximates obstacle braking/lane changes; heading derives from its current direction rather than verified full movement/interpolation. Crashed riders and split-road object dispatch still need integration. Legacy `traffic_draw_nodes` remains for existing standalone inspection/tests that construct a mask-only library; the live game loads full EXE descriptors.

Live car checkpoint verification: 60/60 native and ASAN/UBSAN tests passed; imported-asset CI smoke/course checks and Android debug passed; Android release assembled; sanitizer 3,600-tick live rendered smoke passed. Logs: `analysis/napa-live-cars-ci.log`, `analysis/napa-live-cars-sanitizer-tests.log`, `analysis/napa-live-cars-sanitizer-smoke.log`, `analysis/android-napa-live-cars-release.log`. Original movement, crash/recovery and whole-scene parity remain acceptance work.

Car actor checkpoint committed as `586169a` (imported car views and merged Napa actor nodes). REA follow-up completed: `npx rea-agents@6.4.0 function ... 0x00403d60 --format json` produced `analysis/oracle/rea-crash-entry.json`; its pseudocode exactly matches the existing Ghidra export. Traced crash entry's separate rider/bike motion objects, reason-dependent start animations, collision dimensions, impulses, and later recovery. Added the crash-bike branch of `0x00407a90` to the playback module: stopped bike code 48, random initial 46/47 and queued rolling code 44. Runtime fixtures cover 32 scenarios of 32 steps using the original deterministic replay-random mode, restoring those globals afterwards. Validation is underway; this animation module is not yet live crash physics/recovery.

Crash animation probe correction: the first scratch bike used owner -1, which falls through to original `0x0040a410` and expects a linked rider; the isolated oracle process faulted. Corrected the scratch ownership to the local non-cop path, regenerated a complete fixture, and all 1,024 crash-bike animation steps now match. Added the separate crash-rider branch of `0x00407f30` (fall/roll/stand animation queues and crash state 1→2 when stopped), with a 2,048-step original probe; validation is underway. Full crash entry, independent motion and remount AI are still remaining work.

Crash entry numeric gate: 64 isolated original `0x00403d60` cases now match all 49 tracked fields on both bike and rider, including existing-crash rejection, reason-specific animation, optional independent impulses, launch heights, collision dimensions, cleared motion/control fields and byte flags. The fixture supplies a scratch linked bike/rider and a nonzero landmark to avoid traversing the live course; route/network bookkeeping is outside the native numeric entry routine. Native `CrashConfig` imports the original rider behavior template and crash bike dimensions. All 2,048 crash-rider and 1,024 crash-bike animation steps also match. No live crash path is claimed replaced until independent motion and remount behavior are integrated.

Independent crash actor motion now matches 512 original `0x00439920` cases: momentum transfer, velocity projection, previous-position copy, longitudinal integration and lateral integration. The route callback is tested against a flat private ring; full terrain/wall/contact physics is separate. Offline `0x0040a600` recovery decisions match 256 original cases, including walking direction, strict bike proximity thresholds, delayed automatic remount, animation queues and AI condition restoration after wrecking. Player input-directed walking, route-bound wreck checks, police ownership and the remount reset remain integration work. Private fixtures are regenerated in `analysis/oracle/napa-crash-entry-fixtures.tsv` and never bundled.

Crash module checkpoint before the motion/recovery additions: 61/61 native and ASAN/UBSAN tests passed, imported-asset CI and Android debug passed, Android release assembled. Logs: `analysis/napa-crash-modules-ci.log`, `analysis/napa-crash-modules-sanitizer-tests.log`, `analysis/android-napa-crash-modules-release.log`. Motion/recovery have passed their original-case native comparisons; broader gates will run after live integration.

Remount reset (`0x00404560` / riding copy `0x004067d0`) matches 64 private original cases across 34 fields on each actor and both animation states. The first scratch probe missed the rider's linked bike pointer and faulted in landmark bookkeeping; corrected that link and regenerated the complete fixture. Native reset restores imported normal collision dimensions, resets drag/control fields and queues both actors' idle code; Race restores current stamina separately.

Live Napa crash integration now owns a separate rider motion object and bike/rider playback state, imports the original crash constants, steps independent motion, runs the verified recovery decisions and remount reset, and emits both actors through the merged scene chain. A seeded native Napa scenario separates the actor positions and remounts after 434 ticks, checking restored drag and collision dimensions. Standalone Race clients without an imported actor library retain their synthetic fallback. Full original terrain walls/contact physics, player directed walking/route-bound wreck checks, police interaction, interpolation and split crash tick goldens remain open; this is not full crash parity. Broader native/sanitizer/Android gates are running.

Live crash checkpoint verification completed: 61/61 native and ASAN/UBSAN tests pass, imported-asset CI passes, asset-free Android debug and release assemble, and the sanitizer 3,600-tick rendered run passes. Inspected `analysis/oracle/napa-live-crash.png` (a normal moving frame; the separate crash integration test supplies crash/remount coverage). Logs: `analysis/napa-live-crash-ci.log`, `analysis/napa-live-crash-sanitizer-tests.log`, `analysis/napa-live-crash-sanitizer-smoke.log`, `analysis/android-napa-live-crash-release.log`. Other session's pending simulation/grid corrections remain separate.

Crash checkpoint committed as `63ad4ad`. Police follow-up: actual `npx rea-agents@6.4.0 function ... 0x00406b80 --format json` completed (`analysis/oracle/rea-police-spawn.json`); its pseudocode exactly matches the prior Ghidra export. `0x00407f30` checks offline cop spawning every 4096 ticks (including a zero tick if called), without a difficulty restriction. An explicit isolated original race-ring constructor capture (`RR_ORACLE_POLICE=1`, Napa level 1) successfully creates a cop: bike index 4, class 4 bike/class 8 rider, strength 6, stamina 2200, rider value 171/2, pursuit base speed 120 and target relationship 48000 for the captured player place/personality. New bounded `PoliceConfig` imports the templates, profile 9, rider row 15, bike indices and eight speech IDs; `police_pursuit` matches selected original relationship/speed fields and stats. Spawn and arrest threshold boundary tests pass. This is configuration/decision work; cop chase, collisions, sirens, rendering and busted outcome integration are not yet live.

An original arrest-decision probe is being repaired after an isolated Wine fault. It temporarily substitutes only the nearby-cop lookup after verifying its five original entry bytes, then calls the actual decision/ending routines. Its output is not accepted until a complete capture finishes and the temporary entry/global state is restored. Valid crash fixtures were preserved separately.

Original arrest-decision capture completed with all 1,024 cases and explicit restoration markers for the substituted lookup bytes, instruction cache and game globals. Native `police-test` matches every local arrest outcome and the original 180-tick delay, including normal/crashed speed boundaries, cop/rider longitudinal distance, relative speed, rider exclusion flag, missing lookup result and owner -1 skipping the local outcome. This validates the decision after cop lookup; it does not validate the lookup traversal or live cops. Added restoration checkpoints after earlier isolated faults; a repeat capture is running to check probe stability before accepting further cop work.

Police arrest repeat capture also completed and restored its hook; all recorded decision/restoration rows are identical to the first successful capture. Police and crash scoped tests pass under ASAN/UBSAN (`analysis/napa-police-sanitizer-test.log`), including the live native crash/remount regression. Cop runtime integration and the broader gate remain subsequent work.

Police actor local constructor now matches the original captured numeric bike fields and valid rider fields, using imported behavior callbacks/constants, level bike/throttle selection, reduced AI low gears, condition, stamina, weapon and cop-bank animation initialization. Road ground and initial lane target are supplied by the caller; scheduler links and AI replan/search remain outside this helper. Corrected the capture to honor the original rider pool's 0x208 allocation (`0x00405490`) rather than reading the larger bike layout past it; regenerated the level-1 cop constructor fixture. The full 62-test/native/sanitizer/Android gate is running. No live cop integration is claimed yet.

Directed crash recovery now matches 256 original player walking cases, covering all 16 direction-bit combinations, signed longitudinal targets, strict proximity thresholds, heading/speed selection and resetting the recovery timer when walking input applies. Race maps shared throttle/brake/steering inputs to the original walking directions only during crash recovery, so keyboard/controller/touch use the same implementation. This does not yet add the route-bound wreck branch. Existing 256 offline recovery cases, 512 motion cases, 64 entries/resets and the live separation/remount scenario still pass.

The police actor full native gate found a stale synthetic grid assertion from the parallel simulation edits: the EXE's slot-0 offset is 16384, while 18176 is the measured Napa initial lateral including road-base 1792. Corrected the flat synthetic race test to use the raw 16384 offset. The simulation session's road-base/demo changes remain separate. Full gate will rerun after this correction and the walking additions.

Police local actor/player walking checkpoint gate completed: 62/62 native and ASAN/UBSAN tests pass; imported-asset CI passes; asset-free Android debug/release assemble. Logs: `analysis/napa-police-walking-ci.log`, `analysis/napa-police-walking-sanitizer-tests.log`, `analysis/android-napa-police-walking-release.log`. The native integration test still separates/remounts after 434 ticks. These gates include compatible in-flight simulation/demo changes in the workspace; those changes are kept outside this checkpoint's commit. Next: original combat animation event timing and remaining cop/race integration.

Build fix (October 11, 2026): `src/game_main.cpp` called `Race::set_traffic` but the `TrafficSystem*` setter was missing from `src/race.hpp` (only the `traffic_` member and its use in `race.cpp` remained). Restored the one-line setter. Verified: full `build/rr-game` links, `race`/`traffic`/`crash`/`police`/`actor_traffic` ctests pass, `--ticks 120 --screenshot` headless race renders (Napa GET READY frame), `--menu-frames 120 --screenshot` renders the menu disclaimer. Game left running via nohup (`./build/rr-game --assets /Users/archit/Dev/rr/cdrom/ROADRASH`).

UI cleanup (October 11, 2026): removed the bottom keyboard-instructions overlay from the race HUD in `src/game_main.cpp` (`draw_hud`). The two `SDL_RenderDebugText` lines at the bottom of the view ("Up/Down throttle/brake, Left/Right steer, Z punch, X kick, C backhand" and "P/Esc/Back/Start pauses. Enter/Start/tap resumes. Q quits") are no longer drawn.

### Session 24: Napa playtest review and replanning (October 11, 2026)

- The owner played the Napa race and reported:
  - no traffic collision;
  - the right dashboard meter glitching;
  - bike graphical glitches;
  - an apparent split between the "Napa demo" and `rr-native`.
- Root causes:
  - `Race` has no rider–car contact pass.
  - The tachometer reads bike `+0x2dc`, which only `bike.cpp` initialises; the original `0x004349c0` updates it, but the port keeps that logic privately in `sound.cpp`. The distance readout reads an unmaintained `+0x44` (shown as a constant `25.5` in screenshots 1,500 ticks apart).
  - Native rider/bike/shadow nodes have not been compared with original display lists. Autopilot screenshots show offset or doubled bike parts and dithered blocks.
  - `./run-macos.sh` launches the research viewer (`rr-native`), not `rr-game`, and `build-release/Road Rash Native.app` was built October 10, before the Napa scene and actor work.
- Review of newly landed work: plan 10's replay currently re-anchors the RNG every tick and replays the attract-demo AI branch for the player. This localises divergences but is not acceptance; plan 10 Task 5 now requires a single anchor and one human-control scenario.
- Added [plans/11a-napa-playtest-fixes.md](plans/11a-napa-playtest-fixes.md). Updated the [plans/00-INDEX.md](plans/00-INDEX.md) status table, order and findings, plan 10 Task 5's acceptance rule and plan 11's status. Added `analysis/playtest/` to `.gitignore`. No source code was changed in this session. A parallel session has uncommitted edits in `src/game_main.cpp` and `src/race.cpp`; they were left untouched.

### Session 25: Napa playtest fixes — launch paths and tachometer (October 11, 2026)

- Reviewed [plans/11a-napa-playtest-fixes.md](plans/11a-napa-playtest-fixes.md) and began executing it.
- **Task 4 (one game, every launch path)** done: `run-macos.sh` now builds and
  launches `rr-game` (with `--assets` when the disc is mounted, else the
  first-run import); new `run-viewer.sh` launches the research viewer, which is
  renamed `rr-native` → `rr-viewer` in CMake, the viewer usage string,
  `tools/ci.sh`, README/RUN_NATIVE and plans 00/01/04/08/15. `tools/build_macos_app.sh`
  stamps `RRBuildCommit` (from `git rev-parse --short HEAD`) into the bundle
  plist and re-signs; `tools/verify_macos_app.py` fails a bundle whose stamp
  differs from HEAD, and `tools/ci.sh` fails a stale `build-release` bundle
  (or rebuilds it under `RR_STANDALONE_APP=1`). Rebuilt the standalone bundle
  and confirmed the verifier passes; the stale October 10 bundle fails as
  expected.
- **Task 2 (tachometer)** rpm part done: `update_engine_rpm(Bike&)` is a
  literal port of `0x004349c0` (gear-ratio/speed → `+0x2dc`, `±0x2000` slew,
  `+0x1000` airborne capped at `0xffff`, `−0x1000` after a crash, `+0x2e8`
  speed cache, `+0x2e0 = 0x9ffe`). `SoundSystem::update` now takes `Race&`,
  calls it for the player and the two nearest opponents, and reads `+0x2dc`
  for the engine pitch, deleting the private `rpm_`/`rpm_speed_`. `bike_test`
  and `hud_test` cover the port and the needle-vs-`+0x2dc` relationship; the
  native CTest suite is 62/62.
- The distance readout is traced to the global `DAT_004b8a34`
  (target-opponent distance, `0x00438550`, in feet) but not yet ported (needs
  the `0x00407410` cumulative-distance walk and oracle calibration). Details in
  REVERSE_ENGINEERING.md and VERIFICATION.md.
- Tasks 1 (rider–car collision) and 3 (bike/shadow nodes) remain: both need
  new original captures/fixtures.

### Session 26: replan for Napa completion, all courses and settings screens (October 11, 2026)

- The owner asked for plans to: finish Napa, then port all remaining courses; complete AI, physics, collisions, animation and graphics; and add game-style settings screens (resolution, sound, etc.).
- Reviewed the code and plans against that request. Findings:
  - Napa's remaining work was spread over plans 10, 11 and 11a with no single view per system. Several gaps had no owner: the knock-off decision `0x004052c0`, rider–cop contact, roadside-object collision, the weapon lifecycle, off-road drag and reverse riding, per-tick animation verification, the original needle pen and opponent list, and a native determinism/soak gate.
  - Plan 12 (all courses) was too thin: it missed per-level race start/finish (the course-length table `0x00468f58` differs by level, but the port runs the whole route), the `make_race_setup` extraction that plan 10 Task 4 specified but never landed (`src/race_setup.*` does not exist), and per-course trackers.
  - Settings: `Settings` already stores the original registry values and the Graphics Options popups work, but the sound mix, controls and jukebox handlers are stubs (`frontend.cpp`, `return 0`), the race ignores `HiRes`/`Inset`/`HiResBikers`, the window is fixed (960×760, letterbox, vsync on), and the pause screen is SDL debug text.
- Added [plans/16-gameplay-systems.md](plans/16-gameplay-systems.md) (completion matrix, Napa critical path, 7 new tasks), [plans/17-all-courses-port.md](plans/17-all-courses-port.md) (supersedes plan 12; all maps playable early via `make_race_setup`, then CITY → HIWAY → nitro → MEDLY → CANYN parity) and [plans/18-settings-and-options.md](plans/18-settings-and-options.md) (original options screens plus native Display/Sound/Pause screens built from original menu geometry, fonts and colours; fullscreen, scaling, filtering, vsync; low/inset race views).
- Updated [plans/00-INDEX.md](plans/00-INDEX.md) (status table, order, definition of done), and added pointers in plans 11, 12 (superseded), 13 (Task 3 moved to plan 18) and 14 (Task 2 Step 3 moved to plan 18).
- No source code changed. Uncommitted work from a parallel session (`src/race.*`, `src/game_main.cpp`, `oracle/aweman_proxy.c`) was left untouched.
- Follow-up in the same session: the owner clarified that "graphic settings" means the game itself must render at higher resolutions, and asked for plans for the recommended upscaling approach. Finding: the scene pipeline emits resolution-independent Q16 view coordinates (`DrawNode`), and only `ViewMode::map` fixes the 640×480 surface (shift 15, bit 0 cleared), so higher render scales need a scaled final mapping plus surface-size fixes (`TextureEdge` rows fixed at 480, `int16_t` vertices, horizon/HUD/menu images) rather than a new renderer. Added [plans/19-high-resolution-rendering.md](plans/19-high-resolution-rendering.md) (render scale 1–4× and match-display, 640×480 kept bit-identical, band-threaded raster, widescreen decision-gated) and [plans/20-asset-upscaling.md](plans/20-asset-upscaling.md) (local upscale cache, MMPX for indexed textures and sprites, Real-ESRGAN for menu artwork via a script, GPU screen filters, AI for in-race art experimental). Plan 18's "non-goal" section now points to them; the index lists both.

### Session 27: enhanced visuals and toolchain plans (October 12, 2026)

- The owner asked whether C++ was the right language (answer: yes; keep C++, harden the toolchain instead of a Rust rewrite) and for plans for the graphics enhancements and the hardening steps discussed.
- Code facts used: the race loop is fixed-step (`TickClock`, 60 ticks/s) and redraws from live `Race` state, so interpolation needs a captured per-tick view state; `DrawNode` carries no depth, so perspective texturing and fog add per-vertex depth from the slice projection; textures are affine (`TextureVertex` has no depth term); the draw distance is the constant `view_slices = 33`. Homebrew LLVM 22 is installed with libFuzzer and clang-tidy; Apple clang has neither. Threads today: the import worker, the SDL audio callback and SDL event watches.
- Added [plans/21-enhanced-visuals.md](plans/21-enhanced-visuals.md) and [plans/22-toolchain-hardening.md](plans/22-toolchain-hardening.md). Plan 19 Task 9 (widescreen) now hands implementation to plan 21 Task 8. The index lists both plans and their order.

## Makefile

`native-port/Makefile` wraps the README workflows: `make build|run|viewer|video|test|smoke|app|apk|apk-release|ci|clean`. `make help` lists targets; `COURSE=`, `BIKE=`, `ASSETS=`, `BUILD_TYPE=` override defaults. `run` omits `--course/--bike` unless set, since those flags force quick-race mode.
