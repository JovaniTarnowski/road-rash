# Race banners: original countdown, no wipeout text (October 16, 2026)

Task: review the in-race "GET READY" / "WIPEOUT!" / finish banners and replace
them with the original's in-race presentation. Baseline `59f717f` (HEAD moved
to `e9115c4` during the session, which touched only `tests/race_cache_test.cpp`).
Build dir `build-agent-overlay-20261016`. No live processes, locks, Wine or
Ghidra writes held. Nothing committed.

## Recovered facts (project exports, `analysis/decompiled/`)

- No original "WIPEOUT" or "GET READY" text exists. ENU.DLL strings 176–183
  (`WRECKED!`, `BUSTED!`, `WINNER!`, ...) are results-screen strings;
  207 `Get ready to race!` is the GAME LOADING screen.
- The only single-player in-race banner is the countdown. `0x00442dd0` sets
  `DAT_004753bc = DAT_004753c0 = 0xb4` (0x168 network). `0x00442e10`
  decrements it by the frame's steps and does **not** call the stepper
  `0x0044b2e0` while it is positive; at expiry it uploads the full palette
  `DAT_004b8490`.
- `0x004163d0` race start fades in (`0x00415f70(0x00415d90)`) or, in the
  no-fade branch, uploads `DAT_004b8030` directly: the half-brightness copy
  (`0x00448f90`: every channel `>> 1`). So the countdown shows a
  half-bright view. (Fade target inferred from the no-fade branch; the
  `__ftol` sources in `0x00415d90` were not traced.)
- `0x0044c780` calls the dashboard draw `0x00446c30` only when
  `DAT_004753c0 != DAT_004753bc`: no dashboard on the start-value frame.
- `0x00446c30` countdown block (outside the MiniDash != 2 test):
  `sprintf("%d", DAT_004753c0/0x3c + 1)` into `DAT_004c5bd0` with the big
  Badloc font `DAT_004c11ec` (0x80 high / 0x48 low resolution, `0x00442990`),
  rect {view left, view top + (0x78 >> (1-HiRes)), view right, view bottom},
  `0x00447b20` flags 0x4851: black at the rect, then white at
  −`DAT_0049e120` (HiRes+1). Digits: 179..120 → 3, 119..60 → 2, 59..1 → 1.
- `DAT_004c5bd0` is otherwise written only by the pause message
  (`0x004431d0`, vsprintf, also dims the palette when the countdown is 0) and
  the network race end (`0x00416010`, string 430). No crash or finish text.
- Docs line in plan 21 Task 4 trace labelled `0x4b8030` "full-bright"; per
  `0x00448f90` it is the half-bright copy and `0x4b8490` the full palette.

## Changes

- `src/hud.hpp/.cpp`: `CountdownBanner countdown_banner(countdown, start)`
  and `Hud::draw_countdown` (Badloc 0x80×scale, black then white −2×scale,
  high-resolution geometry as the dashboard labels use).
- `src/race_caches.hpp` `CourseCanvas::draw`: skip the dashboard on the
  start-value frame; draw the digit into the composed dashboard image (so it
  centres with the 4:3 dash under Hor+); half palette (indexed path, cached
  per palette generation) or `>> 1` per channel (RGB path) while the
  countdown runs.
- `src/race_helpers.cpp` `draw_hud`: removed `WIPEOUT!` and `GET READY n`.
  A debug digit remains only when no scene/dashboard is loaded; the
  `FINISHED` banner is limited to the native quick race (no career results
  screen there) and labelled native.
- `tests/hud_test.cpp`: banner timing/digits (pure) and digit raster
  (colours, centring, top ≥ 118; disc-gated).

## Evidence / checks

- `cmake --build build-agent-overlay-20261016 --parallel` exit 0;
  `ctest --test-dir build-agent-overlay-20261016 --output-on-failure -j8`
  99/99 passed, zero failures (race_cache RGB baselines unchanged: they draw
  at tick 300).
- Screenshots `rr-game --ticks 30|200 --screenshot`: tick 30 half-bright
  view with a centred Badloc "3"; tick 200 full brightness, no banner.
  (Scratch only, not committed.)

## Follow-up: palette fades and low-resolution metrics (same day)

Recovered from `analysis/original-disassembly.txt` (0x415d90..0x415f6a):

- `0x00415f70` runs the worker on `timeSetEvent(14, 1, ...)` and blocks on a
  semaphore; counter `0x482350` starts at 0x40, each callback decrements it,
  uploads while > 0 (63 uploads) and releases at 0 (64th callback, 896 ms).
- Fade-in `0x00415d90`: on the first call rebuilds `0x467860[1..63]` as
  `prev * 0.96` (`0x462000`, double) from `table[0] = 1.0`; upload n uses
  `table[n-1]` (n = 63..1): `trunc(half[c] * 0.96^(n-1))` via `fildl`/`fmull`
  /`_ftol`. The last upload is exactly the half palette.
- Fade-out `0x00415e90`: copies the half palette `0x4b8030` to `0x482358`,
  then every callback `x = trunc(x * 0.96)` in place (cumulative) and uploads.
  It runs when `0x00442e10` ends the race (after the decided race's 0xb4
  ticks).
- `0x004163b0` (loop clock base) runs after the fade-in, so no catch-up
  ticks accrue during it.

Native: `fade_in_lut`/`fade_out_lut`/`half_palette_lut` (`src/hud.*`) as
per-channel maps; `Game::fade_lut` (presentation only); `RaceScreen::update_fade`
drives them at 14 ms per upload, holds input/simulation during the fade-in,
resets the clock on release, and ends a decided career race after the
fade-out. `CourseCanvas` applies the map to the whole frame (indexed palette
cache keyed by generation + map; RGB per channel). Low mode countdown now
draws Badloc 0x48 at +60 with a 1-pixel shadow in 320x240 and pixel-doubles.

Checks: full build exit 0; `ctest` 99/99. `hud-test` pins the maps (black
before upload 1, first upload `trunc(127 * 0.96^62)`, last = half, fade-out
121/116 truncation) and the low raster; `race-screen` walks a restart through
the fade (countdown held, 30th-upload map, half palette at upload 63, release
at 64 × 14 ms with zero catch-up, then normal ticking).

## Follow-up 2: pump messages, quit, fade hold, original pixel check (October 16)

Recovered (disassembly `0x423345..0x4235a0`, exports `004431d0.c`,
`00442e10.c`, `00416010.c`):

- Key-up dispatch in the wndproc: Escape (`0x423378`) is ignored while
  `DAT_004753c0 == DAT_004753bc` (start frame). Racing with mode != 3 →
  `0x423408`: pump `0x004431d0(1, string 316)`. Return 1 →
  `PostQuitMessage(0)` and outcome 5 if still racing. `0x00442e10` sets
  `DAT_004753b4` on `WM_QUIT`, so the race ends on the next frame. There
  is **no** 0xb4 run-on (that delay belongs to `0x00416010` decided
  outcomes). This corrects the previous handoff. Then the fade-out runs.
- `VK_PAUSE` (0x13) → `0x4234f7`: when the countdown is off its start value
  and `DAT_004753b0` ∈ {1, 2}, `0x004431d0(0, string 317 "Game paused.\nHit
  any key to continue.")`. `'V'` (0x56) → string 380 (version), not ported.
- `0x004431d0`: sprintf into `DAT_004c5bd0`, `DAT_004753b8 = 1`, half
  palette when the countdown is 0, render-only loop (no simulation). F10
  cycles MiniDash, PrintScreen requests a screenshot, any other key-up exits
  2 (Escape exits 1 when param 1). Joystick buttons exit 2. Exit 2 restores
  the full palette when the countdown is 0.
- `0x00446c30` draws `DAT_004c5bd0` in the countdown rect with the face
  already selected: small Badloc `DAT_004c1210` (0x20/0x12), black then
  white at −(HiRes+1).

Native:
- `Hud::draw_message` (shared `draw_banner` with the countdown).
- `Game::banner_message` is set by `RaceScreen` each frame: quit prompt
  (career Escape) or pause message (`SDLK_PAUSE`, or a lifecycle pause with
  no native menu). Drawn in the race frame over the half palette.
- `RaceAction::pause_message`/`resume_message`: any key (except F10 and
  PrintScreen), pad button or tap resumes.
- Quit-confirm ends the race at once, outcome wrecked, and fades out from
  the half palette. Escape and Pause are ignored on the start frame and
  during fades.
- Fade-in before its first upload now presents nothing new, so the previous
  screen stays up instead of black.
- Fixed: the fade-out release frame could step a burst of catch-up ticks;
  the step loop now also requires `running_`.
- Strings 316/317 come from the career front end; the quick-race path
  loads 317.

Original pixel check: existing local capture `napa-dash-solo` (post-dash
`-dN.raw`, demo countdown, d2 "3", d36 "2", d70 "1") vs `Hud::draw_countdown`,
in region x 295–344, y 130–221, white index 255, shadow index 0:
"3" white 1096/1315, shadow 163/349; "2" 1167/1322, 196/346; "1" 746/750,
162/162. The glyph outlines match. Native "3" has one extra top row and
sits one column right, "2" has one extra top row: GDI hinting in the shared
`TextRenderer`. Changing it would move every text golden, so it was left
alone. `hud-test` pins these numbers when the capture is present.

Checks: full build exit 0; `ctest` 99/99. `race-actions` covers the Pause
key/resume. `race-screen` covers the pause freeze and banner, the quit
banner and freeze, the quit fade-out from half with zero run-on ticks, and
release → wrecked.

## Remaining / not established

- Palette state is not in the captures: the proxy's logged `0x0049e138`
  palette is all zeros. Proving the countdown/pause dimming and both fade
  curves against the original needs a proxy hook on the DirectDraw palette
  `SetEntries` and a new Wine capture (with Escape/Pause key injection for
  the message frames). The oracle proxy claim `terrain-integrate-20261015`
  is still registered; its owner PID 53167 is gone, but it was not reaped.
- GDI hinting offsets for "3"/"2" (above).
- String 380 (`V` key version message), the in-pump F10 MiniDash cycle and
  PrintScreen are not ported; the native race canvas always uses the full
  dashboard.
- Native FPS overlay and the native pause menu (plan 18) are not dimmed.
- Multiplayer scoreboard/chat text in `0x00446c30` is out of scope (plan 09).
