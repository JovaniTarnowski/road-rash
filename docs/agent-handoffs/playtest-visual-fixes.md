# playtest-visual-fixes

Task: Fix playtest visuals: menu scaling, loading screen, video size, HUD meter, start-grid clipping, non-Napa courses

Baseline: 5dbf6adc415f864ef755ca292c237c001511d121

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-playtest-visual-fixes`
Owner PID: 75342
Resource token: `b10503b5-e0c9-42c9-9eda-d34796da50c5`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/game_main.cpp`
- `/Users/archit/Dev/rr/native-port/src/video_player.cpp`
- `/Users/archit/Dev/rr/native-port/src/hud.cpp`
- `/Users/archit/Dev/rr/native-port/src/frontend.cpp`

Also changed (unclaimed, clean before this task): `src/video_player.hpp`,
`src/course_scene.cpp`, `tests/hud_test.cpp`, `tests/frontend_test.cpp`.
`src/game_main.cpp` already carried another session's uncommitted plan 11a
Task 1 edits (`steer_into_traffic`, hunt logging); those were kept. Their edit
had dropped the quick-race `run_race` call (so `--race`/`--course` opened no
window and `--screenshot` did nothing); restored as
`if (!into_traffic) run_race(...)` after the tick loop.

## Progress / evidence

Owner playtest (October 12, 2026) reported six visual faults. Each fix, cause and evidence:

1. **Menu text not crisp.** 640x480 frames were upscaled with nearest sampling at
   non-integer factors (1.5x) without a high-density backbuffer, so glyph columns
   alternated 1/2 px and macOS upsampled again. Now `SDL_WINDOW_HIGH_PIXEL_DENSITY`
   and `SDL_SCALEMODE_PIXELART` (SDL >= 3.4, nearest fallback) for menu, course
   canvas, dashboard, sprites and video. Default window 960x720 (4:3).
2. **Loading screen image clipped.** Thrash mode asks for `Chars/Thrash-F`
   (`0x0041d7d0` case 8 via `0x0041bf90(0)` = "Thrash"), which does not exist, so the
   previous course postcard stays. `0x0041e240` builds a `Postcard/` or `Street/`
   picture by copying the cached base picture (0x96000 bytes) and decoding the
   inset over it; native drew the 397x480 inset alone at x=0 on black. Native now
   composites: `Postcard` (243x480) at 0,0 with the inset at x=640-w=243;
   `Street/*` (258x250) over `Street` at (146,230). **Offsets come from artwork,
   not code:** seam continuity (base|inset column diff 11 vs inset|base 109,
   typical 8.6) and pixel match (street mean abs diff 4.7-7.9 at (146,230) vs ~55
   elsewhere). The decoder's placement inside the libjpeg output path is untraced.
3. **Non-Napa courses broken / separate HUD.** `Game::load_course` built the scene,
   dashboard and actor nodes only for `NAPA`; others used the legacy debug road and
   debug-text HUD. Now all courses take the original path (plan 17 Task 2 Step 2's
   condition removal); a scene construction failure logs and falls back to the
   debug road but keeps dashboard/actors. The scene used `<CRS name>.BOB` for the
   horizon; `0x00416310` reads `Images/Horizons/<name>` through pointer table
   `0x00467aa0` by course number: City, Sierra, Penin, Napa, PCH (HIWAY=Sierra,
   MEDLY=Penin, CANYN=PCH). `horizon_name` in course_scene.cpp reads that table.
4. **Napa start grid tyres clipped.** Grid riders sit at `+0x1c & 255 == 0`, exactly
   on sample 50's near edge (y 152.5 logical); the body reaches y 164.5 and the next
   nearer road strip, emitted later, overdrew it along one scanline. Native ordering
   change (labelled, not original-golden-proven): actors in the front half of a
   sample are emitted after the next nearer pair's road surface (road panels and
   marked ground), before its hills/buildings/scenery. Back-half emission unchanged.
5. **Right HUD meter showed the road.** `0x00446c30` draws the third meter (layout
   entry 50, type 1 at 456,424) whenever the full dashboard is up (mode 0), empty
   without a target; native drew it only with a target, leaving the artwork hole.
   Also from `0x0044ca50` disassembly (0x44ca7b..0x44cbe3): vertical meters blit
   the colour column {11,0,62,31} first, then black {11,32,62,63-v} from the top;
   native had the order reversed (meter always full). Meter blits use flags 0x10
   (no colour key), dashboard 0x11; meters are now opaque.
6. **Videos small/letterboxed.** Owner direction (second pass): videos play
   "fullscreen", not at a fixed scale. `VideoPlayer::draw` now disables the
   logical presentation and fits the clip to the whole render output keeping its
   aspect (`VideoPlayer::fit`), then restores the presentation. For reference, the
   original MCI_PUT (`0x00402820`) used Large (VideoMode 2) = 2x at (0, 240-h),
   showroom (flag 1) = 2x at (0, 20), Small = 1x at (160, 240-h/2) in a 640x480
   window; that fixed-scale rule was replaced at the owner's request.

### Second pass (October 12, 2026)

7. **Dashboard distance (constant 25.5).** The REVERSE_ENGINEERING.md note
   "Dashboard distance readout (layout entry 30)" conflates two values and should be
   corrected by the integrator: layout entry 30 prints `DAT_0049e115` with `%02d%c%d`,
   set by `0x00446a40` from player bike `+0x44` (the formula native already had).
   `0x00438450` copies `+0x1c..+0x2c` into `+0x44..+0x54` each frame for scheduler
   objects in the draw window (player always included), so `+0x44` is the route
   position: readout = ((pos>>8)-50)/33 tenths of a mile (metric ((pos>>5)-400)/165),
   0.0 on the grid (sample 50), wraps as a byte. `DAT_004b8a34` (`0x00438550`, signed
   nearest-opponent gap) is a different readout, drawn only with a target name at
   layout entry 10 with an ahead/behind string; still unported. `Hud::state` now
   reads `+0x1c`; hud-test checks 12 for sample 50+33*12.
8. **Autopilot (`--autopilot`, native demo driver, not original AI).** Root cause of
   "crashes a lot": it kept steering while crashed, and the crash walk (0x0040a600
   state 38) only converges with no input, so the first wipeout never ended; it also
   aimed at the road centre line at full throttle. Rewritten: returns 0 while crashed;
   chooses among candidate lines (every 62 units plus lane centres/lines, first
   right-hand lane preferred) the first clear of traffic and riders (lateral
   clearance 120 > car contact 94), with a time check for oncoming cars against the
   overtake time; follows a blocking car's speed when no line is clear; speed cap
   200000/bend from the bend ahead (move() push ~ bend x distance); damped steering on
   lateral position predicted 16 ticks ahead from `+0x10c`; road widths from the
   narrowest sample to the aim point; target relative to the aim sample's centre.
   Headless runs print an `end tick ...` summary line.
   Soak (`rr-game --course N --ticks 90000 --autopilot`, density 6, 7 opponents):
   before, each course logged one crash and never remounted; after, all five finish
   first (NAPA 61173 ticks, CITY 81437, HIWAY 62182, CANYN 56937, MEDLY 53743) with
   one crash total (CITY, reason 0, where the road centre jumps at a merge).
   CITY's route samples 5456..5605 have zero road width (L=R=0, shoulder 1): the
   tail past the real finish that plan 17 Task 1 race bounds must trace. The wall
   stand-in crashes any rider above speed 4000 there; the autopilot crawls it.
   Second-pass gate: `cmake --build build-agent-playtest-visual-fixes --parallel`
   pass; ctest 64/64 pass (music now passes too). During the pass another session's
   half-finished race.hpp edit broke the build for a few minutes; nothing of theirs
   was touched.

Also: race logical presentation 640x510 (debug-renderer leftover) is now 640x480,
removing the empty strip under the dashboard.

Evidence (scratch, not committed): before/after screenshots of Napa start, all four
other courses at 1,800 autopilot ticks, BUSTED1 video frame, menu text crops and
frontend dumps (`frontend-test --dump`).

## Checks / remaining work

- `cmake --build build-agent-playtest-visual-fixes --parallel`: pass.
- `ctest --test-dir build-agent-playtest-visual-fixes --output-on-failure`: 63/64.
  The failure is `music` (`MIDS: truncated RIFF`), from another session's untracked
  `src/race_music.cpp`/`tests/music_test.cpp`: it passes when run from the repo
  root and aborts under ctest (working-directory dependent) in both this build and
  `build/`. Not touched here.
- New regressions: hud-test (opponent meter hole opaque without target; vertical
  meter changes with value), frontend-test (loading postcard fills the right side).
- Not run: sanitizers, Android assembly, oracle. No simulation code changed; the
  actor-order change is render-only and consumes no RNG.
- Remaining: CANYN shows a black region past the cliff (terrain type-1 sides,
  plan 17 Task 4); autopilot is not course-aware (frequent wipeouts off Napa);
  dashboard distance readout still constant 25.5 (traced, unported);
  `0x0044cc50` police indicator not drawn; actor/road ordering needs an original
  natural-frame golden (plan 17 Task 3/oracle-scene); postcard/street inset
  placement should be traced in the decoder. Small-video mode omits the "Matte" image.

## Live processes / resources

Claims released at handoff. No live processes or held resources.
