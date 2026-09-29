# Road Rash native port: plan index

These plans take the port from its current state (a research viewer that renders original road geometry) to a complete, playable native game on macOS ARM64 and Android ARM64. Each plan covers one subsystem, can be executed by an agent with no prior context, and ends in working, tested software.

Read this file first. It defines the conventions, the verification method and the order that every plan assumes.

## Where things stand

See [../PROGRESS.md](../PROGRESS.md) for the status table and [../REVERSE_ENGINEERING.md](../REVERSE_ENGINEERING.md) for all recovered formats and function addresses. In short:
- **Done:** resource directories, course graph, lanes, shoulders, the full road projection including split/merge roads, menu/dashboard/horizon image decoding, macOS and Android builds of a viewer.
- **Not started:** sprites, scenery, simulation, AI, combat, HUD, menus, audio, video, saves, final packaging.

## Plans and order

| # | Plan | Depends on | Produces |
| --- | --- | --- | --- |
| 01 | [Foundation and verification oracle](01-foundation-and-oracle.md) | — | git repo, module layout, 8-bit framebuffer, Wine oracle that dumps original game state, golden-test harness |
| 02 | [Road and world rendering](02-road-and-world-rendering.md) | 01 | Original-accurate road raster: colours, terrain profiles, horizon, occlusion; oracle-matched |
| 03 | [Sprites and scenery](03-sprites-and-scenery.md) | 01, 02 | Sprite/animation decoders, roadside objects, bikes/riders/cars drawn in world |
| 04 | [Race simulation](04-race-simulation.md) | 01, 02 | Fixed-tick race loop, player bike physics, camera, deterministic replay tests |
| 05 | [Opponents, traffic, police, combat](05-opponents-traffic-combat.md) | 03, 04 | AI riders, traffic, cops, weapons/attacks, crashes, race results |
| 06 | [Front end, progression, saves](06-frontend-progression-saves.md) | 03, 04 | Menus, text, bike shop, levels/cash, save/load, full game flow |
| 07 | [Audio and video](07-audio-and-video.md) | 01 | Sound effects, music (RRA + MIDI/SBK), engine audio, AVI cutscenes |
| 08 | [Platform, input, packaging](08-platform-input-packaging.md) | 04–07 | Controls (keyboard, gamepad, touch), lifecycle, asset import, signed .app and release APK |
| 09 | [Multiplayer (decision-gated)](09-multiplayer.md) | 05, 06, 08 | Network play, only if the owner decides to keep it |

Plans 02 and 07 can run in parallel after 01. Plans 03 and 04 can run in parallel after 02. Plan 06 can start its text/menu tasks as soon as 03's sprite decoder exists.

**Completion** means plans 01–08 are done and the definition of done below holds. Plan 09 is optional.

## How these plans handle reverse engineering

Most remaining work recovers behaviour from a stripped 1996 binary. The exact native code cannot be written in advance, because the semantics are not yet known. So these plans pin every such task down differently:

- **A named starting point:** the function addresses, globals and structure offsets already traced, and which decompiled files to read (`analysis/decompiled/<address>.c`, disassembly in `analysis/original-disassembly.txt`).
- **An oracle test written first:** the original game runs under Wine with a hook DLL (plan 01) that dumps its real state. The task captures that ground truth into a golden file and writes a failing native test against it before porting anything.
- **A port rule:** translate the named function(s) line by line into fixed-width integer C++, keeping original arithmetic (shifts, truncating divides, 32-bit wraparound) exactly. No "improvements" until the golden test passes.
- **Acceptance:** the golden test passes with zero tolerance unless the task states a tolerance and why.

Wherever interfaces and infrastructure are known, the plans give complete code.

## Conventions every agent must follow

### Environment
- **Repo:** `/Users/archit/Dev/rr/native-port`. After plan 01 task 1 it is a git repository; before that it is not.
- **Shell:** fish. Use `bash -c '...'` for bash syntax such as `for`/`set --` loops, `$(...)`, `&&`-heavy scripts.
- **Original game data:** `../cdrom/ROADRASH` (mounted ISO). Mount it if missing: `hdiutil attach -readonly -nobrowse -mountpoint /Users/archit/Dev/rr/cdrom "/Users/archit/Dev/rr/Road Rash (USA) (En,Fr,De,Es,It) (Rerelease) (19970930).iso"`.
- **Reference executable:** `../cdrom/ROADRASH/ROADRASH.EXE`, SHA-256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`. Never use the patched copy in `/Users/archit/Dev/rr/android/Download` (different hash).
- **Wine:** `$HOME/Applications/Wine Staging.app/Contents/Resources/wine/bin/wine` (wine-11.18 Staging). Prefix: `/Users/archit/Dev/rr/prefix`; the game is installed at `prefix/drive_c/ElectronicArts/RoadRash` (hash verified identical).
- **Android:** SDK in `.tools/android-sdk`; `export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`. The device (MANGMI AIR X) is usually disconnected; check with `.tools/android-sdk/platform-tools/adb devices`.

### Build and test
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

After any change to shared code, all of these must still pass:
- `ctest`
- `build/rr-native --assets ../cdrom/ROADRASH --smoke`
- `build/rr-course --exe ../cdrom/ROADRASH/ROADRASH.EXE ../cdrom/ROADRASH/DATA/COURSES/*.CRS`

### Code rules
- C++17, SDL3 only (plus the single-header libraries listed in the plans). No new dependency without the licence file in `third_party/`.
- All original data parsing is bounds-checked and throws `std::runtime_error` on malformed input. Every parser gets a truncation/garbage test, like `tests/resource_test.cpp`.
- Fixed-width integers for anything ported from the original. Emulate 32-bit wraparound with `uint32_t` arithmetic where the original can overflow.
- Comments cite the original address a rule came from, e.g. `// 0x00444630`.
- **Never embed original game data** (tables, images, text, audio) in source or tests. Read them at runtime from the user's files. Golden files derived from the game live in `analysis/oracle/` (git-ignored) and tests that need them skip cleanly (CTest `SKIP_RETURN_CODE 77`) when absent.
- Tests using only synthetic data must always run.

### Evidence and documentation
Every task that recovers a format or behaviour must update, in the same commit:
- `docs/REVERSE_ENGINEERING.md`: the finding, its evidence (addresses), and how it was validated
- `docs/VERIFICATION.md`: what was run and what passed or failed
- `docs/PROGRESS.md`: status table and timeline

Never claim a match with the original that was not measured. Write "not verified" where that applies.

### Commits
One commit per task, message `<area>: <what>` (for example `road: port terrain type 1 wall profiles`), ending with:
```
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
```

## Definition of done for the whole port

1. From a clean checkout plus the user's own Road Rash disc image, a documented import step produces a runnable game on:
   - macOS ARM64 (standalone `.app`, no Homebrew dependency)
   - Android ARM64 (installable APK; the disc data is imported at runtime and not bundled)
2. All five courses and all five difficulty levels are playable in the single-player modes the original offers: the career ("Big Game") and single "Thrash" races.
   - Opponents, traffic, police, weapons, crashes, cash, bike shop and saves all behave like the original.
3. Oracle golden tests for road projection, simulation ticks, AI and combat pass for the recorded scenarios listed in each plan.
4. Audio (effects, both music systems) and all cutscene videos play.
5. Controls work with keyboard, gamepad (including the MANGMI AIR X built-in controls) and touch.
6. Docs describe every recovered format and every known deviation from the original.

## Global risks

- **The oracle may be hard to build** (mingw-w64 toolchain, Wine hook stability). Plan 01 has fallback paths (winedbg scripting, pure frame capture). Without an oracle, fidelity claims are limited to self-consistency and must be labelled as such.
- **Unknown data formats:** sprite compression and `.SBK` banks are the largest unknowns. Each has a dedicated research task with a time box. If it overruns, stop and report rather than guess.
- **Licensing:** the port may never ship original assets. Android must import from the user's disc rather than bundling (plan 08).
