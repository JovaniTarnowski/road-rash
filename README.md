# Road Rash — Native Port

Native C++20 / SDL3 port of the 1996 Windows rerelease of Road Rash.
Runs the game directly on modern systems: no Wine, no x86 emulation at runtime.

Primary target is macOS ARM64, second is Android ARM64. Linux and Windows
are planned (see `docs/plans/33-linux-windows-ports.md`). The port is
incomplete; see Status below.

Reference for reverse engineering is the unmodified disc binary at
`../cdrom/ROADRASH/ROADRASH.EXE`. Oracle captures run it under Wine to
produce comparison traces. It is never executed by the game itself.

## Status

Playable today via `rr-game`: full menu / career / shop / save flow leads
into races on all five courses (Napa, City, Highway, Canyon, Medley) with
the original bike physics, AI field, combat, traffic, pedestrians, engine /
effect audio, soundtrack, and cutscene playback.

What matches the original on receipted captures:

- Napa level 1, full 14-rider offline field: 2400/2400 ticks identical.
- Napa level 1, recorded human-input run: 1431/1431 ticks identical.
- Full native suite: 105/105 CTests, zero skips; sanitizer suites clean.

Still open (see `docs/CURRENT_STATUS.md` and `docs/PROGRESS.md`):

- Simulation parity outside those captures: other courses/levels, human
  paths beyond the receipted run, isolated fight/steal/traffic cases.
- Gameplay: live police, wall/crash-sequence stand-ins, some car/pedestrian
  branches, race MIDI, sirens/car engines.
- Frontend: some option screens (mano-a-mano, jukebox, sound mix, controls),
  no driven-career original-save round trip, no menu pixel comparison.
- Android builds and packages but hardware validation (render, input,
  import, lifecycle, performance) is pending.
- Linux/Windows builds do not exist yet.

## Requirements

- Your own 1997 Windows rerelease disc (ISO or extracted `ROADRASH`
  folder, 341 files, SHA-256 verified on import). No game data is included
  or redistributed.
- CMake 3.22+, a C++20 compiler, SDL3 3.2+ (`brew install cmake sdl3`).
- Android builds additionally need JDK 21, Android SDK 35 / NDK 28, Gradle
  wrapper in `android/`.

## Quick start (macOS)

```sh
./run-macos.sh                       # Release build in build-play/, launch game
./run-macos.sh --debug               # Debug build in build/ (slower renderer)
```

Or with CMake directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
build/rr-game --assets ../cdrom/ROADRASH
```

Options:

```sh
build/rr-game --assets ../cdrom/ROADRASH --course 0 --level 1 --bike 0
build/rr-game --assets ../cdrom/ROADRASH --video VIDEO/INTRO.AVI
```

`--course 0-4` (Napa, City, Highway, Canyon, Medley),
`--level 1-5`, `--bike 0-14`. Omitting race flags enters the menu/career
flow. `--ticks N --screenshot out.bmp` runs a headless verification slice.

## Controls

| Action | Keyboard | Gamepad | Touch |
| --- | --- | --- | --- |
| Accelerate | Up / W | South / RT | Free zones (auto-throttle) |
| Brake | Down / S | East / LT | Bottom-middle zone |
| Steer | Left/Right, A/D | D-pad / left stick | Left/right thirds |
| Punch / Kick / Backhand | Z / X / C | West / North / RB | Top third |
| Pause | P / Esc | Start | System Back / pause tap |
| Restart after finish | Enter | South | Tap |
| Quit (desktop) | Q | — | — |

P/Esc/Start opens the pause menu (Resume, Sound, Display, Controls,
Quit Race with confirmation). Backgrounding, focus loss, or disconnecting
the active gamepad also pauses; resume is always explicit. Keys are
rebindable in Graphics Options → Configure Input; gamepad mapping is fixed
except punch/kick swap and stick dead zone.

## Disc import

First launch without `--assets` shows the import screen: pick the ISO or an
extracted folder. The importer verifies all 341 files by SHA-256 and commits
only a complete match; failures leave any existing install untouched.

Command-line equivalent:

```sh
build/rr-import "/path/to/Road Rash.iso" "/path/to/imported/ROADRASH"
build/rr-game --assets "/path/to/imported/ROADRASH"
```

Default destination is the SDL pref dir `RoadRashNative/RoadRash/game/ROADRASH`.
`RR_PREF_OVERRIDE` redirects it for isolated test runs.

## Tools

| Binary / script | Purpose |
| --- | --- |
| `build/rr-game` | The game (menus, career, races, video playback) |
| `build/rr-viewer` (`./run-viewer.sh`) | Asset research viewer, not the game: screens, course drive-through |
| `build/rr-import` | CLI disc import with hash verification |
| `build/rr-inspect FILE.RSC` | Dump a resource container directory |
| `build/rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE DATA/COURSES/*.CRS` | Course graph / projection sweep |
| `python3 tools/check_corpus.py <assets>` | Validate all resource containers |
| `python3 tools/render_demo.py` | Render a 2-minute 5-course demo MP4 from the engine |
| `tools/bench_game.sh user original enhanced` | Race frame-time benchmark (`docs/PERFORMANCE.md`) |
| `tools/ci.sh` | Local verification gate (native build, tests, APK assembly) |

Video playback uses native Cinepak / IMA-ADPCM decoders. FFmpeg appears
only in tests as a decode oracle; runtime needs neither FFmpeg nor Wine.

## Android

```sh
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
android/gradlew -p android assembleDebug
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk`.
Normal APKs ship no game data; on first launch pick your own ISO and it is
verified the same way as desktop. SDK location lives in the ignored
`android/local.properties`. Optimized local build:
`assembleRelease -PreleaseDebugSigning=true`. Owner signing and versioning
are documented in `docs/USER_GUIDE.md`. Device behavior is unverified.

## macOS packaging

```sh
tools/build_macos_app.sh
```

Produces `build-release/Road Rash Native.app` and a ZIP with pinned static
SDL3, ARM64 / macOS 13 baseline, bundled licences, ad-hoc signature, and no
game data. Pushing a `v*` tag builds and attaches the same package via
GitHub Actions.

## Docs map

- `docs/CURRENT_STATUS.md` — evidence matrix and latest gate receipts.
- `docs/PROGRESS.md` — what is ported, approximations, next steps.
- `docs/VERIFICATION.md` — commands run, outcomes, skips, evidence paths.
- `docs/REVERSE_ENGINEERING.md` — recovered structures and addresses.
- `docs/USER_GUIDE.md` — install, import, settings, controls reference.
- `docs/plans/00-INDEX.md` — plan dependency order to completion.
- `docs/AGENT_TOOLING.md` — agent workflow, gates, capture rules.

Commercial assets, disc dumps, and upscaled artwork stay local and out of
the source tree, APKs, and app bundles. Third-party licence texts are in
`third_party/`. No redistribution licence for the game is implied.
