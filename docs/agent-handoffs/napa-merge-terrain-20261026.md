# napa-merge-terrain-20261026

Task: Fix split branch terrain overdraw in reported Napa merge window

Baseline: a3c66abfb4e43d8fe8edc4a458daffc4809e3cdb

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-napa-merge-terrain-20261026`
Owner PID: 95735
Resource token: `6f885b0d-2cf2-45e8-9998-b4b10ee62b99`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/course_scene.cpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_panels.cpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_panels.hpp`
- `/Users/archit/Dev/rr/native-port/tests/scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/course_scene_test.cpp`

## Reproduction

User's four screenshots show grass bands crossing the roadway, truncating the
road and covering most of the sky while playing Napa on web, HUD 2.6–2.8 miles.
Imported NAPA route A reproduces the same failures around samples 940, 945,
970 and 975 (the HUD uses `(sample-50)/33` tenths). Samples 919–999 are a
merge window: the neighboring road is restored before the branches meet.
No browser cache assumption was needed; the current native renderer reproduces it.

An isolated-node raster probe at sample 945 identified a terrain quad with
points approximately (-0.375,139.7), (13326.6,-67.5), (16384,-257.5),
(-0.1875,305.4), Q16 logical view coordinates. It paints the apparent curtain.
The inward hill profile had incorrectly been extended to the viewport boundary
as if it were an exterior roadside profile.

## Original evidence and implementation

- `0x00443cd0` draws exterior sides separately and dispatches the inner gap
  through `0x0043be80` → `0x0041d500` for hills. Inner hills use bounded band
  counts; only exterior hills use the extended fourth band.
- `0x00454890`'s approaching-merge branch joins the two world-space inward
  profiles. It walks their three hill points, records each side's band limit,
  finds a common ridge through `0x00454c30`, and collapses surplus points to
  that ridge. Separated profiles retain three bands and request a bridge.
- `0x00454c30` uses the midpoint of the preceding X endpoints and a signed
  Q16 slope on the first profile. Its fourth argument is unused. Checked the
  signed compare/setge/setle and idiv sequences in the exported assembly at
  `0x00454a9b`, `0x00454ad2`, and `0x00454c30–0x00454c7f`.
- `0x0041d620` connects the two terminal inner edges with the first road's
  right-side hill-band-2 material. `0x0041d500` takes neighboring band maxima
  to avoid a seam popping between slices.
- Native now joins terrain in per-view local copies before projection for
  type-4/type-4 approaching merge windows, limits inward band emission and
  emits the connecting ridge when needed. Exterior profiles, stored terrain,
  simulation and random stream remain untouched. This ports the reported
  approaching-merge path; departing splits and mixed terrain transitions are
  separate existing fidelity work, not newly claimed complete.

## Regression and verification

- New imported-scene regression checks sky and road coverage at samples
  940/945/970/975 at logical widths 320/426. Against the prior library it
  fails: `Napa merge terrain covers the roadway at 940` (exit 1),
  `/tmp/rr-napa-terrain-baseline.log`.
- Synthetic checks verify joined ridge continuity, preservation of exterior
  hills, band limits and the separated-profile bridge request.
- The original reported curtain/road bands disappear in the native scene
  sweep. Before/after sheets and probes are under ignored local
  `analysis/napa-terrain-20261026/` (disc-derived; never commit images/assets).
- Owned Release build + full CTest: 106/106 pass, zero skips (41.08 s),
  `/tmp/rr-napa-terrain-{config,build,ctest}.log`.
- No C++ LSP channel exposed; Homebrew clangd with owned compile DB checks
  the new terrain helper with zero errors, `/tmp/rr-napa-terrain-clangd.log`.
- No new original-runtime capture or device verification. Static original
  trace and native scene reproduction establish this fix; full split/merge
  original pixel parity remains distinct.

## Ownership and live resources

Only the five owned renderer/test files and this handoff changed. OMP's
combat work in race.cpp/race_schedule.cpp/field_replay.hpp, shared verification
and scenario docs, and its napa-fight handoff are preserved and excluded from
staging. No Wine/Ghidra/capture locks held. Private build above; no live server.
Use an isolated committed-source checkout for final native/web builds.

## Final committed-source gates

- Source `66fcb52`, isolated tracked-clean checkout
  `/tmp/rr-user-road-combat-release/native-port`, excluding OMP's unfinished
  combat changes. Full Release build and CTest `-j 4`: **106/106 pass**, zero
  skips (27.16 s), `/tmp/rr-napa-terrain-clean-{build,ctest}.log`.
- Emscripten browser Release build succeeds and validates nine asset-free
  code/metadata files: `/tmp/rr-napa-terrain-clean-web.log`, bundle at
  `build-agent-user-road-combat-clean-web/dist`.
- Native build identity: commit `66fcb52`, `dirty=false`, source SHA-256
  `75419a54128436b09bf5bc1ed21d3e25ecb48e8485fe55921db4e6bf7f7d6c87`.
- Changed scene setup clangd check also zero errors. New captured live-original
  merge goldens, sanitizers and device validation were not run. Existing original
  replay gates pass; no golden was modified.
- Source claim released after completion. No background local servers/captures.
