# Road and World Rendering Implementation Plan

> **Status (October 11, 2026 review):** Napa scene construction is done and verified piecewise. Remaining Napa items (split stitching, marked terrain, building marking transitions, random order) are in [plan 11](11-napa-race-completion.md) Task 3; the other courses are in [plan 12](12-all-courses.md). This plan is kept as background for addresses and findings; follow the newer plans for remaining work.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. This plan requires plan 01 to be complete: git, `rr::Framebuffer`, the golden reader and the oracle.

**Goal:** Replace the viewer's placeholder flat-shaded road with a pixel-accurate port of the original 3D world renderer (road surface and markings, shoulders, terrain walls/hills/buildings, sky and horizon), drawn into an 8-bit framebuffer and matched against the original's own frames.

**Architecture:** The original frame (`0x004439f0`) runs in three steps:
1. **Build:** `0x00443a00` projects road slices (already ported as `rr::project_road`) and builds a linked display list of drawable nodes.
2. **Draw:** `0x00450420` walks the list (`0x0040ff40` → `0x0040ffc0` per node, then `0x0044d300`) and rasterises into the locked 8-bit back surface.
3. **Present:** `0x0044c780` unlocks and flips.

The native port mirrors this split:
- `src/scene.*` builds a list of draw nodes from projected slices.
- `src/raster.*` fills spans into `rr::Framebuffer` exactly as the original rasteriser does.
- The platform layer uploads the framebuffer with the palette.

**Tech Stack:** C++17, SDL3 (presentation only), CTest, oracle from plan 01.

**Spec:** [00-INDEX.md](00-INDEX.md), [../REVERSE_ENGINEERING.md](../REVERSE_ENGINEERING.md) sections "Road projection recovered", "Splits and merges recovered", "Course geometry recovered" (row-chunk table, `DLSR`, terrain types).

## Global Constraints

- The 3D view space is 320×240 Q16 with the horizon on row 90 (`0x5a0000`). The presentation resolution follows the original's high/low resolution mode (determined in Task 2).
- Colour is palette indices only. No RGB arithmetic anywhere in the renderer.
- All ported arithmetic keeps the original integer semantics (truncating `/`, arithmetic `>>`, 32-bit wrap).
- Every task adds oracle keys and golden comparisons. A task is not done until its golden test passes, or every deviation is documented with frame numbers and cause.
- No original bitmaps or colour tables are embedded in source. Tables are read from `ROADRASH.EXE` at runtime, as `read_depth_table` does, each with an address constant and a sanity check.

## Review Focus

1. **Road segments crossing the screen edge or behind the camera** (near slices project to x far outside 0..319 and rows > 240): the rasteriser must clip without overflow. Task 3 tests coordinates of ±2³⁰ Q16.
2. **Hill crests** where farther slices project lower than nearer ones: the occlusion row must hide them exactly as the original does. Task 6's golden scenario includes a crest (NAPA sample 300).
3. **Dual roads at different heights** (a split with up to 3,369-unit height gaps): the draw order between halves must match the original. Task 6 includes NAPA samples 700–1000.
4. **Low-resolution mode** (the original halves surfaces): if the game offers it, both modes must render. Task 2 determines this and adds a golden for each mode found.
5. **Palette-index-exact output on Android** (the texture upload path): the framebuffer-to-RGBA conversion must be the only colour step. Task 8 compares a macOS and an Android capture of the same frame.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/exe_tables.hpp/.cpp` | Read fixed-address tables from `ROADRASH.EXE` (generalises `read_depth_table`) |
| `src/raster.hpp/.cpp` | Span/polygon fill into `rr::Framebuffer`, exact port of the original fillers |
| `src/terrain.hpp/.cpp` | Decode the terrain chunk (`RBLD`/`RMTN`/`RDWD`/`RHIL`) and the other per-section chunks needed for drawing |
| `src/scene.hpp/.cpp` | Build draw nodes from projected slices (port of the display-list build in `0x00443d70`/`0x00444630` cases 0–4) |
| `src/world_renderer.hpp/.cpp` | Draw sky/horizon + scene into a framebuffer (port of `0x0040ff40` path) |
| `tests/raster_test.cpp`, `tests/terrain_test.cpp`, `tests/oracle_frame_test.cpp` | Tests |
| `oracle/aweman_proxy.c` | Extended dumps (raw pixels, sub-slice fields) |

---

### Task 1: Oracle dumps of raw frames and sub-slice fields

**Files:**
- Modify: `oracle/aweman_proxy.c`
- Create: `tools/golden_frame_to_png.py`

**Interfaces:**
- Consumes: `rr_patch_call`, `rr_log`, `dump_frame` (plan 01)
- Produces:
  - Per-frame key `pixels.file`: frame index; the raw frame is written as `<scenario>-f<frame>.raw`, width×height bytes of palette indices, next to the TSV.
  - Keys `pixels.width`, `pixels.height`, `pixels.pitch`.
  - For each dumped road struct: `slice.<s>.road.<r>.sub.<off>` for every dword offset `off` in `0x100..0x164` of the sub-slice (the `+0x0c` pointer). Plan 02 needs these (terrain fields `+0x138…+0x160`, `+0xf0…+0x118`, flag bytes `+0x148/9`).
  - `palette.<i>` (packed `0xRRGGBB`) whenever the palette changes (`0x0041df60` / `0x0041e710` path).

- [ ] **Step 1: Find the locked back-buffer pointer**

The draw writes into the surface locked from `DAT_00475410` (vtable `+0x64` = `Lock`; `+0x80` = `Unlock`, as seen in `0x0044c780`). Run:
```sh
grep -nE "\+ 0x64\)\)\(DAT_00475410|DAT_00475410 \+ 0x64" analysis/decompiled/*.c | head
```
Open each hit and identify where the `DDSURFACEDESC` from `Lock` is stored (`lpSurface` is at offset `0x24` of a `DDSURFACEDESC`, `lPitch` at `0x10`). Record the global holding the pixel pointer and pitch in `docs/REVERSE_ENGINEERING.md`.

- [ ] **Step 2: Hook the draw call and dump pixels**

The frame routine is `4439f0: call 0x443a00; 4439f5: call 0x450420; 4439fa: jmp 0x44c780`. Patch `0x004439f5` (bytes `e8 26 ca 00 00`) to a hook that calls `0x00450420` and then, when `RR_ORACLE_PIXELS=1`, writes the locked surface rows (width × height from the globals found in Step 1) to `<out-dir>\<scenario>-f<frame>.raw` and logs `pixels.*`. Use the pointer/pitch globals from Step 1. Do not call `Lock` yourself. Include the sub-slice dword dump from the Interfaces list in `dump_frame`.

- [ ] **Step 3: Converter for viewing**

`tools/golden_frame_to_png.py RAW WIDTH HEIGHT PALETTE.RAW OUT.png` maps indices through `PALETTE.RAW` (entry 0 forced black) and writes a PNG using Pillow.

- [ ] **Step 4: Capture reference scenarios**

Record an input file once (plan 01 Task 7), then capture with the fixed seed. Each scenario gets a replay input file, stored as `analysis/oracle/<name>.input`.

| Name | Content |
| --- | --- |
| `napa-crest` | Napa race start through sample ≥ 400 (crest at 300) |
| `napa-split` | Ride Napa through the split at sample 700 and the merge at 1000 on branch A |
| `city-building` | City, 60 s (building terrain) |
| `canyon-walls` | Canyon, 60 s (mountain terrain) |
| `hiway-hills` | Highway, 60 s (hill terrain) |

Use `RR_ORACLE_PIXELS=1 RR_ORACLE_SEED=12345 RR_ORACLE_INPUT=... oracle/run.sh <name>`.

Expected: TSVs ending in `end`, plus one `.raw` per frame. Spot-check a frame with the converter and look at it.

- [ ] **Step 5: Commit**

```bash
git add oracle tools/golden_frame_to_png.py && git commit -m "oracle: raw 8-bit frame and sub-slice dumps

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Resolution mode and screen mapping

Before rasterising, establish how the original maps the 320×240 Q16 view onto its surface.

**Files:**
- Modify: `docs/REVERSE_ENGINEERING.md`
- Create: `src/view.hpp`

**Interfaces:**
- Implemented: `rr::ViewMode` / `rr::view_mode(bool high, bool inset=false)` including surface/display dimensions, origin and signed Q16 conversion. High maps `(q16 >> 15) & 0xfffffffe`, low maps `q16 >> 16`.
- Evidence: `0x00410160`, `0x0040ffc0`, `0x00448870`, `0x004457f0`; 24 original helper comparisons pass. Scene integration and actual low/inset frame comparisons remain open.

- [ ] **Step 1: Measure**

From a `napa-crest` frame:
- Take `pixels.width/height`.
- For five slices, compare the dumped `screen_y >> 16` with the rows where road pixels actually start in the `.raw` frame.

Also read `0x0044c5f0` (low-res halving) and the resolution option in the menus (`0x00448200` startup) to see whether a low-resolution mode exists.

- [ ] **Step 2: Write `src/view.hpp` with the measured constants and an explanation comment**

Cite the frames and addresses used. If a low-resolution mode exists, capture `napa-crest-lo` as well.

- [ ] **Step 3: Commit**

```bash
git add src/view.hpp docs && git commit -m "render: document original view-to-surface mapping

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Rasteriser port

Corrected by disassembly and original frame capture (October 2): polygon fill is `0x0044d620`, with spans at `0x0044db30`. `0x0044d300` traverses the display list and dispatches draws. `0x0040ffc0` converts Q16 coordinates using the active coordinate shift; it does not dispatch node types. The previously listed `0x0044b630`/`0x0044ba40`/`0x0044bd40`/`0x0044c0e0` prepare projected scenery/sprites.

**Files:**
- Create: `src/raster.hpp`, `src/raster.cpp`, `tests/raster_test.cpp`
- Modify: `CMakeLists.txt`

**Current interfaces (corrected from original disassembly):**
- `RasterPoint` contains integer surface x/y after `0x0040ffc0` coordinate conversion.
- `RasterQuad` contains four normalized points and one palette index.
- `RasterClip` has left/top/right/bottom bounds; span right is exclusive, bottom row inclusive.
- `fill_quad(Framebuffer&, const RasterQuad&, RasterClip)` ports the edge/span stage after `0x0044ce60` normalization.

**Verified implementation (October 2):**
- Q16 signed edge slopes use the original split quotient/remainder calculation and 32-bit shifts/add wrap.
- Scan rows include the bottom endpoint. An edge ending below its next neighboring vertex drops that endpoint, so another edge can replace it without double participation.
- Spans floor signed Q16 x values and write left-inclusive/right-exclusive palette bytes. A fully horizontal polygon draws one clipped span.
- Duplicate neighboring points are skipped when selecting endpoint neighbors; this reproduces the original degenerate-edge tables without embedding those tables.
- Synthetic cases cover rectangles, diamonds, clipped negative coordinates, horizontal spans, duplicate-point triangles and rotations.
- `RR_ORACLE_RASTER=2 oracle/run.sh raster-fixtures 3` invokes the original pure filler on 64 synthetic inputs into an isolated 640×480 memory buffer during oracle setup. This avoids waiting for cinematics; it does not capture natural race frames. Original normalization runs before vertices are recorded.
- `oracle-raster` compares every byte of every resulting image: **64 fixtures, zero mismatches**, including fractional trapezoids, rotations/degeneracies and ±16,384 pixel coordinates.

**Still open:**
- Normalization is now ported: `normalize_quad` matches 2,048 original calls in flags 0/8/4, and `draw_flat_quad` matches all 64 indexed fixture images from pre-normalization inputs. Pathological rejected-polygon raster behavior remains open.
- Indexed texture spans, edge interpolation, normalized quad scan conversion and mip selection are now implemented in `src/texture.cpp`: 1,008 original spans and 64 textured images match. Hook both primitives into original scene/resource construction; sprite/display-list integration remains open.
- Full-frame original comparison, both view modes, terrain/occlusion and actual Android output remain open.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "render: port original polygon fill

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Terrain chunk decoding

The terrain chunk at section row `+0x10` is tagged on disk `DLBR` (`RBLD`, building), `NTMR` (`RMTN`, mountain) or `LIHR` (`RHIL`, hill); `RDWD`/`RDWE` also appear in code. The traversal iterator is at half `+0xb4`:
- init `0x004510c0`; for `RHIL`, init `0x00450fc0`
- step `0x004516e0` → `0x00451220` (`RMTN`/`RDWD`) or `0x004515a0` (`RHIL`)
- per-slice export `0x00451910` → `0x00451710` (type 1), `0x004517b0` (type 0), `0x004517e0` (type 4)

**Current implementation (October 2):**
- `src/terrain.hpp/.cpp`: bounded immutable `TerrainDefinition`, stateful `TerrainIterator` with caller-owned random callback, and `TerrainSlice` exported selectors/walls/points/shades.
- `CourseSection::terrain` stores the definition. Random profiles are not cached in `PathSample`: original state and random-call order matter.
- Original init/step/export component comparison: 864 slices, zero mismatches. All 109 disc sections decode and traverse in both directions. Malformed/truncated payload checks pass.
- Signed negative/inverted hill endpoints and unsorted wall boundaries in the disc are preserved, following original one-boundary-per-call behavior.

**Still open:** natural course-cache/shared random-stream initialization, fractional step original comparisons, `0x00454240` split transformations and full race scenario comparisons (`city-building`, `canyon-walls`, `hiway-hills`). Component equality does not complete scene integration.

---

### Task 5: Scene build (projection cases 0–4 and visibility)

`0x00444630` computes per-type screen fields: case 0 (building/shoulder heights), cases 1–3 (wall profiles), case 4 (hills via `0x0041d6d0`). `0x00443d70`'s tail computes visibility (`+0x100`) and the occlusion row `0x004c10c8`. Edge stitching between slices uses `0x00443940`.

**Files:**
- Create: `src/scene.hpp`, `src/scene.cpp`
- Modify: `src/road.hpp/.cpp` (`RoadSurface` gains the per-type projected fields and a visibility flag)
- Test: extend `tests/oracle_road_test.cpp` to compare `height_left_row`, `height_right_row`, `lane_left`, `lane_right`, `visible`, and the type-specific projected fields added in Task 1

**Interfaces:**
- Consumes: `rr::TerrainSlice` (Task 4), `rr::project_road`
- Produces:
  ```cpp
  namespace rr {
  struct DrawNode { int32_t depth; uint8_t kind; /* road, shoulder, wall, hill, marking; values from Step 1 */ std::vector<Trapezoid> polys; };
  struct Scene { std::vector<DrawNode> nodes; int32_t occlusion_row; };
  Scene build_scene(const std::vector<RoadSlice>& slices, const std::vector<RouteSample>& route);
  }
  ```

- [ ] **Step 1:** Extend `oracle_road_test.cpp` expectations to the new fields. Run it and see it fail.
- [ ] **Step 2:** Port `0x00444630` cases 1–4 and the `0x00443d70` tail into `project_road` (fields) and `build_scene` (nodes).
- [ ] **Step 3:** Run `ctest -R oracle-road` until PASS on all Task 1 scenarios. Deviations are handled as in plan 01 Task 6 Step 3.
- [ ] **Step 4:** Commit: `render: port per-type projection, visibility and scene build`.

---

### Task 6: Road colours, markings and world draw

Colours come from the per-sample selector bytes (`+0x148/+0x149`), the `DLSR` flag bytes (`+16/+17`) and palette ranges. Which is which is traced in `0x0040ffc0` and the fillers.

**Files:**
- Create: `src/world_renderer.hpp`, `src/world_renderer.cpp`, `tests/oracle_frame_test.cpp`

**Interfaces:**
- Consumes: `rr::Scene`, `rr::fill_quad`, `rr::Framebuffer`, `rr::Palette`
- Produces:
  ```cpp
  namespace rr {
  struct WorldAssets {
      std::vector<uint8_t> horizon;        // palette indices, row-major (IMAGES/HORIZONS/<course>.BOB), filled in Task 7
      int horizon_width = 0, horizon_height = 0;
      uint8_t sky_index = 0;               // from Task 7's trace
      std::vector<uint8_t> colour_table;   // road/marking colour indices from Task 6's trace, read from the EXE
  };
  void draw_world(Framebuffer& fb, const ViewMode& mode, const Scene& scene, const WorldAssets& assets, int32_t heading);
  }
  ```

- [ ] **Step 1: Write the pixel golden test**

`tests/oracle_frame_test.cpp`: for each golden frame with `pixels.file`:
1. Rebuild the native scene for that frame's camera.
2. Draw it with `draw_world`.
3. Compare palette indices against the `.raw` inside the 3D view, excluding pixels covered by sprites.

Until plan 03 exists, exclude sprites with a mask: pixels where the original frame differs from a background-only render are ignored only if a later frame proves they belong to a moving sprite. Simpler, and required: compare only frames whose `slice` keys show no traffic and no rider within the view. Plan 03 later removes this restriction.

Report the percentage of identical pixels per frame. **Acceptance: 100% on the selected frames.**

- [ ] **Step 2: Trace colours and markings**

Read `0x0040ffc0` and the fillers for palette index sources. Document them.

- [ ] **Step 3: Port the colour selection and marking stripes**

Write `draw_world`.

- [ ] **Step 4: Iterate to 100%**

Use `tools/compare_frames.py` on converted frames to see where differences are.

- [ ] **Step 5: Commit**

`render: original road colours and markings`.

---

### Task 7: Sky and horizon

`0x00416310` selects and loads the horizon, meter and dashboard. `0x0044c440` splits hi-res horizon rows into two 360-wide halves. The horizon draw position (vertical: tied to camera height/pitch; horizontal: tied to accumulated heading) is traced from `0x0040ffc0` node types or `0x00446880`.

Known lead: every simulation step (`0x0044b3b0`) accumulates `DAT_004b7ef8 += bend × Δposition × 256`, using the sub-slice bend byte `+0xb8` of the ring slices at `+0x13c`/`+0x140` of the player's slice pointer `+0x2fc`. It splits the move across a sample boundary into two weighted parts. This is almost certainly the horizon's horizontal scroll. Start by dumping `0x004b7ef8` in the frame hook and correlating it with the horizon x offset in the raw frames.

**Files:**
- Modify: `src/world_renderer.cpp`, `src/viewer.cpp` (course → horizon file mapping from `0x00416310`)

- [ ] **Step 1:** Trace which horizon `.BOB` belongs to each course and how its x offset follows heading and y follows camera height. Document it.
- [ ] **Step 2:** Extend `oracle_frame_test` to include sky pixels (above the highest road row).
- [ ] **Step 3:** Implement and reach 100% on the selected frames of every scenario.
- [ ] **Step 4:** Commit: `render: horizon placement and sky`.

---

### Task 8: Viewer switch-over and cross-platform check

**Files:**
- Modify: `src/viewer.cpp`: course screens render via `draw_world` into a `Framebuffer`, uploaded each frame into an `SDL_PIXELFORMAT_RGBA32` streaming texture with `Framebuffer::to_rgba`. Delete the placeholder `draw_road` and `quad`.

- [ ] **Step 1:** Implement the switch. Keep `--screenshot`, now saving the framebuffer through `to_rgba` to BMP, so it is exact.
- [ ] **Step 2:** Rebuild the APK. If a device is connected, capture NAPA sample 300 on Android (`adb exec-out screencap -p`) and on macOS. Compare with `tools/compare_frames.py`; expect 100% inside the view after scaling with nearest-neighbour. If no device is connected, record "not verified on device" in `VERIFICATION.md`.
- [ ] **Step 3:** Update PROGRESS/REVERSE_ENGINEERING/VERIFICATION. Commit: `viewer: original world renderer replaces placeholder shading`.

### Display-list draw stage milestone (October 2, 2026)

`src/world_renderer.*` now implements generic ordered dispatch from caller-owned `DrawNode` objects. Original flat/texture quality selection, inclusive bbox rejection, normalization/flip handling, UV insets and mip selection match 32 original-function synthetic lists in both resolutions: 6,144,000 pixels exact. Signed CEL UV bounds match all 17 real indexed resources. Scene construction, resource selector resolution, dynamic rider specialization, horizon, natural full-frame comparisons and viewer switch-over are still required; Task 8 is not complete.

### Road construction/material milestone (October 2, 2026)

`src/road_panels.*` ports `0x0043c170` / `0x0043c0b0` geometry and material roles: 128 original cases, 343 emitted panels exact. `src/exe_tables.*` bounds-checks PE virtual-address data; `src/road_materials.*` reads original load descriptors and binds eight imported CEL slots, preserving the marked-middle alias. Together with the generic renderer, all 24,576,000 original pixels from 128 isolated real-material fixtures match. Natural cache/slice visibility, road/terrain composition, scene order and viewer switch-over remain pending; this does not close the full world renderer tasks.
