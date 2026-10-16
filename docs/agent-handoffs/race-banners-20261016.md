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

## Remaining / not established

- No original capture compares the countdown frame pixel-for-pixel; GDI
  hinting/glyph raster is the existing `TextRenderer` approximation.
- The blocking ~0.9 s black-to-half fade-in at race start (64 steps × 14 ms)
  is not ported; native starts at half brightness.
- Low-resolution geometry (0x48 font, +60) is not separately modelled;
  low mode uses the high metrics in the 640 compose space.
- Native FPS overlay and pause menu are not dimmed (native surfaces). The
  original pause message (small Badloc `DAT_004c5bd0`, half palette) remains
  native per plan 18.
- Multiplayer scoreboard/chat text in `0x00446c30` is out of scope (plan 09).
