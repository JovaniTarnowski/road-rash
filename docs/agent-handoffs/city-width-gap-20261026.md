# city-width-gap-20261026

Task: Fix City road width transition triangle identified at recording 38 seconds

Baseline: 9db946ffc763ee709af7001c1eea07b1c0cbc7d9

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-city-width-gap-20261026`
Owner PID: 95735
Resource token: `9d0da036-3746-42ec-92d4-64aae33f1079`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/course_scene.cpp`
- `/Users/archit/Dev/rr/native-port/tests/course_scene_test.cpp`

## Reproduction and root cause

The supplied screenshot clearly shows a triangular background leak through the
right lane near 00:38 in the **Desktop** recording (247.286667 seconds).
The earlier **Documents** recording is only 40.41 seconds and is a different
segment. This explains why the previous session did not locate this frame.

Reproduced with imported CITY course data, route A, camera sample 156.5,
height 150 and lateral 0. The widening at sample 159 changes left lane Q8
512 -> 533 (right remains 512). Rounding the left lane count upward changes
2 -> 3. `CourseScene` used sample 159's rounded origin for the far end of
sample 158's panels, but sample 158's origin/count at the near end. Every
far endpoint shifted by one 250-unit lane, producing the displaced yellow
centre line and exposed-background triangle in the supplied screenshot.

Original `0x00444630` writes the previous record's far lane origin (+0x30)
using the **previous** sub-slice's +0xd8 at the new depth/scale whenever
origins differ. Verified against exported pseudocode and assembly
`0x00444c57–0x00444ca3`: dword loads from each sub-slice +0xd8; compare;
previous origin plus new world x minus camera x; signed multiply by new
scale; add screen centre; store previous record +0x30. `0x0043c170` then
walks that strip's own lane count at both ends. Native now projects the
same near-strip lane origin at both depths, including the sub-slice centre
correction (`road.offset - old.offset`) and existing 32-bit wrapping.
No change to the simulation, randomness, assets or road-panel component rules.

## Tests and visual evidence

- Added an imported-scene pixel coverage regression over the right-lane leak
  region, at logical widths 320 and 426. It requires pavement palette values
  rather than background scenery. Before the fix, `course_scene-test` exits 1:
  `City sample 159 width transition exposes scenery inside road`.
- With the fix the regression and the all-course/all-sample scene construction
  sweep pass. The same imported City view was inspected before/after: the
  triangle closes and the displaced centre stripe is corrected.
- Local evidence (ignored, disc-derived): `analysis/city-gap-20261026/`:
  `frame38.png` from the Desktop recording, `city159.png` before,
  `city159-fixed.png` after, transition enumeration and rendering probe.
- Native Release configure/full build succeeds in the owned build directory.
  Full CTest running: `/tmp/rr-city-ctest.log`. Baseline regression failure:
  `/tmp/rr-city-baseline.log`; fixed sweep: `/tmp/rr-city-fixed.log`.
- No C++ LSP channel in this client; Homebrew clangd with the owned database,
  changed scene lines: zero errors (`/tmp/rr-city-clangd.log`).
- No new original pixel capture or full City-race parity claim; this is the
  exact user-reported native scene reproduction plus original assembly trace.

## Ownership / resources

Changed only `src/course_scene.cpp`, `tests/course_scene_test.cpp` and docs.
OMP's in-progress `race.cpp`, `race.hpp`, `oracle_sim_test.cpp` edits preserved.
No Wine prefix, Ghidra, device, or capture resource held. Rendering probes read
original local disc data only. Prior isolated Release checkout/build from
`user-road-combat-20261025.md` is available for exact committed-source gates.
