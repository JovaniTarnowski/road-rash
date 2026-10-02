# Verification log (all sessions September 28, 2026)

## Session 1: macOS ARM64

- CMake configure and C++17 build succeeded on macOS 27.0 / Apple silicon.
- `file build/rr-native`: Mach-O 64-bit executable arm64.
- Dynamic dependencies: native SDL3, libc++, libSystem. No Windows runtime dependency.
- CTest resource-bounds test passed: valid synthetic resource, truncated inputs, invalid signature, overflowing/out-of-range offsets, sizes and counts.
- Original corpus: all 13 resource containers accepted, 429 directory entries validated against file bounds.
- Viewer smoke run decoded and rendered all nine configured screens without error.
- Original menu screenshot and Napa panorama screenshot visually inspected: `analysis/native-viewer.png`, `analysis/native-panorama.png`.

## Session 1: Android ARM64

- Gradle `assembleDebug` succeeded with NDK r28c, SDK 35, AGP 8.9.2 and SDL3 3.4.8.
- APK: `android/app/build/outputs/apk/debug/app-debug.apk` (approximately 4.1 MB).
- Installed successfully on connected MANGMI AIR X (`arm64-v8a`).
- Activity launch reported success. SDL runtime log confirms entry into `SDL_main` from `libmain.so` in the device's ARM64 library directory.
- **Visual rendering and touch navigation are not yet verified on Android.** The device was locked; screenshots showed the lock screen or sleeping display and SDL reported pause/stop lifecycle transitions. No device security settings were changed.
- Open `Road Rash Native Research` after unlocking to complete the device check. Tap to cycle the nine asset screens. Gameplay is not implemented.

## Session 1: analysis

- Ghidra 12.1.4 imported the original executable and saved its project.
- Export script produced 1,585 function pseudocode files with zero export failures.
- This is not a claim that all 1,585 functions have correct types or recovered semantics.
- Original asset inventory and executable hash recorded in `analysis/inventory.json`.
- Exact instruction disassembly saved in `analysis/original-disassembly.txt` for checking inferred pseudocode.

## Session 2: course geometry and road projection

- `rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE` on all five courses: every node reachable and counted; both branch routes built (CANYN 5,423/5,443, CITY 5,606/5,606, HIWAY 6,005/6,075, MEDLY 5,634/5,645, NAPA 6,224/6,319 samples).
- The same run projected 927,680 camera positions (every 1/16 sample, both branches, all courses) without errors. Projected road rows stayed in 15..762 of the 240-row view space, so no road lands above the top of the screen.
- Depth table read from the reference executable: 4,352 entries, positive and non-increasing; entry 256 = 32,768 (2^24/512).
- CTest `road-projection` passed: flat convergence, double-integrated curvature, single-integrated rise, fractional-camera rounding, route-end truncation, rejection of non-PE input. Synthetic depth table; no original data in tests.
- macOS viewer smoke run decoded all nine images and all five courses. Screenshot `analysis/native-road-napa.png` (NAPA sample 300) visually inspected: converging lanes, a crest, and the road reappearing on a further rise. HIWAY 2500 and CANYN 800 were also checked.
- **Not compared against the original game's output.** Placement matches the recovered arithmetic, but no Wine capture at a matching position has been diffed. Shading, scenery and split roads are placeholders or absent.
- Android: `assembleDebug` succeeded; APK (4.9 MB) contains `libmain.so` (ELF aarch64) with the road renderer, the five `.CRS` files and `ROADRASH.EXE`. **No device was connected in this session**, so the Android road view is built but not run. Touch taps cycle screens and the road auto-drives. Speed and branch controls are keyboard-only.

## Session 3: split and merge roadways

- `rr-course` on all five courses, both routes: every split opens an 82-sample dual stretch and every merge restores an 81-sample window. Lateral separation is exactly 0 one sample before every merge; the tool aborts otherwise. Height mismatches between paired branches mirror exactly between routes A and B and close to within ±5 units one sample before each merge.
- Projection sweep (every 1/16 sample, both routes) projected about 1.9 million second-road slices without errors. Rows stayed within 37..722; centre columns reach ±174,000 px mid-branch, consistent with separations of up to about 32,000 world units.
- CTest `road-projection` extended: split ahead (both halves start together, own offsets, own bend and rise), camera inside a dual stretch (stored separation), then own-bend integration. Passed.
- Screenshots inspected: NAPA 740 (just after the split) and 960 (second road converging onto the merge point; saved as `analysis/native-road-merge.png`).
  - A first renderer draft let the second road's verge bands cut through the rider's road. The bands are a placeholder, not taken from the original; only the rider's own road now lays them.
- Android APK rebuilt (5.3 MB). **Still no device connected; not run on hardware.**
- Still not compared against Wine captures of the original.

## Session 4: section chunk survey

- Every section row on all five courses carries `HTPR`, `MSRR`, `DLSR`, `NALR` and `DZHR` chunks (109 sections); `BORR` is in 108, `CESR` in 97, and a terrain chunk in all 109 (`LIHR` 64, `DLBR` 34, `NTMR` 11).
- `DLSR`: 389 records. The extent is 16 + 36·n in all 109 chunks. All 261 records with end > start have Q16 slopes equal to `(end byte − start byte)·65536/(end − start)` within ±1, with zero mismatches. 274 of 280 consecutive record pairs start where the previous one ended.
- Survey scripts were one-off Python checks; not yet part of `tools/` or CTest. No code change to the native reader or renderer in this session, so build and test status is unchanged from session 3.
- Shoulder rendering (same session): `walk_intervals` accepts every `NALR` and `DLSR` table on all five courses (the corpus sweep and smoke run are unchanged and pass).
  - Lane widths compared before and after the walker: 34,182 samples, 6 changed (HIWAY section 6, samples 1–6, one iterator step earlier), matching `0x00452920`.
  - CTest `road-projection` gained a shoulder-edge check; both tests pass.
  - Screenshots inspected: NAPA 300 and CITY 1500 (asymmetric shoulders; saved as `analysis/native-road-shoulders.png`).
  - APK rebuilt; no device connected.

## Session 5: playable Napa

- CTest: 7/7 pass (`resource-bounds`, `road-projection`, `framebuffer`, `assets`, `bike`, `race`, `input`).
  - `bike`/`race` expectations are hand-computed from the ported routines.
  - Red steps were observed as missing headers or symbols (for `bike`, after the fact, by removing the implementation).
- Headless ride on the real Napa data (real physics tables and `SPEC`s) with a keep-to-centre bot finishes the full route:
  - bike 1: 9.7 min, 0 crashes
  - bike 8: 7.2 min, 1 crash
  - bike 15: 10.0 min, 3 crashes
  - top speeds 11,776 / 16,640 / 12,288
- `rr-game --autopilot` screenshots at ticks 30, 1,800 and 9,000 were inspected: countdown, fifth gear at speed ~111 on a curve, a crest at 24%. Saved as `analysis/native-game-napa.png`.
- A real-time 8-second run completed without errors.
- A refactor check (road drawing moved out of the viewer): viewer screenshot byte-identical before and after.
- Android: APK built (5.3 MB) with the game as `libmain.so`. **Not run: no device connected.**
- **Not verified against the original game:** no oracle; handling, speeds and crash behaviour may differ from the 1996 game.

## Session 6: opponents and combat

- CTest: 8/8 pass, also under UBSan/ASan. New checks:
  - bob decoding (synthetic, plus the corpus: `HIBOB` 326 frames with frame 3 at 56×128, `LOCOP` 45, remap row 0 entry 2 = 80)
  - the opponent field from the executable
  - AI holds its target speed and stays on the road
  - riders never stay inside each other
  - punches drain stamina and knock a rider off; out-of-reach punches do nothing
- All five sprite files decode with every byte consumed. Frames were rendered through all 9 remaps and inspected.
- Headless Napa, level 1, bot player plus 7 AI:
  - without contact: AI finish times 599–703 s, no crashes
  - with contact: 641–703 s, no crashes
- Screenshots inspected: the start grid, AI racing ahead, side-by-side riders, and a punch landing (opponent stamina bar drops; saved as `analysis/native-game-combat.png`).
- The input-mapping additions for attack bits were written together with their tests (red not observed separately).
- **Not verified:** nothing is compared against the original game. Android was rebuilt but has not run on hardware.

## Session 7: lifecycle, multi-touch and frame timing (October 1, 2026)

- Debug configure/build succeeded; CTest **9/9 passed**. Existing resource/course/physics/race/sprite checks remain passing.
- Sanitizer build (`-fsanitize=undefined,address -fno-sanitize-recover=undefined`): **9/9 passed**.
- New `session` checks use synthetic SDL events: background pauses immediately; background/unfocused resume is refused; foreground/focus gain requires explicit resume; clock reset is consumed once. Rational timing produces exactly 600 steps over 10 seconds at each of 30, 60, 120 and 144 Hz. Paused time and partial ticks are discarded; `UINT64_MAX` elapsed time is capped at 20 steps without multiplication overflow or remaining catch-up debt.
- Expanded `input` checks: two-finger steering/punching, independent release, touch-device/finger ID pairs, brake priority over auto-throttle, motion between zones, cancellation, and clearing all fingers. These test intent aggregation; they do not establish Android's physical event delivery.
- Native game smoke: `build/rr-game --assets ../cdrom/ROADRASH --autopilot --ticks 1800 --paused --screenshot analysis/native-game-paused.bmp` exited successfully. Converted PNG visually inspected: dimmed race, centred pause title and readable resume/quit instructions (`analysis/native-game-paused.png`). `--ticks` pre-runs the simulation before `--paused` takes effect.
- `JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home android/gradlew -p android assembleDebug`: **BUILD SUCCESSFUL**. `aapt dump badging` confirms `arm64-v8a`, min SDK 23/target SDK 35 and optional gamepad/touchscreen features. `apksigner verify --verbose` passes with the debug signature (v1/v2; v3/v4 not enabled). It reports the standard unsigned Gradle metadata entry warning.
- `git diff --check` passes. `adb devices` lists no devices: **no Android hardware validation performed**.
- Remaining platform checks: physical multi-touch/gamepad input, background/foreground and controller disconnect in a live Android race, actual render-device reset, ultrawide/portrait resizing and device frame-time measurements. Mixer/career pause/save and final import/release packaging remain unimplemented. Rendering and simulation still have the previously documented original-game deviations.
