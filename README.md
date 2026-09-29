# Road Rash native port research

Progress summary, timeline and next steps: [docs/PROGRESS.md](docs/PROGRESS.md). Binary evidence: [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md). Test log: [docs/VERIFICATION.md](docs/VERIFICATION.md). Plans to completion: [docs/plans/00-INDEX.md](docs/plans/00-INDEX.md).

Native C++17 / SDL3 work targeting macOS ARM64 and Android ARM64. `rr-game` is a **playable single-course solo ride** with the original bike physics; `rr-native` is the asset research viewer. It does not execute the Windows binary, use Wine, or emulate x86.

## Current implementation

- Original executable imported and analyzed in Ghidra: 1,585 functions exported as address-keyed pseudocode.
- Original disc inventory: 341 files with SHA-256 hashes and executable import table.
- Native, bounds-checked resource directory reader, validated against all 13 `CRSR` containers on this disc.
- Native JPEG decoding for six original menu screens.
- Native palette decoding for the original 190×64 dashboard and 720×320 Napa and city horizons. Dimensions traced to original executable routines.
- Native decoding of all five courses' sections, lane widths and branch graph.
- Integer port of the original road projection: 33 perspective slices, curvature/height integration and the original depth table read from `ROADRASH.EXE`, including the second roadway at splits and merges. The viewer drives through each course with flat placeholder shading.
- Shared desktop/Android viewer source with keyboard, mouse and touch navigation.

No racing, bike physics, scenery, road textures, AI, combat, audio, video, save games, or original menu interactions have been ported. Decompiler output is research input: inferred signatures, types and library labels still need correction. A successful decompilation export does not imply semantic correctness or recompilable source.

## Play (single course)

```sh
cd /Users/archit/Dev/rr/native-port
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel
build/rr-game --assets ../cdrom/ROADRASH            # Napa; --course 0-4, --bike 0-14
```

Arrows or WASD: Up accelerates, Down brakes, Left/Right steer. Gamepad: A or right trigger accelerates, B or left trigger brakes, D-pad or left stick steers. Touch: left/right thirds steer, bottom middle brakes, any other touch accelerates. Enter, A or a tap restarts after the finish. Esc quits.

The race uses the original 60 Hz bike physics ported from the executable (throttle, brakes, 7-gear shifting, drag, traction, steering, curve drift, crests and jumps) on the original course geometry. The rider and all graphics are placeholders, and there are no opponents, traffic, police or combat yet. See `docs/PROGRESS.md` for what is simplified.

## Run the research viewer on this Mac

```sh
cd /Users/archit/Dev/rr/native-port
./run-macos.sh
```

The default assets directory is `../cdrom/ROADRASH`; an alternative extracted original directory can be passed as the first argument. Left/Right, Space, click, or tap cycle nine images and five course views. On course views, Up/Down change drive speed (negative reverses) and B switches split branch. Escape exits. `--screen 9 --position 1400 --screenshot out.bmp` captures one frame.

Dependencies: CMake, a C++17 compiler and SDL3 (`brew install cmake sdl3`). The local build links Homebrew SDL3; it is not yet a standalone distributable app bundle.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
build/rr-native --assets ../cdrom/ROADRASH --smoke
build/rr-inspect ../cdrom/ROADRASH/DATA/BIKESPEC.RSC
build/rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE ../cdrom/ROADRASH/DATA/COURSES/*.CRS
python3 tools/check_corpus.py ../cdrom/ROADRASH
```

## Android

The Gradle project is `android/`; it builds the game (`src/game_main.cpp`) into `libmain.so` for `arm64-v8a`. SDL's Java activity supplies the Android lifecycle and input bridge. The APK is a debug research viewer and includes 17 assets copied from the local disc: the menu images, palette, bike specs, five courses and `ROADRASH.EXE` (read only for its projection table).

```sh
python3 tools/prepare_android_assets.py ../cdrom/ROADRASH
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
android/gradlew -p android assembleDebug
```

SDK location is configured in the local, ignored `android/local.properties`. This workspace has its SDK in `.tools/android-sdk`. Required packages: `platforms;android-35`, `build-tools;35.0.0`, `ndk;28.2.13676358`, `cmake;3.22.1`, and `platform-tools`. Gradle 8.11.1 and Android Gradle Plugin 8.9.2 are pinned. The bundled SDL3 3.4.8 AAR comes from the official Android development archive, with its SHA-256 verified before extraction.

Output: `android/app/build/outputs/apk/debug/app-debug.apk`.

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
