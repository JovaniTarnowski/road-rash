# dash-bench 20261015 — scratch `dash_bench` updated for Hor+ centring

Task: bring the untracked scratch benchmark `tests/dash_bench.cpp` up to date
with `cb44f8b` (road slices centre on `logical_width/2`; `CourseCanvas::draw`
no longer shifts nodes by the margin), harden its SDL setup, and re-measure.

Baseline: HEAD `166de8f` (contains `cb44f8b`). Shared tree
`/Users/archit/Dev/rr/native-port`, no branch switch, no stash. Committed
as a tracked bench at the owner's request (this file + `tests/dash_bench.cpp`).

## Owned / changed files

- `tests/dash_bench.cpp` (was untracked scratch; now tracked, header updated).
- `docs/agent-handoffs/dash-bench-20261015.md` (this file).
- `CMakeLists.txt`: **not changed**. The `RR_BENCH_DASH` block (line ~322)
  already works: `RR_SOURCE_DIR` is now a global `add_compile_definitions`
  (line ~110), so the earlier "RR_SOURCE_DIR undefined" Android failure
  (VERIFICATION.md ~line 1015) is fixed by the option being OFF by default
  and by the global definition when it is ON.
- Not touched: `tests/race_cache_test.cpp` (modified in the tree by another
  session investigating the 4:3 RGB baseline).

## What changed in `tests/dash_bench.cpp`

- `rr::project_road(..., g.depth, g.logical_width())`, matching
  `RaceScreen` (`src/race_screen.cpp:239`). `g.widescreen` is assigned from
  settings before the projection, so slices use 320 (4:3) or 384 (16:9).
  Before the change the bench always projected at 320, which mis-centred
  the road on a widescreen canvas after `cb44f8b`.
- `SDL_Init`, `SDL_CreateWindow`, `SDL_CreateRenderer` are checked; failure
  prints `dash_bench: <call> failed: <SDL_GetError()>` and exits 1. A missing
  course scene also exits 1. Asset/load exceptions still terminate non-zero
  (uncaught). `CourseCanvas::draw` already throws on its own SDL failures.
- Output now prints `draw_ms` to 2 decimals and the `logical_width` used.

## Commands and exit codes

Hardware: Apple M3 Pro, macOS 27; AppleClang Release.

```sh
cmake -S . -B build-agent-dash-bench-20261015 -DCMAKE_BUILD_TYPE=Release -DRR_BENCH_DASH=ON   # exit 0
cmake --build build-agent-dash-bench-20261015 --target dash_bench-test --parallel 8          # exit 0, no warnings
```

Settings: two scratch copies of the owner's
`~/Library/Application Support/RoadRashNative/RoadRash/settings.txt`
(read only) with only `widescreen=` changed, in the session scratchpad
(`ws0/`, `ws1/`). Owner settings have `render_scale=2`, all `enh_*`=0,
`texture_detail=1`, so these are Original-render numbers.

```sh
RR_PREF_OVERRIDE=<scratch>/ws0 ./build-agent-dash-bench-20261015/dash_bench-test   # 3 runs, exit 0 each
RR_PREF_OVERRIDE=<scratch>/ws1 ./build-agent-dash-bench-20261015/dash_bench-test   # 3 runs, exit 0 each
SDL_VIDEO_DRIVER=bogus ./build-agent-dash-bench-20261015/dash_bench-test           # exit 1 (failure path check)
```

## Results (Release, s=2, 8 threads, Napa 900 autopilot ticks, 10-frame mean)

| widescreen | logical_width | run 1 | run 2 | run 3 |
|---|---|---|---|---|
| 0 (4:3)  | 320 | 6.98 | 10.09 | 6.77 ms |
| 1 (16:9) | 384 | 7.77 | 7.57  | 7.40 ms |

Run 2 at 4:3 (10.09) is an outlier; typical 4:3 ~6.9 ms, 16:9 ~7.5 ms.

## Comparison with earlier numbers

- VERIFICATION.md widescreen review item 2 (Release, Original 2x 16:9 after
  compose-once fix): 6.7 ms. Now ~7.4–7.8 ms at 16:9 with the corrected
  projection: about +10%, within the ~10% run noise seen in
  `bench-release-20261016f`, and HEAD has since gained `1b79351` (HUD pen
  width) and `cb44f8b`. Not treated as a regression without a
  same-HEAD p50/p95 harness.
- `bench-20261016d` Debug 116.1 ms (16:9) is Debug, not comparable.
- Whole-frame `race_cache-test --benchmark` Release (bench-release-20261016f)
  row 4 (2x 4:3 Original) p50 7.07 ms vs this bench's ~6.9 ms at 4:3:
  consistent.
- Enhanced (61.2 → 34.1 ms) was not re-measured: owner settings have all
  enhancements off.

## Remaining

- No p50/p95 or per-frame distribution; 10-frame mean only; only s=2.
- Enhanced settings run not done.
- No processes left running; held resources: build dir
  `build-agent-dash-bench-20261015` only.
