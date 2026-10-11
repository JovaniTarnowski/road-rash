# Android Port on Hardware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Task 1 needs only a connected device. Tasks 2–6 can start any time. Task 7 runs after plans 11–13.

**Goal:** Take the Android build from "assembles asset-free" to "verified on the MANGMI AIR X":
- the disc imports through the system picker;
- races run at full simulation rate with acceptable frame times;
- the built-in controls and touch both work;
- lifecycle, audio and saves survive real device use;
- an owner-signed release APK passes the hardware checklist.

**Architecture:** The game is the same C++ code as macOS (`rr-game` via SDL3's `SDLActivity`). This plan adds four things:
- a launch-argument path, so benchmark and diagnostic modes run on the device without a desktop shell;
- a frame-time benchmark;
- a performance path using the original's own low-resolution race view (`0x00448870`, settings `HiRes` `0x00475380`). The game currently ignores this setting and always renders high resolution;
- a visible touch overlay, a native addition.

Everything else is verification on hardware.

**Tech Stack:** SDL3 3.4.8 AAR, NDK 28.2, Gradle, `adb`, C++17.

**Spec:** [00-INDEX.md](00-INDEX.md) definition of done items 1 and 5. The old [08-platform-input-packaging.md](08-platform-input-packaging.md) Task 6 checklist is folded into Task 7 here.

## Global Constraints

- **Device:** MANGMI AIR X (ARM64, built-in controls). Check with `.tools/android-sdk/platform-tools/adb devices`. When no device is attached, do Tasks 2–6 in software only, and leave every hardware line in VERIFICATION.md explicitly "not run: no device".
- **APKs:**
  - APKs never contain game data; `verifyNoGameData` must pass;
  - `-PdevAssets=true` is for local debug research only;
  - never install the patched Wine-era setup from `/Users/archit/Dev/rr/android`.
- **Storage:** imported data lives in app-private storage (the `SDL_GetPrefPath("RoadRashNative","RoadRash")` tree, `src/importer.cpp`). Saves and settings live in the same pref path. Uninstalling deletes them, and the user guide must say so.
- **Targets:** simulation at 60 ticks/s with no dropped ticks in steady state, and render p95 ≤ 16.7 ms on the device in the default view mode.

## Review Focus

1. **Picking a 518 MB ISO over SAF on a slow SD card or USB stick:** progress must keep updating, cancel must work, and running out of space must give a clear error with nothing half-committed. Task 4 runs this on the device, including a low-space case created by filling storage with `adb shell` (`fallocate` in app storage).
2. **Controller disconnect or reconnect mid-race** (the AIR X controller can re-enumerate on resume): the race pauses and input recovers. Task 5.
3. **Screen off/on and the app switcher during a race and during a video:** the renderer reset path recreates every texture. Task 5.
4. **Display cutouts and very wide aspect ratios:** the letterboxed 640×480 view must not be hidden under a cutout, and the touch zones must map to the visible area. Task 3.
5. **Thermal throttling during long races:** the benchmark runs a full 10-minute Napa race, not just a minute. Task 2.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `android/app/src/main/java/org/roadrash/nativeport/MainActivity.java` | `getArguments()` from intent extras (debug builds only) |
| `src/game_main.cpp` | `--bench`, `--log-frame-times`; honour the `HiRes` setting for the race view |
| `src/frame_stats.hpp/.cpp` | Frame-time percentile accumulator (pure, testable) |
| `src/touch_overlay.hpp/.cpp` | Visible touch controls drawn over the letterboxed view |
| `tests/frame_stats_test.cpp`, `tests/touch_overlay_test.cpp` | Synthetic tests |
| `tools/android_device_check.sh` | Install, launch, benchmark, collect logcat and screenshots |
| `docs/USER_GUIDE.md` | Android import, controls and storage notes |

---

### Task 1: First device run and diagnostics script

**Files:**
- Create: `tools/android_device_check.sh`
- Modify: `android/app/src/main/java/org/roadrash/nativeport/MainActivity.java`

**Interfaces:**
- Produces: `tools/android_device_check.sh <apk> [args…]`. It installs the APK, launches it with `args`, waits, and saves `logcat` and `screencap` output to `analysis/android/<timestamp>/`.

- [ ] **Step 1: Launch arguments.** Implement them, for debuggable builds only, so that release builds ignore extras:

```java
package org.roadrash.nativeport;

import android.content.pm.ApplicationInfo;
import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    // Debug builds only: `adb shell am start -n org.roadrash.nativeport/.MainActivity -e args "--bench napa"`.
    @Override
    protected String[] getArguments() {
        if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0) return new String[0];
        String args = getIntent() != null ? getIntent().getStringExtra("args") : null;
        return args == null || args.trim().isEmpty() ? new String[0] : args.trim().split("\\s+");
    }
}
```

- [ ] **Step 2: Write the script.**

```sh
#!/bin/sh
# Usage: tools/android_device_check.sh <apk> [game args...]
set -eu
ADB="$(dirname "$0")/../.tools/android-sdk/platform-tools/adb"
APK="$1"; shift
OUT="analysis/android/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"
"$ADB" get-state >/dev/null 2>&1 || { echo "no device: $("$ADB" devices | tail -n +2)"; exit 3; }
"$ADB" install -r "$APK"
"$ADB" logcat -c
"$ADB" shell am start -W -n org.roadrash.nativeport/.MainActivity ${1:+-e args "$*"}
sleep "${RR_WAIT:-30}"
"$ADB" exec-out screencap -p > "$OUT/screen.png"
"$ADB" logcat -d > "$OUT/logcat.txt"
"$ADB" shell dumpsys gfxinfo org.roadrash.nativeport > "$OUT/gfxinfo.txt"
"$ADB" shell getprop ro.product.model > "$OUT/device.txt"
echo "wrote $OUT"
```

  `.tools` sits at the repo root `/Users/archit/Dev/rr/native-port/.tools`. Run the script from `native-port`.
- [ ] **Step 3: Run on the device.** Build with `android/gradlew -p android assembleDebug`, then run `tools/android_device_check.sh android/app/build/outputs/apk/debug/app-debug.apk`. Expected: the first-run import screen. Record in VERIFICATION.md the model, Android version, GL/Vulkan renderer name (from logcat `SDL`) and screen size. **If no device is attached, the script exits 3; record that and stop the task here.**
- [ ] **Step 4:** Commit: `android: debug launch arguments and device check script`.

---

### Task 2: Frame-time benchmark and the original low-resolution view

**Files:**
- Create: `src/frame_stats.hpp`, `src/frame_stats.cpp`, `tests/frame_stats_test.cpp`
- Modify: `src/game_main.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  class FrameStats {
  public:
      void add(uint64_t frame_ns, uint32_t sim_steps);   // one presented frame
      uint64_t percentile_ns(int p) const;               // nearest-rank, p in 1..100; 0 when empty
      uint32_t dropped_ticks() const;                    // frames that hit the 20-step cap
      size_t frames() const;
  };
  }
  ```
  CLI: `rr-game --bench <course> [--ticks N]` runs an autopilot race with rendering. It logs `bench frames=… p50=… p95=… p99=… dropped=…` through `SDL_Log`, which appears in logcat, then exits.

- [ ] **Step 1: Write the failing test.**

```cpp
#include "frame_stats.hpp"
#include <iostream>
int main() {
    int failures = 0;
    rr::FrameStats s;
    if (s.percentile_ns(95) != 0) { std::cerr << "empty\n"; ++failures; }
    for (uint64_t i = 1; i <= 100; ++i) s.add(i * 1000000, i == 100 ? 20 : 1);
    if (s.percentile_ns(50) != 50000000) { std::cerr << "p50 " << s.percentile_ns(50) << '\n'; ++failures; }
    if (s.percentile_ns(95) != 95000000) { std::cerr << "p95\n"; ++failures; }
    if (s.percentile_ns(100) != 100000000) { std::cerr << "p100\n"; ++failures; }
    if (s.dropped_ticks() != 1) { std::cerr << "dropped\n"; ++failures; }
    if (s.frames() != 100) { std::cerr << "frames\n"; ++failures; }
    if (!failures) std::cout << "frame stats ok\n";
    return failures ? 1 : 0;
}
```

- [ ] **Step 2:** Run the test and confirm it fails (missing header). Implement `FrameStats` (store samples, sort a copy for percentiles), then run it and confirm it passes.
- [ ] **Step 3: Honour `HiRes`.** The race canvas currently calls `rr::view_mode(true)` and a fixed 640×480 framebuffer. Make it use `settings.high_resolution`:
  - low mode renders 320×240 through `view_mode(false)`;
  - the HUD uses the low-resolution dashboard path from `0x00416310`. Trace it if `hud.cpp` lacks it, and add `hud_test` cases.

  Original pixel goldens exist only for high mode. Capture one low-mode `napa-drive-low` frame set with the original's `HiRes` off, and add it to `oracle-scene`.
- [ ] **Step 4: Measure.** On macOS, run `--bench napa --ticks 36000` in both modes and record the numbers. On the device, use `tools/android_device_check.sh <apk> --bench napa --ticks 36000` with `RR_WAIT=660`, in both modes, and record p50/p95/p99 and dropped ticks.
- [ ] **Step 5: Decide the default.** If high mode meets p95 ≤ 16.7 ms on the device, keep the original default. Otherwise profile with `simpleperf record -p <pid>` and attack the top function. Likely candidates are the textured span loop and the `to_rgba` conversion; a NEON or table-driven conversion is allowed if it is pixel-identical (`framebuffer_test`). Fall back to low mode as the Android default only if that fails, and document the choice.
- [ ] **Step 6:** Commit: `perf: frame-time benchmark; race view honours HiRes setting`.

---

### Task 3: Visible touch overlay and safe-area layout

**Files:**
- Create: `src/touch_overlay.hpp`, `src/touch_overlay.cpp`, `tests/touch_overlay_test.cpp`
- Modify: `src/input.cpp`, `src/input.hpp`, `src/game_main.cpp`, `docs/USER_GUIDE.md`

**Interfaces:**
- Consumes: `rr::TouchInput` (`src/input.hpp`), its current zones (steer halves, brake, top-third attack).
- Produces:
  ```cpp
  namespace rr {
  struct OverlayLayout { SDL_FRect steer_left, steer_right, brake, attack_fist, attack_kick, attack_weapon, pause; };
  OverlayLayout overlay_layout(SDL_FRect visible_view, SDL_Rect safe_area);   // pure; rects inside safe_area
  void draw_overlay(SDL_Renderer*, const OverlayLayout&, const PlayerInput& pressed);
  }
  ```
  `TouchInput` hit-tests against `OverlayLayout` instead of fixed fractions, so the visuals and hit zones cannot disagree.

- [ ] **Step 1: Write the failing tests.**
  - For a 2400×1080 window with a 100-pixel left cutout safe area, every rect lies inside the safe area and none overlap.
  - For 4:3, the layout still fits.
  - A touch at a rect's centre produces the matching control bit through `TouchInput`.
  - The overlay hides after a gamepad event and returns on the next touch.
- [ ] **Step 2:** Run the tests and confirm they fail. Implement, using `SDL_GetWindowSafeArea`. Run them and confirm they pass.
- [ ] **Step 3:** Smoke test on macOS with touch simulated from the mouse (`SDL_HINT_MOUSE_TOUCH_EVENTS`). On the device, take screenshots at each orientation the manifest allows.
- [ ] **Step 4:** Commit: `input: visible touch overlay in the safe area`.

---

### Task 4: Disc import on the device

**Files:**
- Modify: `src/import_ui.cpp`, `src/importer.cpp` (only if a defect is found), `docs/VERIFICATION.md`, `docs/USER_GUIDE.md`

- [ ] **Step 1:** Push the ISO: `adb push "../Road Rash (USA) (En,Fr,De,Es,It) (Rerelease) (19970930).iso" /sdcard/Download/`. Launch, pick it through the SAF picker, and time the import. All 341 files must hash-verify.
- [ ] **Step 2:** Cancel halfway, then relaunch. There must be no partial data, and the picker appears again.
- [ ] **Step 3:** Low-space case: fill app storage with `adb shell run-as org.roadrash.nativeport fallocate -l <size> files/fill` until less than 400 MB is free, then import. Expected: a clear "not enough space" error, staging cleaned up and nothing committed. **If the importer does not check space up front**, add a check (`std::filesystem::space` on the destination) before copying. Write a test with a fake space function first (`importer_test`).
- [ ] **Step 4:** Folder import: pick an extracted copy of the disc as a folder, if the SAF tree picker is supported by SDL's dialog on this Android version. Otherwise record it as unsupported and say so in the guide.
- [ ] **Step 5:** Commit: `android: device import verified` (plus `importer: free-space check` if Step 3 needed it).

---

### Task 5: Controls, lifecycle and audio on the device

**Files:**
- Modify: `src/input.cpp`, `src/game_main.cpp` as defects require, `docs/VERIFICATION.md`

- [ ] **Step 1: AIR X controls.** Log `SDL_EVENT_GAMEPAD_*` and the gamepad name and mapping string in debug builds. Confirm that every built-in button and both sticks reach SDL as a gamepad and not as keyboard keys. The old Winlator profile `RoadRash-AirX.icp` reports Android keycodes 96 (A), 97 (B), 99 (X), 100 (Y), 102 (L1), 103 (R1), 106/107 (thumbs), 108 (Start), 109 (Select). If SDL sees keys instead, add an SDL gamepad mapping hint for this device (`SDL_HINT_GAMECONTROLLERCONFIG`) with a test for the mapping string format.
- [ ] **Step 2: Checklist.** Run each item, record the result in VERIFICATION.md, and fix defects with a test first where the behaviour is testable on desktop:
  1. Menu navigation with the D-pad and stick; confirm and back.
  2. Race: throttle, brake, steer, the three attacks, pause and resume.
  3. Home → return during a race; during a video; during import.
  4. Screen off → on during a race (renderer reset).
  5. Controller disconnect/reconnect, if the device can toggle it (Bluetooth controller fallback).
  6. Audio: effects, engine, music; volume keys; headphones plug/unplug; no audio burst on resume.
  7. Kill the process (`adb shell am force-stop`) mid-career and relaunch: the career and settings persist.
  8. Back button opens the pause menu and never exits without confirmation.
- [ ] **Step 3:** Commit: `android: controls and lifecycle verified on device`.

---

### Task 6: Owner release signing

**Files:**
- Modify: `docs/USER_GUIDE.md`, `docs/VERIFICATION.md`

- [ ] **Step 1:** The owner creates the release keystore. **Claude does not create or handle passwords:** the owner sets `RR_KEYSTORE*` in `~/.gradle/gradle.properties` or the environment.
- [ ] **Step 2:** `android/gradlew -p android assembleRelease verifyNoGameData` with the owner key. Check with `apksigner verify --print-certs` (v2/v3).
- [ ] **Step 3:** Install the signed release APK over the debug build (expect a signature conflict: uninstall first, which deletes imported data and saves; the guide says so). Re-run Task 4 Step 1 and Task 5 items 2 and 7 on the release build.
- [ ] **Step 4:** Commit: `docs: owner-signed Android release verified`.

---

### Task 7: Full-game device pass (after plans 11–13)

- [ ] **Step 1:** On the release APK, play a complete Napa career race and one race on each other course. Record benchmark numbers for each course (`--bench <course>` on a debug build of the same commit).
- [ ] **Step 2:** Run the attract demo, the jukebox, a level-up video and the save/load flow.
- [ ] **Step 3:** Record everything in VERIFICATION.md and set the PROGRESS.md "Android build" row to hardware-verified, with the date and device. Commit: `android: full game verified on MANGMI AIR X`.

## Exit criteria

- Every Task 5 checklist item passed on the device, with dates.
- The benchmark meets the target in the chosen default mode, and the owner-signed release APK is verified.
