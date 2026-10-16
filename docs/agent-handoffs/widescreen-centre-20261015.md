# Widescreen centring fix 20261015 (plan 21 Task 8 follow-up)

Baseline: d6ebde0 "docs: camera order, outcome selection, jukebox fade handoffs 20261016j".
Tracked tree was clean at start (only pre-existing untracked analysis/fuzz/handoff files and
`tests/dash_bench.cpp`, which belongs to another worker and was left unchanged). Committed together with this handoff.

## Symptom (owner screenshots, City, widescreen=1 16:10, render_scale 2)
Scenery stood in the traffic lane (dumpster, barrel, trees, lamp posts), the sidewalk/building
strips tore, a black wedge appeared at the lower left, and building facades broke at strip seams.
The 4:3 view looked correct. Interpolation was off (`enh_interpolate=0`), so the camera blend was not involved.

## Root cause
Hor+ used three different centre columns:
- `RoadSlice::screen_x` (road surface, panels, first quad of each terrain strip) and `project_hills`
  (called without a width) centred at the fixed 4:3 column 160.
- Billboards, actors, traffic, building transitions and marked ground centred at `logical_width/2`.
- `HillPanelInput`, `ShoulderPanelInput`, `BuildingShoulderInput` and `BuildingPanelInput` were never given
  `logical_width` in `CourseScene::draw_nodes`, so their subdivided quads and edge culls used the 320 default.
Then `CourseCanvas::draw` (race_caches.hpp) added the side margin to every node. That put the
road surface in the right place but everything built at `logical_width/2` one margin too far right (53 logical px at 16:9,
32 at 16:10). Subdivided strips also kinked between their road-derived first quad and the later quads.

## Change
- `src/road.hpp`, `src/road.cpp`: `RoadSlice::centre_q16` (default 160 Q16). `project_road(..., logical_width=320)`
  sets it to `(logical_width&~1)*32768`, the same formula the builders use, and rejects widths outside 320..4096.
- `src/course_scene.cpp`: `project_hills` gets `logical_width`; all four panel inputs get `logical_width`.
- `src/race_screen.cpp`: settings (widescreen) sync before projection; road projected at `g_.logical_width()`.
- `src/race_caches.hpp`: removed the per-node margin shift (the margin now only places the 4:3 dashboard).
- `tests/race_cache_test.cpp`: projects slices at the game's logical width per wide/narrow row.
- Tests: `tests/road_test.cpp` (synthetic: wide slice centre/shift, hill points and billboard anchor share
  the road centre, narrow width rejected). `tests/course_scene_test.cpp` (disc: CITY and NAPA, two cameras: every
  node fully inside the 4:3 view must reappear at 426 shifted by exactly +53 columns).

At logical width 320 every path is unchanged: centre 160, margin 0.

## Evidence
- Build dir `build-agent-widescreen-centre-20261015` (Debug): configure/build exit 0.
- New course_scene check with the panel-width assignments removed: `CITY widescreen: 65312 interior nodes, 8536 not at +53`
  (fails). With the fix: CITY 65312/0, NAPA 62798/0 (passes).
- Live frames (`rr-game --course 1 --autopilot --ticks N --screenshot`, `RR_PREF_OVERRIDE` copies of the owner's
  settings with widescreen 0/1, scratch only): with the fix, scenery sits on the sidewalks and strips are continuous at
  ticks 500/900/1300. The 4:3 BMP at ticks 500 and 900 is byte-identical (`cmp`) to the pre-fix binary.
- First gate after the fix: `ctest --test-dir build-agent-widescreen-centre-20261015 --output-on-failure -j8` → exit 8,
  98/99. race-cache failed at row=2 (scale 1, 16:9, indexed) because its wide self-baselines encoded the misaligned scene.

## Baseline re-record (owner-approved 2026-10-15)
The owner approved re-recording only the wide entries (indices 1, 3, 5 = scale slot*2 + wide) of `original_scene`
and `rgb_baseline` in `tests/race_cache_test.cpp`. To collect the values, the two baseline checks were made
print-only for one run (file restored from a copy afterwards; the diff shows only the intended edits):

| idx | scale | original_scene old → new | rgb_baseline old → new |
|---|---|---|---|
| 1 | 1, wide | 18290316952315886966 → 15653804743185687758 | 12480883420206857557 → 4304228047002938247 |
| 3 | 2, wide | 4509599773851679003 → 8935959284729329489 | 1477636935681336033 → 12848431387687779404 |
| 5 | 4, wide | 2853708629718616441 → 11834733791893956520 | 13490494933351736534 → 1873835806207674357 |

The 4:3 `original_scene` entries (0, 2, 4) all still match. **Pre-existing, not touched:** 4:3 `rgb_baseline`
entries 2 (scale 2) and 4 (scale 4) do not match the current render: got 11345352425216510781 and
9967728013922361189, want 8749086642151112002 and 637450489077128212. Gate 20261016h logged the same row=5 hash
11345352425216510781 before this change, and every 4:3 path is unchanged by construction (centre 160, margin 0),
so the cause lies elsewhere. Not re-blessed.

Gate rerun: `cmake --build build-agent-widescreen-centre-20261015 --parallel` exit 0; `ctest ... -j8` → exit 8,
98/99 passed. race-cache now passes rows 0–4, including every wide row's scene, dashboard and horizon checks
(rows 2 and 3). It then fails at row=5 (scale 2, 4:3, RGB) on the pre-existing entry above:
`RGB canvas output changed from pre-optimization baseline`. Later rows (6–11) do not run past that throw; their wide
values come from the probe run. All other 98 tests pass, including road-projection and course_scene with the new checks.

## Remaining
- race-cache stays red on the pre-existing 4:3 RGB baseline mismatch (entries 2 and 4). It needs its own
  investigation: find which change since fdcd4aa moved the 4:3 RGB canvas, then fix it or get approval to re-record.
- `tests/dash_bench.cpp` (untracked, other worker) still projects at width 320; pass `g.logical_width()` when it lands.
- Not checked on Android hardware. Integrator: merge into PROGRESS.md/VERIFICATION.md and plan 21 Task 8 status.

No live processes or held resources.
