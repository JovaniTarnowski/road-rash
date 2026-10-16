# pch-terrain-20261016

Task: Pacific Highway type-1 mountain/cliff terrain rendering

Baseline: 9a778540c22819c397798daeb9e9909eaa6be167

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-pch-terrain-20261016`
Owner PID: 83509
Resource token: `39bf9e52-c417-48c9-823d-81cc43797bdc`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/course.hpp`
- `/Users/archit/Dev/rr/native-port/src/course.cpp`
- `/Users/archit/Dev/rr/native-port/src/course_scene.cpp`
- `/Users/archit/Dev/rr/native-port/src/course_scene.hpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_panels.cpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_panels.hpp`
- `/Users/archit/Dev/rr/native-port/tests/scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/course_scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/PROGRESS.md`
- `/Users/archit/Dev/rr/native-port/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/pch-terrain-20261016.md`

## Progress / evidence

Symptom: Pacific Highway (CLI `--course 3`, file CANYN) showed flat black
wedges beside the road below 2/3 of the screen height, at 4:3 and Hor+
widescreen alike. Napa/CITY were fine: they have no type-1 sections.

Cause: course section terrain kinds (probe over `read_course`): CANYN section
0 is `NTMR` (mountain, sub-slice type 1); HIWAY, MEDLY and CANYN have 21
mountain sections in total. `CourseScene::draw_nodes` drew only types 0 and 4,
so a mountain side emitted nothing outside the shoulder-less road, and the
horizon's `fb.clear(0)` showed below its 320 rows. Commit cb44f8b is not
involved.

Original evidence (decompiled exports plus disassembly):

- `0x0043bef0` types 1-3: shoulder `0x0041d260` while the pair is visible,
  then `+0x154/+0x160` < 1 → `0x0040d030`, otherwise `0x0040dfd0`.
- `0x00451710` exports per side {width, offset, height}; `0x00454240` makes
  base = outer shoulder edge − width (left) / + width (right), top likewise
  with the offset, and height < 1 → −`+0xc0` (HTPR iterator +4, absolute
  elevation). It also clears pair-B heights for type 1.
- `0x00444630` case 1: base at the road row (+0x70/+0x78), top
  height·scale above it (+0x80/+0x88).
- `0x0040dfd0`: skipped within depth 0x3d; quad far top, near top, near base
  +1 row, far base +1 row (a hill neighbour gives shoulder/first point);
  near edge pushed one column outward when the visibility test passes;
  plateau tiles of 4×`+0x24` while the far top is above the near top
  (`0x00450720` horizontal edges, frame 1 material).
- `0x0040d030`: quad far base, near base (both −0x6000), near top, far top;
  water from `0x00441870` CEL 0x10 (WATER128). Disassembly
  `0x0040d269..0x0040d33c`: the right strip clamps to the view edge unless a
  foot is within 256 columns of it; the left strip is built only when both
  feet are left of column 0 (off-screen). `+0x40` = `0x4b6e70[32]` is a UV
  shift, not a colour.
- `0x0040ccb0`: materials are category 1 frames 0/1 of the selector.
- `0x0041d260`: for types other than 0/4 the shoulder ends at the wall base
  (+0x70, world +0x14c), not +0x60. Without this, HIWAY showed clear colour
  between shoulder and set-back walls.

Original capture: `RR_ORACLE_COURSE=5 RR_ORACLE_DRIVE=2 RR_ORACLE_SEED=1
RR_ORACLE_PIXELS=1 RR_ORACLE_SHOT_EVERY=60 RR_ORACLE_FRAMES=420 oracle/run.sh
pch-terrain-20261016 400`: exit 0, 420 frames, proxy `48f9bdf5…`
(`analysis/oracle/pch-terrain-20261016*`, git-ignored). Shots f0/f120/f240/
f360: mountain wall left with dirt to the road, water right, no index-0
pixel below row 320. Native frames show the same layout (visual comparison
only, not a pixel match).

Changed files: `src/course.hpp/.cpp` (`PathSample::elevation`),
`src/terrain_panels.hpp/.cpp` (`WallPanelInput`, `wall_draw_nodes`),
`src/course_scene.hpp/.cpp` (water texture, wall transform/projection,
type-1 dispatch, shoulder outer edge), `tests/scene_test.cpp` (synthetic
wall/plateau/cliff/water cases), `tests/course_scene_test.cpp` (HIWAY/MEDLY/
CANYN mountain views at 320 and 426: no view over 1% index 0 below the
horizon), `docs/PROGRESS.md`, `docs/VERIFICATION.md`.

## Checks / remaining work

See `docs/VERIFICATION.md` (October 16, mountain terrain). Remaining:

- Type 3 split sides (`0x0040dac0`, `0x0040d420`, `+0xf4/+0xf8`) are not
  produced natively.
- Building neighbour of a mountain pair: the original reads facade fields
  as ground points; native skips that wall quad.
- Node flags 0x20000/0x40000 set by the wall builders are not modelled.
- No pixel or display-list comparison with the capture's scene dumps
  (`-scene*.raw`); plan 17 Task step "terrain type 1 sides" can cite this
  work but its golden acceptance is still open.
- Scenery ground height on type 1 (`0x00439b10` ramps) still returns 0.

No live processes or held resources besides the file claim (released at
commit).
