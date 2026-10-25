# Using the development port

The port is under active development. It currently provides a playable race,
verified disc import, native cutscene playback, menus, career/shop/saves,
race effects/engine sounds and the full settings/pause screens. Original
world scene construction, traffic/police and race MIDI remain unfinished.
[PROGRESS.md](PROGRESS.md) records the exact gaps.

## Requirements and installation

You need your own Road Rash 1997 Windows rerelease disc image or extracted
ROADRASH folder matching the verified inventory. No original game data is
included in the application or normal APKs.

macOS: Apple Silicon, macOS 13 or later. Build with
`tools/build_macos_app.sh`, then copy `build-release/Road Rash Native.app`
to your desired application folder. The build fetches a hash-pinned SDL3
source release and produces a self-contained app and ZIP. It uses a local
ad-hoc signature; Developer ID signing/notarization is not configured.

Android: ARM64, Android 6/API 23 or later. The SDK build uses JDK 21 and the
configured NDK. For local testing:

```sh
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
android/gradlew -p android assembleRelease -PreleaseDebugSigning=true
.tools/android-sdk/platform-tools/adb install -r android/app/build/outputs/apk/release/app-release.apk
```

This selects the local debug key for an optimized development APK. For an
owner-signed release, omit that flag and supply all four environment variables
or Gradle properties: `RR_KEYSTORE`, `RR_KEYSTORE_PASSWORD`, `RR_KEY_ALIAS`,
`RR_KEY_PASSWORD`. Keep passwords out of tracked files and shell command
arguments. Version name comes from the latest Git tag, falling back to
`0.1-research`. Hardware acceptance remains pending; builds and signatures
alone do not establish Android runtime behavior.

## Importing your disc

First launch presents the import screen. Select the ISO (or an extracted
folder on desktop). The importer checks all 341 required files against their
SHA-256 inventory, shows progress and commits only a complete matching copy.
Keep enough free space for the roughly 467 MB imported files. Nonseekable
document providers additionally require temporary space for the ISO; that
cache is removed after commit or cancellation.

Imported data is under SDL's application preference directory,
`RoadRashNative/RoadRash/game/ROADRASH`. On macOS this is normally
`~/Library/Application Support/RoadRashNative/RoadRash/game/ROADRASH`.
Android uses the application's private storage. The `RR_PREF_OVERRIDE`
environment variable changes the root for isolated developer runs.

A mismatch or missing file leaves the existing destination intact. The first
run screen displays the error. Android's actual Storage Access Framework
picker/import flow still needs device validation.

## Current race controls

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Accelerate | Up/W | South button/right trigger |
| Brake | Down/S | East button/left trigger |
| Steer | Left/Right or A/D | D-pad/left stick |
| Punch | Z/left Ctrl | West button |
| Kick | X/left Alt | North button |
| Backhand | C | Right shoulder |
| Pause/resume | P/Esc; Enter resumes | Start |
| Restart after finish | Enter | South button |
| Quit desktop | Q | — |

Touch uses screen zones, with independent fingers for steering and attacking.
Braking takes priority over touch auto-throttle. A tap resumes while paused
or restarts after a finish. Android Back pauses. Background/focus loss or
controller disconnect pauses; returning requires explicit resume.

Rebinding: Graphics Options → Configure Input lists the nine original key
bindings with the currently bound key each. Select a row and press the new
key; Escape cancels. Binding a key that is already used unbinds the other
action (the original rule). The joystick defaults are restored from the
Defaults rows. Gamepad mapping is fixed as in the table above; the Gamepad
page offers "Swap punch and kick" and a stick dead-zone slider (10–40 %).
The Touch page (touch devices) sets overlay opacity and a left-handed layout
that mirrors the touch zones.

Current race HUD and several behaviors are provisional, as documented
in PROGRESS.md. Career saves use the recovered original format; only a seeded
original-writer file has been round-tripped so far, not a driven career save.

## Settings

Graphics Options in the restroom menu holds the original settings plus three
native screens, all saved to `settings.txt` beside the saves folder (old
files still load; unknown key codes fall back per action):

| Screen | Original options | Native additions |
| --- | --- | --- |
| Graphics | Resolution, Bikers, Videos | Display screen: window/fullscreen, window size, scaling, filtering, render resolution, widescreen, visual style, VSync, FPS counter, touch opacity, haptics |
| Sound | — (dialog 300) | Sound screen: master + six channel sliders (music, effects, engines, others, sirens, interface), mute, swap speakers |
| Configure Input | Nine key bindings, Defaults | Gamepad page (swap punch+kick, dead zone), Touch page (opacity, left-handed) |

In-race pause (P/Esc/Start, Android Back): RESUME, SOUND, DISPLAY, CONTROLS
and QUIT RACE over the darkened race frame. QUIT RACE asks for confirmation
and leaves the race as a wrecked result. Sub-screen Back returns to the pause
menu. Sound sliders apply live while paused.

Hotkeys: Alt+Enter or Cmd+F toggles fullscreen; F10 cycles the MiniDash
display; `/` in menus toggles the opponent list; the FPS row shows frames and
simulation ticks per second in the race's top-right corner.

Frame interpolation (upcoming Enhancements screen): with it off every frame
is the newest simulation tick, exactly as before; with it on the game blends
positions between ticks for smoother motion on high-refresh displays, at most
one tick (16.7 ms) of extra input-to-photon delay.

Widescreen (Display screen): Off keeps the original 4:3 view; 16:10,
16:9 and Match display widen it (Hor+: same height, more road at the
sides, centre unchanged). The dashboard stays centred with darkened side
panels; in Enhanced/Custom visual style, menus stay centred at 640x480 with
blurred, darkened side bars derived from the current background (Original
visual style and videos keep black pillarbox bars). 4:3 is the original-exact mode.

Visual Style (Display screen): Original is the exact 1996 renderer; Enhanced
applies the measured preset (interpolation, perspective-correct textures,
trilinear filtering, fog 40, dusk grade, bloom, vignette, motion blur,
match-display widescreen — all under the frame budget, see VERIFICATION.md);
Custom opens the Enhancements screen with one row per effect. Any manual edit
shows Custom. Draw distance stays 33 slices (original limit).

Render Resolution (Display screen): Match display picks the largest of
640x480, 1280x960, 1920x1440, 2560x1920 fitting the window; the fixed
choices pin one. 640x480 is the original-exact mode; higher scales cost
GPU time (roughly 4x pixels per step). The original Low/Inset Resolution
choice locks it to 640x480. Applies live in menus and in the pause menu;
the next race frame is complete and the tick counter never moves.

## Menu controls

Launch without race options to enter the menus. Keyboard arrows or Tab move
focus, Enter/Space selects, and Escape/Android Back returns. Touch or click a
menu item to select it. On gamepad, use the D-pad or left stick to move focus,
South/Start to select and East/Back to return. The stick chooses one direction
on diagonals and repeats after 350 ms, then every 100 ms while held.

Controllers already connected when entering menus or videos are detected.
Disconnecting the active menu controller pauses; South/Start or a tap resumes.
After a pause, video or race, release the stick to neutral before navigating
again. This prevents a held race steering input from moving menu focus.
Physical controls on the MANGMI AIR X still require device validation.

## Developer commands

```sh
build/rr-game --assets ../cdrom/ROADRASH --course 0 --level 1 --bike 0
build/rr-game --assets ../cdrom/ROADRASH --video VIDEO/INTRO.AVI
build/rr-game --assets ../cdrom/ROADRASH --video VIDEO/RAT/BANZAI.AVI
tools/ci.sh
```

Course indexes: 0 NAPA, 1 CITY, 2 HIWAY, 3 CANYN, 4 MEDLY. Levels are 1–5,
bike indexes 0–14. Videos support P/Esc/Start pause, input to skip, input to
resume while paused, and desktop Q to quit. The explicit video path bypasses
the menu flow.

`RR_CODEC_ORACLE=1 tools/ci.sh` adds the full FFmpeg codec comparison;
`RR_STANDALONE_APP=1 tools/ci.sh` also builds/verifies the macOS bundle.
FFmpeg is used only by tests. Runtime playback needs no FFmpeg or Wine.

If audio cannot open, explicit video playback uses a silent wall-clock
fallback. Device A/V latency and physical audio-focus handling are not yet
measured. MIDI/SBK playback is still open; there is no current GM bank setting.
