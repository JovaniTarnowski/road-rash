# Platform, Input and Packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Tasks 1–2 can start after plan 01. Tasks 3–8 need the game executable `rr-game` from plans 04–07.

**Goal:** Turn `rr-game` into shippable products:
- a standalone macOS ARM64 `.app`
- an installable Android ARM64 APK that imports the user's own disc at first launch instead of bundling any game data

Both get keyboard, gamepad (including the MANGMI AIR X built-in controls) and touch input, correct lifecycle behaviour, stable frame pacing, and a CI script.

**Architecture:**
- **Game data:** read through `rr::Assets` (plan 01) from an imported copy under the app's pref path. The importer reads either an extracted `ROADRASH` folder or the ISO directly (a native ISO 9660 reader) and verifies SHA-256 hashes before use.
- **Input:** from all devices, mapped onto the original's action codes (`KeyBindings` → `ControlMask`, plan 04), so the simulation never knows the device.
- **SDL:** linked statically on macOS from a pinned, hash-verified source release; Android keeps the verified SDL3 AAR.

**Tech Stack:** C++17, SDL3 3.4.8, CMake, Gradle/AGP 8.9.2, NDK r28c.

**Spec:** [00-INDEX.md](00-INDEX.md) (definition of done items 1 and 5).

## Global Constraints

- **No game data in any build artifact.** The APK must not contain `assets/game/`. `tools/prepare_android_assets.py` becomes a developer-only option (`-PdevAssets=true`) that is off by default. A release check fails the build if any `assets/game` entry is present.
- Imported data must match the reference hashes (`analysis/inventory.json` lists 341 files with SHA-256). Hashes of the files may be committed; the files may not.
- Everything added to `third_party/` has its licence file and a line in `third_party/README.md`.
- Signing keys and keystore passwords are never committed; they are read from `~/.gradle/gradle.properties` or environment variables.

## Review Focus

1. **The user points the importer at the wrong disc or a damaged ISO:** the import must fail with a clear message listing the mismatching files and leave no half-imported state. Task 1 tests a corrupted-byte ISO and a foreign ISO.
2. **The game is launched with no data imported** (fresh install, or data deleted from settings): it must show the import screen, not crash. Task 2 tests an empty pref dir.
3. **A controller connected or disconnected mid-race:** the race pauses on disconnect of the active device, and newly connected controllers work immediately. Task 3 tests both via synthetic SDL events.
4. **Android backgrounding mid-race** (home button, incoming call): the simulation stops, audio pauses, and the state is intact on return. `onDestroy` after backgrounding must not lose career progress. Task 4.
5. **Displays with unusual aspect ratios or refresh rates** (21:9 phone, 120 Hz iPad-style Android panels, 144 Hz Mac displays): letterboxed 4:3 presentation, and simulation speed unchanged. Tasks 4 and 6.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/iso9660.hpp/.cpp` | Read files from an ISO 9660 image (read-only, Level 1 plus the rerelease's layout) |
| `src/importer.hpp/.cpp` | Import from ISO or folder into pref path, with hash verification |
| `src/reference_hashes.inc` | Generated from `analysis/inventory.json`: path → SHA-256 (hashes only) |
| `tools/gen_reference_hashes.py` | Generates the above |
| `src/input.hpp/.cpp` | Keyboard, gamepad and touch to action codes; touch overlay |
| `src/app.hpp/.cpp` | Lifecycle, pause, window/presentation, frame pacing (extracted from `game_main.cpp`) |
| `macos/Info.plist.in`, `macos/AppIcon.icns` | Bundle metadata and an original (non-game) icon |
| `cmake/sdl3_static.cmake` | Pinned SDL3 source build for macOS |
| `android/app/src/main/java/.../MainActivity.java` | Unchanged or minimal; SDL provides the dialogs |
| `tools/ci.sh` | Build, test, smoke, Android assemble, artifact checks |
| `docs/USER_GUIDE.md` | Import, controls, troubleshooting |

---

### Task 1: ISO 9660 reader and importer

Implemented October 2: native reader/hash/import modules, generated 341-file reference metadata, transactional folder/ISO import, nonseekable-stream cache and cancellation cleanup. Real ISO import and viewer smoke passed; synthetic parser/import tests and sanitizers pass. See VERIFICATION.md.

**Files:**
- Create: `src/iso9660.hpp`, `src/iso9660.cpp`, `src/importer.hpp`, `src/importer.cpp`, `tools/gen_reference_hashes.py`, `src/reference_hashes.inc`
- Create: `tests/iso9660_test.cpp`, `tests/importer_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  class Iso9660 {
  public:
      explicit Iso9660(std::function<size_t(uint64_t offset, uint8_t* out, size_t n)> read_at);  // random access to the image
      std::vector<std::string> list(const std::string& dir) const;      // upper-case names without ';1'
      std::vector<uint8_t> read(const std::string& path) const;         // e.g. "ROADRASH/DATA/GLOBAL.RSC"
  };
  struct ImportProgress { size_t done, total; std::string current; };
  struct ImportResult { bool ok; std::vector<std::string> mismatched, missing; };
  // source: path to an .iso file or to a directory containing ROADRASH/. dest: directory that becomes the Assets root.
  ImportResult import_game(const std::string& source, const std::string& dest, const std::function<void(const ImportProgress&)>& progress);
  }
  ```

- [ ] **Step 1: Failing tests**

`iso9660_test` builds a tiny ISO image in memory:
- 2048-byte sectors
- Primary Volume Descriptor at sector 16: type 1, `CD001`, root directory record at byte 156 of the PVD
- Terminator at sector 17
- Root directory at sector 18 containing `ROADRASH` (a directory at sector 19)
- `ROADRASH` contains `A.BIN;1` (5 bytes at sector 20)

Directory record layout: length (1 byte), extent LBA (both-endian 4+4 bytes at offset 2), data length (both-endian at offset 10), flags (offset 25, bit 1 = directory), name length (offset 32), name (offset 33).

Assert:
- `list("ROADRASH") == {"A.BIN"}` and `read("ROADRASH/A.BIN")` returns the 5 bytes
- missing paths throw
- a directory record whose extent points past the image end throws
- a directory loop (a child pointing at its parent's extent) is rejected, not looped

`importer_test`:
- With a temp source dir containing two files and an injected hash table (`import_game` overload taking the table, for tests), a matching import produces `ok` and copies both.
- One corrupted byte gives `ok == false`, lists that file, and leaves the destination empty: import goes to `dest + ".partial"`, then is renamed.

- [ ] **Step 2:** `tools/gen_reference_hashes.py` reads `analysis/inventory.json` and writes `src/reference_hashes.inc` as `{"ROADRASH/DATA/GLOBAL.RSC", "…sha256…"},` lines. Use an SHA-256 implementation: add `third_party/sha256.h` (public domain / MIT single header) with its licence.
- [ ] **Step 3:** Implement and pass. Then import from the real ISO (`/Users/archit/Dev/rr/Road Rash (USA) (En,Fr,De,Es,It) (Rerelease) (19970930).iso`) into a temp dir. All 341 files match and `rr-native --assets <tmp>/ROADRASH --smoke` works.
- [ ] **Step 4:** Commit: `platform: ISO 9660 reader and verified game data import`.

---

### Task 2: First-run import flow on both platforms

Implemented October 2: shared first-run UI, async SDL file/folder selection, worker progress/errors, default pref-path data root, empty-pref CTest smoke and asset-free Android assembly with APK verification. Desktop screenshot inspected. Physical Android SAF remains open.

**Files:**
- Modify: `src/game_main.cpp`, `src/frontend.cpp` (an import screen before the splash when no data is present)
- Modify: `android/app/build.gradle`, `tools/prepare_android_assets.py`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `import_game`, `SDL_ShowOpenFileDialog`/`SDL_ShowOpenFolderDialog` (SDL3 dialog API; on Android it uses the Storage Access Framework and returns `content://` URIs, which `SDL_IOFromFile` can open)
- Produces: `std::string rr::data_root();` returns `SDL_GetPrefPath(...) + "game/ROADRASH"`.

- [ ] **Step 1: Test**

With `SDL_GetPrefPath` redirected to an empty temp dir (via `RR_PREF_OVERRIDE`), the game starts into the import screen and exits cleanly on quit.

- [ ] **Step 2: Implement the import screen**

- The button opens the file dialog, filtered to `*.iso`, or the folder dialog.
- The screen shows progress and a hash-mismatch list.
- On Android, an ISO chosen via SAF is read through `SDL_IOFromFile(uri)` with random-access seeks. If SDL's Android content IO is not seekable, copy the ISO into the cache directory first, then import.

- [ ] **Step 3: Strip game data from the APK**

In `android/app/build.gradle`, add a `devAssets` project property. Without it, exclude `assets/game/**` via `androidResources { ignoreAssetsPattern "!game" }` or a source-set exclusion.

Add a `verifyNoGameData` task that unzips the built APK listing and fails if any entry starts with `assets/game/`, and make `assembleRelease` and `assembleDebug` depend on it.

- [ ] **Step 4:** Build the debug APK. `unzip -l app-debug.apk | grep -c assets/game` prints 0.
- [ ] **Step 5:** Commit: `platform: first-run data import; no game data in APK`.

---

### Task 3: Input devices

**Files:**
- Create: `src/input.hpp`, `src/input.cpp`, `tests/input_test.cpp`

**Interfaces:**
- Consumes: `rr::KeyBindings`, `rr::ControlMask` (plan 04), `rr::MenuInput` (plan 06)
- Produces:
  ```cpp
  namespace rr {
  class InputRouter {
  public:
      explicit InputRouter(const Settings&);
      void handle(const SDL_Event&);                  // keyboard, gamepad add/remove/buttons/axes, touch
      ControlMask race_controls() const;             // union of all devices, original action semantics
      MenuInput menu_input();                        // edge-triggered for menus
      bool active_device_lost();                     // true once after the device in use disconnects
      void draw_touch_overlay(Framebuffer&) const;   // only when touch was the last used device
  };
  }
  ```
  Gamepad defaults:
  - left stick / D-pad: steer and menu navigation
  - right trigger: accelerate; left trigger: brake
  - X/Y: left/right punch; shoulders: kicks
  - Start: pause; B: back

  These are adjusted to the original action list from plan 04.

- [ ] **Step 1: Tests with synthetic events**

- An `SDL_EVENT_GAMEPAD_BUTTON_DOWN` for the accelerate binding sets the same `ControlMask` bit as the keyboard accelerate key.
- A stick past 50% sets steer, and below the deadzone clears it.
- A touch-down inside the virtual accelerate button sets accelerate, and touch-up clears it.
- `SDL_EVENT_GAMEPAD_REMOVED` for the active device makes `active_device_lost()` true exactly once.
- A newly added gamepad works without restart.

- [ ] **Step 2:** Implement. Rebinding reuses the front-end options (plan 06 `Settings.keys`); add gamepad bindings to `Settings`.
- [ ] **Step 3:** On the MANGMI AIR X (when connected), run `adb logcat -s SDL` while pressing every control and confirm it appears as an SDL gamepad. If it presents as a keyboard instead, add its key codes to the default bindings.
- [ ] **Step 4:** Commit: `input: gamepad, touch and rebinding`.

---

### Task 4: Lifecycle, presentation and frame pacing

Implementation note (October 1, 2026): `src/session.hpp/.cpp` now provides atomic lifecycle state and a rational fixed-step clock, integrated directly into `game_main.cpp`. Background/focus loss/controller removal pauses; foreground requires explicit resume. Back opens pause, touch state clears, and render resets invalidate the sprite cache. Synthetic lifecycle/timing tests pass at 30/60/120/144 Hz; the pause screen is visually checked. This task remains partial: no mixer/career exists to pause/save, there is no `App` abstraction, presentation remains the current 640×510 game/HUD letterbox, and Android hardware/resize checks are pending.

**Files:**
- Create: `src/app.hpp`, `src/app.cpp`, `tests/app_test.cpp`
- Modify: `src/game_main.cpp` (use `App`)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  class App {
  public:
      App(int argc, char** argv);
      int run();                                  // the whole program
      void on_event(const SDL_Event&);            // public for tests
      bool simulation_paused() const;
      uint64_t frames_presented() const;
  };
  }
  ```

- [ ] **Step 1: Tests with synthetic events and a virtual clock**

- `SDL_EVENT_WILL_ENTER_BACKGROUND` pauses the simulation and mixer, and saves the career.
- `SDL_EVENT_DID_ENTER_FOREGROUND` shows the pause menu (it does not auto-resume).
- `SDL_EVENT_RENDER_DEVICE_RESET` recreates textures.
- The Android back key opens the pause menu in a race and goes back in menus.
- With a virtual clock at 30/60/120/144 Hz, the number of simulation steps after 10 s equals `10 * ticks_per_second` (±1).

- [ ] **Step 2: Implement presentation**

Integer-scaled 4:3 letterbox where possible, nearest filtering, black bars, and `SDL_SetRenderVSync(renderer, 1)`. The fixed-step accumulator is capped at `max_steps_per_frame`.

- [ ] **Step 3:** On macOS, resize the window to 21:9 and to portrait. The image stays 4:3 and centred.
- [ ] **Step 4:** Commit: `app: lifecycle, letterboxing and frame pacing`.

---

### Task 5: macOS standalone app bundle

**Files:**
- Create: `cmake/sdl3_static.cmake`, `macos/Info.plist.in`, `macos/AppIcon.icns` (new artwork, not from the game)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `build-release/Road Rash Native.app`. `otool -L` on its binary lists only `/usr/lib` and `/System` libraries.

- [ ] **Step 1: Pin the SDL3 source**

Download the SDL 3.4.8 source release tarball from the official GitHub release, record its SHA-256 in `cmake/sdl3_static.cmake`, and build it with `ExternalProject_Add` or `FetchContent` (`URL`, `URL_HASH SHA256=...`, `-DSDL_STATIC=ON -DSDL_SHARED=OFF -DCMAKE_OSX_ARCHITECTURES=arm64`). Enable it with `-DRR_BUNDLE_SDL=ON`.

- [ ] **Step 2: Bundle**

`add_executable(rr-game MACOSX_BUNDLE ...)` with the `MACOSX_BUNDLE_*` properties and `Info.plist.in`:
- bundle id `org.roadrash.nativeport`
- `LSMinimumSystemVersion` 13.0
- `NSHighResolutionCapable` true
- game-controller usage enabled
- no file-type claims

Post-build: `codesign --force --deep --sign - "Road Rash Native.app"`.

- [ ] **Step 3: Verify**

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DRR_BUNDLE_SDL=ON
cmake --build build-release --parallel
otool -L "build-release/Road Rash Native.app/Contents/MacOS/Road Rash Native" | grep -c homebrew   # expect 0
codesign --verify --deep --strict "build-release/Road Rash Native.app" && echo SIGNED
open "build-release/Road Rash Native.app"
```
Expected: `0`, `SIGNED`, and the app launches into the import screen, or the splash if data is imported.

- [ ] **Step 4:** Commit: `macos: standalone signed app bundle with static SDL3`.

---

### Task 6: Android release build and device validation

**Files:**
- Modify: `android/app/build.gradle`, `android/app/src/main/AndroidManifest.xml`

- [ ] **Step 1: Release configuration**

- `versionName` from the git tag (fallback `0.x`)
- `applicationId` unchanged
- label "Road Rash Native"
- `signingConfigs.release` from properties `RR_KEYSTORE`, `RR_KEYSTORE_PASSWORD`, `RR_KEY_ALIAS`, `RR_KEY_PASSWORD`
- `minifyEnabled false`, because SDL uses JNI reflection. If minification is enabled later, add SDL's keep rules.
- In the manifest:
  - add `android:resizeableActivity="true"`
  - keep landscape
  - declare `<uses-feature android:name="android.hardware.gamepad" android:required="false"/>` and `android.hardware.touchscreen` as not required

- [ ] **Step 2:** `android/gradlew -p android assembleRelease` (with local signing properties) produces a signed APK. `verifyNoGameData` passes.

- [ ] **Step 3: Device checklist** (MANGMI AIR X, when connected: `adb devices`)

1. Install with `adb install -r`.
2. Import the disc from device storage (copy the ISO with `adb push` to `/sdcard/Download/`).
3. Play a race with the built-in controls.
4. Background and return.
5. Kill the process and relaunch; the career persists.
6. Check `adb shell dumpsys gfxinfo org.roadrash.nativeport` for frame-time percentiles: p95 at most 16.7 ms.

Record the results in `docs/VERIFICATION.md`. If no device is available, state that explicitly and leave this item open in `PROGRESS.md`.

- [ ] **Step 4:** Commit: `android: signed release build and device validation`.

---

### Task 7: CI script

**Files:**
- Create: `tools/ci.sh`

- [ ] **Step 1: Write the script**

```sh
#!/bin/sh
# Full local verification. Game-data tests skip automatically when the disc is absent.
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
if [ -d ../cdrom/ROADRASH ]; then
  build/rr-native --assets ../cdrom/ROADRASH --smoke
  build/rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE ../cdrom/ROADRASH/DATA/COURSES/*.CRS
fi
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}"
android/gradlew -p android assembleDebug -q
unzip -l android/app/build/outputs/apk/debug/app-debug.apk | grep -q 'assets/game/' && { echo "game data in APK"; exit 1; }
echo CI OK
```

- [ ] **Step 2:** Run `tools/ci.sh`; expect `CI OK`.
- [ ] **Step 3:** Commit: `tools: local CI script`.

---

### Task 8: User guide and final acceptance

**Files:**
- Create: `docs/USER_GUIDE.md`
- Modify: `README.md`, `docs/PROGRESS.md`, `docs/VERIFICATION.md`

- [ ] **Step 1: Write the user guide**

`USER_GUIDE.md` covers:
- requirements (your own Road Rash 1997 rerelease disc or ISO)
- installing on macOS and Android
- importing
- controls for keyboard, gamepad and touch
- settings, save file location, known deviations (fonts, MIDI), troubleshooting

- [ ] **Step 2: Acceptance walkthrough**

Go through each item of the definition of done in `00-INDEX.md`, recording evidence (command output, device notes, golden test names) in `VERIFICATION.md`. Any item not met stays open in `PROGRESS.md` with its reason.

- [ ] **Step 3:** Commit: `docs: user guide and final acceptance record`.

October 2 packaging status: Task 5 software checks pass (static pinned SDL, signed ARM64 app/ZIP, system-only dependencies, first-run CLI smoke and 23 release tests). The internal executable is named `Road Rash Native`. Signing is ad-hoc, not Developer ID/notarized. Task 6 release configuration and local development-key APK/signature checks pass; owner-key signing and all hardware checklist items remain open. Task 7 local CI passes. Task 8 guide exists for the current incomplete development build; final acceptance is open.
