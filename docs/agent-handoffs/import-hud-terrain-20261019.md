# Handoff: import screen redesign + HUD meter + Napa terrain stripes (2026-10-19)

## Task
User screenshots (3): import screen restyle request; terrain banding on Napa
opening straight for a few seconds; empty bottom-right HUD meter question.

## Baseline
- `git log --oneline -1`: f06a0d4 (plus OMP's dirty tree, preserved untouched).
- Dirty files at start (not mine, left alone): CMakeLists.txt,
  oracle/aweman_proxy.c, src/course_scene.cpp, police.cpp, race.cpp/hpp,
  race_pedestrians.cpp, race_schedule.cpp, sound.cpp/hpp, terrain.cpp,
  track_ring.cpp, several tests. The terrain.cpp/track_ring.cpp diffs are
  OMP's TEMP `SETUPDRAW` tracing only.

## My change (only file edited)
- `src/import_ui.cpp`: arcade Road Rash restyle of `run_import_screen`.
  Input behavior identical: same key/mouse hit rects (ISO {80,200,220,50},
  folder {340,200,220,50}), same dialog/worker/progress/smoke/screenshot flow.
  Paint only: checkered-flag top/bottom strips, red/yellow stripes, animated
  leader dashes, big yellow-on-red `ROAD RASH` title (4x debug text), yellow
  `NATIVE` plate, yellow primary ISO button with blinking border, dark folder
  button, red progress bar with yellow tip, blinking hint line, footer tag.
  Colors via draw color before `SDL_RenderDebugText` (it uses current color).
  Title math accounts for `SDL_SetRenderScale` scaling coordinates.

## Evidence / checks (private build dir `build-import`, shared `build/` untouched)
- `cmake --build build-import --target rr-game`: pass.
- Full `cmake --build build-import --parallel`: pass.
- `ctest -R "hud-test|importer|import-job|display"`: pass (importer, display).
- `ctest -R hud`: pass.
- `ctest -R "course_scene|terrain|track-ring|hill"`: 5 pass;
  `oracle-terrain` / `oracle-terrain-medly` skipped (no disc in sandbox).
- Offscreen smoke screenshot NOT obtained: sandbox has no display
  (`The video driver did not add any displays`; offscreen+software also fails
  on GL init). Needs `./run-macos.sh` visual check by owner.

## HUD meter answer (no code change)
- Intended original behavior. `hud.cpp:261` draws the third (opponent) bar at
  layout 50 empty (`0`) whenever `state.target` is empty; `hud.hpp:23-31` and
  `0x00446b80` notes: name+meter+gap all clear with no target. Screen 3 shows
  no target name (screen 2 shows `Mike` + `0.013` gap), so the hole renders the
  empty bar. Test `tests/hud_test.cpp:326-330` pins this: with no target the
  opponent-meter rect (456,424,51x31) must be painted, never transparent.

## Napa terrain stripes investigation (no code change; needs owner capture)
- Stripes are horizontal full-width bands over the fields, road clean, early
  race (02.7 distance), clears after a few seconds. That reads as
  camera-position-dependent (opening straight looking at far hill slices), not
  time-dependent: fade-in is only 63x14ms and countdown is already over.
- Suspects, both in code read (not proven):
  1. Far-depth hill band path in `src/terrain_panels.cpp:69-86`
     (`hill_draw_nodes`): `near.depth<1234 && band<3` selects subdivision +
     `material(band, length...)` texture by projected length; at far depth tiny
     lengths can flip band/mip choice slice-to-slice, stacking as horizontal
     stripes until the camera advances.
  2. Precomputed-vs-live terrain height zeroing: `src/course_scene.cpp:77`
     zeroes `t.points[*][1]` for type-4 slices with marking flags, and
     `apply_ring_terrain` (line 129-136) re-applies live values but keeps the
     precomputed type; any flag mismatch near the start line flattens hill
     bands into bright strips for the opening seconds.
- Next step (needs a running game + disc, unavailable in sandbox): capture
  `--course 0` frames over the first ~10s and diff against the research viewer
  / oracle scene capture at the same ticks; log which band/material each striped
  row's DrawNode carries. Do NOT retune hill texturing on this hypothesis alone
  (fidelity rules: measured mismatch first).

## Remaining / for integrator
- Visual sign-off of the import screen via `./run-macos.sh` (font scale,
  colors, blink rate). Consider a follow-up screenshot in docs.
- No new maintained test: the restyle is renderer paint with no headless
  harness (`display` test covers logical presentation only); behavior paths are
  covered by existing importer/display tests. Say so if asked.
- Left uncommitted per session policy (no commit request); OMP tree untouched.
