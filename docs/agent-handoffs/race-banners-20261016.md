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

## Remaining / not established

- No original capture compares countdown/fade frames pixel-for-pixel; GDI
  glyph raster is the existing `TextRenderer` approximation.
- Before the first fade-in upload native shows black; the original still
  shows the previous (loading screen) palette for up to 14 ms.
- Native FPS overlay and pause menu are not dimmed (native surfaces). The
  original pause message (small Badloc `DAT_004c5bd0`, half palette) remains
  native per plan 18.
- Quit-confirm (outcome 5) still ends the native race immediately; the
  original keeps running 0xb4 ticks and fades out. Native no-fade branch
  (`DAT_00482b68`, network) not modelled.
- Multiplayer scoreboard/chat text in `0x00446c30` is out of scope (plan 09).
