# Road Rash native port research

Current implementation and acceptance gaps: [docs/CURRENT_STATUS.md](docs/CURRENT_STATUS.md).
Agent setup, ownership, verification profiles and capture receipts:
[docs/AGENT_TOOLING.md](docs/AGENT_TOOLING.md). The dated sections below and in
the progress timeline describe historical checkpoints; use the current matrix
and gate receipts when assessing completion.

Progress summary, timeline and next steps: [docs/PROGRESS.md](docs/PROGRESS.md). Binary evidence: [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md). Test log: [docs/VERIFICATION.md](docs/VERIFICATION.md). Plans to completion: [docs/plans/00-INDEX.md](docs/plans/00-INDEX.md).

Native C++17 / SDL3 work targeting macOS ARM64 and Android ARM64. `rr-game` is a **playable race against seven AI opponents** with the original bike physics and combat; `rr-viewer` is the asset research viewer. It does not execute the Windows binary, use Wine, or emulate x86.

C++ formatting and static analysis: `make format FILES='src/foo.cpp src/foo.hpp'`,
`make format-check FILES='src/foo.cpp src/foo.hpp'`, and
`make lint BUILD_DIR=build FILES='src/foo.cpp'` use LLVM 22.
See [docs/CPP_QUALITY.md](docs/CPP_QUALITY.md) for setup, checks and rollout.

## Current implementation

- Original executable imported and analyzed in Ghidra: 1,585 functions exported as address-keyed pseudocode.
- Original disc inventory: 341 files with SHA-256 hashes and executable import table.
- Native, bounds-checked resource directory reader, validated against all 13 `CRSR` containers on this disc.
- Native JPEG decoding for six original menu screens.
- Native palette decoding for the original 190×64 dashboard and 720×320 Napa and city horizons. Dimensions traced to original executable routines.
- Native decoding of all five courses' sections, lane widths and branch graph.
- Integer port of the original road projection: 33 perspective slices, curvature/height integration and the original depth table read from `ROADRASH.EXE`, including the second roadway at splits and merges. The viewer drives through each course with flat placeholder shading.
- Shared desktop/Android viewer source with keyboard, mouse and touch navigation.

Racing, bike physics, AI riders, sprites and combat are implemented with the approximations recorded in `docs/PROGRESS.md`. Scenery, original road textures, game sound triggers/music selection, save games and original menu interactions remain open. Native audio/video codecs are implemented and video playback is available via `--video`. Decompiler output is research input: inferred signatures, types and library labels still need correction.

## Play (single course)

```sh
cd /Users/archit/Dev/rr/native-port
./run-macos.sh                                      # build and launch the current game
```

Or build and run directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel
build/rr-game --assets ../cdrom/ROADRASH            # Napa; --course 0-4, --bike 0-14
```

Arrows or WASD: Up accelerates, Down brakes, Left/Right steer; Z punches, X kicks, C backhands. Gamepad: A or right trigger accelerates, B or left trigger brakes, D-pad or left stick steers; X punches, Y kicks, RB backhands. Touch: top third punches, left/right thirds below it steer, bottom middle brakes; other zones accelerate. Multiple fingers can steer and punch together; a braking finger overrides touch auto-throttle. Enter, A or a tap restarts after the finish.

P/Esc or gamepad Start toggles pause. Android Back opens pause. Enter, Start or a tap resumes. Q or closing the desktop window quits. Backgrounding, losing window focus or disconnecting the active gamepad pauses the race and clears touch input. Returning to the app requires an explicit resume. `--paused` starts the presentation paused (after any `--ticks` verification run).

The race uses the original 60 Hz bike physics ported from the executable (throttle, brakes, 7-gear shifting, drag, traction, steering, curve drift, crests and jumps) on the original course geometry. You race seven original AI opponents with punches, kicks and weapons, drawn with the original rider sprites. Road shading is a placeholder; traffic, police, race audio and gameplay menus are not done yet. See `docs/PROGRESS.md` for what is simplified.

## Import your disc

Launching `rr-game` without `--assets` uses its imported data directory and shows the first-run picker when absent. On desktop, choose either an ISO or the extracted folder. The importer leaves no half-imported destination on missing files, hash mismatch or cancellation. Existing nonempty destinations are preserved.

A command-line import is also available:

```sh
build/rr-import "/path/to/Road Rash.iso" "/path/to/imported/ROADRASH"
build/rr-game --assets "/path/to/imported/ROADRASH"
```

The default destination is `SDL_GetPrefPath("RoadRashNative", "RoadRash")/game/ROADRASH`. `RR_PREF_OVERRIDE` redirects the pref root for development tests. `--first-run-smoke` renders the missing-data screen once and exits; `--screenshot` can capture it. All imported commercial assets stay outside the source and APK.

## Run the research viewer (developer tool)

The asset research viewer is a separate tool, not the game. Use `./run-viewer.sh`:

```sh
cd /Users/archit/Dev/rr/native-port
./run-viewer.sh
```

The default assets directory is `../cdrom/ROADRASH`; an alternative extracted original directory can be passed as the first argument. Left/Right, Space, click, or tap cycle nine images and five course views. On course views, Up/Down change drive speed (negative reverses) and B switches split branch. Escape exits. `--screen 9 --position 1400 --screenshot out.bmp` captures one frame.

Dependencies: CMake, a C++17 compiler and SDL3 (`brew install cmake sdl3`). The local build links Homebrew SDL3; it is not yet a standalone distributable app bundle.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
build/rr-viewer --assets ../cdrom/ROADRASH --smoke
build/rr-inspect ../cdrom/ROADRASH/DATA/BIKESPEC.RSC
build/rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE ../cdrom/ROADRASH/DATA/COURSES/*.CRS
python3 tools/check_corpus.py ../cdrom/ROADRASH
```

## Android

The Gradle project is `android/`; it builds the game into `libmain.so` for `arm64-v8a`. Normal APKs contain no original game data. On first launch choose your own rerelease ISO; all 341 files are SHA-256 verified before the imported directory is committed. SDL supplies the Android content-URI picker and lifecycle/input bridge. Nonseekable document streams are temporarily cached during import.

```sh
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
android/gradlew -p android assembleDebug
```

SDK location is configured in the local, ignored `android/local.properties`. This workspace has its SDK in `.tools/android-sdk`. Required packages: `platforms;android-35`, `build-tools;35.0.0`, `ndk;28.2.13676358`, `cmake;3.22.1`, and `platform-tools`. Gradle 8.11.1 and Android Gradle Plugin 8.9.2 are pinned. The bundled SDL3 3.4.8 AAR comes from the official Android development archive, with its SHA-256 verified before extraction.

Output: `android/app/build/outputs/apk/debug/app-debug.apk`. Assembly verifies that the APK has no `assets/game/` entries. For local developer research only, stage with `tools/prepare_android_assets.py` and build with `-PdevAssets=true`; that exemption never applies to release APKs.

The APK was built and installed on the connected MANGMI AIR X. Its native library starts, but rendering and touch checks remain pending because the device was locked. See `docs/VERIFICATION.md` for exact test coverage.

## Reproduce analysis

```sh
python3 tools/inventory.py ../cdrom/ROADRASH --out analysis/inventory.json
tools/decompile.sh
```

The script defaults to Homebrew Ghidra 12.1.4 and JDK 21; `GHIDRA_HOME` and `JAVA_HOME` can override those locations. It overwrites the RoadRash analysis import, so save manual Ghidra changes separately before rerunning.

- `analysis/decompiled/*.c`: pseudocode, one file per virtual address.
- `analysis/decompiled/functions.tsv`: function index and export results.
- `analysis/decompiled/string-references.tsv`: strings and referencing functions.
- `analysis/ghidra-project/`: saved analysis database.
- `analysis/inventory.json`: original asset hashes and PE information.
- `analysis/resources.tsv`: resource directory dump of all original containers.
- `docs/REVERSE_ENGINEERING.md`: recovered structures, evidence and next implementation work.

Original commercial assets and reverse-engineered output remain local. No redistribution license for the game is implied. Third-party library license texts are in `third_party/` and inside the SDL AAR.

References: [SDL Android integration](https://wiki.libsdl.org/SDL3/README-android), [Ghidra headless documentation](https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/RuntimeScripts/support/analyzeHeadlessREADME.md), [Android Gradle Plugin 8.9 compatibility](https://developer.android.com/build/releases/agp-8-9-0-release-notes).

Native cutscene playback (the presentation and clip-selection frontend is still in progress):
```sh
build/rr-game --assets ../cdrom/ROADRASH --video VIDEO/INTRO.AVI
build/rr-game --assets ../cdrom/ROADRASH --video VIDEO/RAT/BANZAI.AVI
python3 tools/verify_video_audio.py ../cdrom/ROADRASH/VIDEO --video-decoder build/cinepak-test
```
P/Esc pauses, a key/click/tap skips, and input while paused resumes. Codec verification uses FFmpeg as a test oracle only; playback uses the native decoders and SDL3. For a deterministic silent screenshot use `--ticks 600 --screenshot analysis/native-video.bmp` (ten seconds).

Standalone macOS packaging: `tools/build_macos_app.sh` creates
`build-release/Road Rash Native.app` and `.zip`, with pinned static SDL,
ARM64/macOS 13 support, included licences and an ad-hoc signature. No Homebrew
runtime libraries or original game data are included. This packages the
current incomplete port. Android optimized local APK: `android/gradlew -p
android assembleRelease -PreleaseDebugSigning=true` (requires JDK 21).
Owner signing and current controls/import instructions are in
[docs/USER_GUIDE.md](docs/USER_GUIDE.md). `tools/ci.sh` runs local verification.
