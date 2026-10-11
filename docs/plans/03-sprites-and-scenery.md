# Sprites and Scenery Implementation Plan

> **Status (October 11, 2026 review):** Decoders, families, CLGP, RRAN/RRFD and static scenery are done. Rider/car animation playback, shadows and merged actor nodes are in [plan 11](11-napa-race-completion.md) Task 2. This plan is kept as background for addresses and findings; follow the newer plans for remaining work.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires plan 01 and plan 02 Tasks 1–3 (oracle pixel dumps, view mapping, rasteriser).

**Goal:** Decode every sprite and animation format in the game (3DO-style cels, palette lookup tables, animation sets, rider families, bike bobs, shadows), place roadside scenery from course data, and draw scaled sprites into the world exactly as the original does.

**Architecture:**
- **Format decoders:** bounds-checked, one file per format, each with a synthetic unit test plus a corpus test that decodes every instance on the disc.
- **Sprite renderer:** a port of the original scaled-sprite blitter, driven by draw nodes that `scene.cpp` (plan 02) sorts with road polygons into the original display-list order.
- **Oracle:** plan 01's hook DLL gains dumps of display-list nodes so ordering, positions and scale are compared numerically before pixels are compared.

**Tech Stack:** C++17, CTest, oracle (plan 01), `rr::Framebuffer`, `rr::fill_trapezoid` (plan 02).

**Spec:** [00-INDEX.md](00-INDEX.md), [../REVERSE_ENGINEERING.md](../REVERSE_ENGINEERING.md).

## Global Constraints

- No decoded pixels, palettes or animation tables are committed. Tests use synthetic data, or read the disc and skip with exit code 77 when `../cdrom/ROADRASH` is absent.
- Every decoder rejects truncated input and oversized counts, and never reads outside the resource payload.
- Draw order and clipping must match the original display list. Proven by the Task 7 numeric oracle test before pixel comparison.

## Known facts to start from

| Resource | Where | Evidence so far |
| --- | --- | --- |
| `CEL ` | `GLOBAL.RSC` (32), each `.CRS` (2) | Payload starts `CCB ` (on-disk bytes `20 42 43 43`), chunk size at `+4`; 3DO Cel Control Block layout likely. Loaders `0x00441770`, `0x00441870`, `0x00441940` |
| `PAL ` | `GLOBAL.RSC` (1) | Payload `PLUT` (disk `54 55 4c 50`), size `0x2c`, count 16 at `+8`, then 16-bit entries (e.g. `0xd6ff`, `0x56fe`), probably 3DO 15-bit RGB (1-5-5-5). Loader `0x00441770` |
| `ANIM` | `GLOBAL.RSC` (4), `.CAR` (10–12) | Payload starts `RRAN` (disk `NARR`) header with offsets, then `RRFD` (disk `DFRR`) frame definitions. Users `0x00445a60`, `0x004465f0`, `0x0044aa80` |
| `CANS` | `GLOBAL.RSC` (4), `.CAR` (6) | Payload `CANS` (disk `SNAC`), size at `+4`, count at `+8`. Same users as `ANIM` |
| `FAM ` | `FAMILIES.RSC` (269) | 82,560-byte records for id 1. Users `0x00415ad0`, `0x0044a860`, `0x0044ab80` |
| `BIKERS/HIBOB.DAT`, `LOBOB.DAT` | files | Leading `uint16` then 8-bit palette-indexed data; high/low resolution bike+rider bobs |
| `BIKERS/HISHADOW.DAT`, `HISHADOC.DAT`, `LO*` | files | Shadow masks, probably RLE |
| `BIKERS/LOCOP.DAT` | file | Police bike bobs |
| `BIKERS/TEMPLATE.MIP` | file | Header then offset table; referenced by string `Template` at `0x00410160` |
| Course row chunks `BORR` (+0x08), `MSRR` (+0x0c), `DZHR` (+0x1c), `CESR` (+0x24) | `.CRS` | Iterators mapped in REVERSE_ENGINEERING.md. `0x00454240` calls `0x00438930` (object placement) for entries at sub-slice `+0x60..` |
| Game's own screenshot writer | `0x0041e880` | Format string `%s/Image%03d.BMP`; an alternative pixel oracle |

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/cel.hpp/.cpp` | `CCB`/`PLUT` decode to 8-bit indexed images with transparency |
| `src/anim.hpp/.cpp` | `ANIM` (`RRAN`/`RRFD`) and `CANS` decoding |
| `src/family.hpp/.cpp` | `FAM ` rider/bike family records |
| `src/bob.hpp/.cpp` | `BIKERS/*.DAT` bobs, shadows, `TEMPLATE.MIP` |
| `src/scenery.hpp/.cpp` | Per-sample roadside object placement from course chunks |
| `src/sprite_raster.hpp/.cpp` | Scaled, clipped, transparent sprite blit (port) |
| `tools/sprite_dump.cpp` | CLI: decode any sprite resource to PNG for inspection (local only) |
| `tests/cel_test.cpp`, `tests/anim_test.cpp`, `tests/bob_test.cpp`, `tests/scenery_test.cpp`, `tests/sprite_raster_test.cpp`, `tests/oracle_display_test.cpp` | Tests |

---

### Task 1: Oracle display-list dump

**Files:**
- Modify: `oracle/aweman_proxy.c`

**Interfaces:**
- Produces golden keys per frame: `node.<i>.{type,depth,x,y,scale,sprite_ptr,frame,flags}` for each display node visited by `0x0040ff40` (list head: the `param_1` passed in; next pointer at node `+4`; stop at `+0x0c == 0x40000000`), plus `node.count`.

- [ ] **Step 1: Identify node fields**

Read `0x0044d300` (display-list traversal and draw dispatch), `0x0040ff40` (transformed linked nodes), and `0x0040ffc0` (coordinate conversion). Write the node layout into `REVERSE_ENGINEERING.md`: type byte/word, depth key, screen position, scale, sprite reference, animation frame, flip/clip flags.

- [ ] **Step 2: Dump nodes**

In the draw hook from plan 02 Task 1 (patch at `0x004439f5`), before calling `0x00450420`, walk the list head `DAT_0049e66c` (the argument `0x00450420` passes to `0x0040ff40`). Log the fields from Step 1 with the key names above. For fields not yet understood, log raw dwords as `node.<i>.raw.<k>` for k in 0..31.

- [ ] **Step 3: Capture scenarios**

Capture `napa-traffic` (a 60 s race with opponents and traffic in view) and `city-scenery` (a 60 s city race), with a fixed seed and recorded input. Include pixels.

- [ ] **Step 4: Commit**

`oracle: display list node dump`.

---

### Task 2: 3DO cel and PLUT decoder

**Recovered implementation (October 2):** `src/cel.hpp/.cpp` reads CCB/PLUT and PC indexed pixel blocks. The actual disc has 42 CELs: 17 direct indexed textures (117 mips) and 25 legacy four-byte PDAT words. Original-loader comparisons match all fields/mip bytes. Generic 3DO packed-pixel decoding is not needed for the indexed PC blocks; legacy word rendering remains unresolved. `rr::Cel` retains palette/flags/dimensions, optional mip texture, and optional legacy word. It does not return a fabricated image for legacy records. See REVERSE_ENGINEERING.md and VERIFICATION.md; selector tables, transforms and sprite dump tooling remain open.

**Files:**
- Create: `src/cel.hpp`, `src/cel.cpp`, `tests/cel_test.cpp`, `tools/sprite_dump.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct Image8 { int width = 0, height = 0; std::vector<uint8_t> pixels; std::vector<uint8_t> opaque; }; // opaque: 1 = drawn
  struct Plut { std::vector<uint16_t> entries; };                    // raw 16-bit entries as stored
  Plut read_plut(const std::vector<uint8_t>& payload);                 // 'PLUT' chunk
  // Decode a CEL resource payload ('CCB ' chunk and its pixel data) to palette indices of the PC game.
  Image8 read_cel(const std::vector<uint8_t>& payload);
  }
  ```

- [ ] **Step 1: Establish the layout from code and data**

1. Read `0x00441770` (loads `CEL ` and `PAL `) and whatever it calls to prepare cels for drawing.
2. Determine:
   - Whether the PC version stores 3DO-format CCB flags (`PRE0`/`PRE1` words, width/height from preamble)
   - Packed versus unpacked rows
   - Bits per pixel
   - How PLUT 15-bit colours map to the 8-bit PC palette (a nearest-colour table? a direct index?)
3. Public references for 3DO cels (CCB and PDAT/PLUT layouts) describe the preamble bits. Use them only as hypotheses to confirm against the code.

Write the layout into `REVERSE_ENGINEERING.md` with addresses.

- [ ] **Step 2: Write failing tests**

- **Synthetic:** build a minimal `CCB ` payload in the test from the documented layout (a 4×2 cel, 8 bpp unpacked or the format found in Step 1). Assert `read_cel` returns width 4, height 2 and the exact indices, and that truncating the buffer at every length throws. Also test `read_plut` on a two-entry PLUT: count and values.
- **Corpus test** (skips with 77 if the disc is absent): decode all 32 `CEL ` in `GLOBAL.RSC` and both in each `.CRS`. Assert non-zero dimensions no larger than 640×480 and no exceptions.

- [ ] **Step 3: Run to verify failure**

Run: `cmake --build build --parallel 2>&1 | grep -m1 cel.hpp`
Expected: header missing.

- [ ] **Step 4: Implement**

Implement per Step 1. `tools/sprite_dump.cpp` (built only for desktop) writes `Image8` through `PALETTE.RAW` to PNG using `stb_image_write.h`. Add that header to `third_party/` with its licence note in `third_party/README.md`.

- [ ] **Step 5: Run the tests**

All pass. Dump every cel to `/tmp/cels/` and look at them; they should be recognisable images.

- [ ] **Step 6: Commit**

`sprites: decode 3DO-style cels and PLUTs`.

---

### Task 3: Bobs, shadows and MIP template

**Files:**
- Create: `src/bob.hpp`, `src/bob.cpp`, `tests/bob_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct BobSheet { std::vector<Image8> frames; };
  BobSheet read_bobs(const std::vector<uint8_t>& file);       // HIBOB/LOBOB/LOCOP
  BobSheet read_shadows(const std::vector<uint8_t>& file);    // HISHADOW/HISHADOC/LOSHADOW/LOSHADOC
  struct MipTemplate { /* fields from Step 1 */ };
  MipTemplate read_mip_template(const std::vector<uint8_t>& file);
  }
  ```

- [ ] **Step 1: Find the loaders**

Search the string table for the file names:
```sh
grep -nE "BOB|SHADO|COP|TEMPLATE|\.MIP|\.DAT" analysis/decompiled/string-references.tsv
```
Read the referencing functions. Many names may be built with `%s` (for example `Hi`/`Lo` prefixes, as in `/kHiDash.BOB` at `0x00416310`). If there is no direct hit, search for `fopen`/`CreateFileA` callers with `.DAT`.

Document the header, frame table and per-frame encoding (RLE, transparency index).

- [ ] **Step 2: Tests**

Write synthetic tests, then a corpus test that decodes all seven `BIKERS` files. Assert:
- Frame counts are equal between HI and LO variants.
- Every frame fits in 640×480.
- Shadow frame counts match bob frame counts where the loader pairs them.

- [ ] **Step 3: Implement, pass, dump to PNG, inspect, commit**

Commit: `sprites: decode bike bobs, shadows and MIP template`.

---

### Task 4: Animation sets and rider families

**Files:**
- Create: `src/anim.hpp`, `src/anim.cpp`, `src/family.hpp`, `src/family.cpp`, `tests/anim_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct AnimFrame { uint16_t image; int16_t dx, dy; uint16_t flags; };   // fields confirmed in Step 1
  struct Animation { std::vector<AnimFrame> frames; };
  struct AnimSet { std::vector<Animation> animations; };
  AnimSet read_anim(const std::vector<uint8_t>& payload);                 // 'RRAN' + 'RRFD'
  struct Cans { std::vector<uint32_t> entries; };                          // meaning from Step 1
  Cans read_cans(const std::vector<uint8_t>& payload);
  struct Family { /* sprite frame table and palette remap for one rider/bike family */ };
  Family read_family(const std::vector<uint8_t>& payload);                // 'FAM '
  }
  ```
  Field names inside these structs are fixed by Step 1's documentation. The function names are fixed here.

- [ ] **Step 1: Trace**

1. Read `0x00445a60`, `0x004465f0` and `0x0044aa80` (`ANIM`/`CANS`), and `0x00415ad0`, `0x0044a860` and `0x0044ab80` (`FAM `).
2. `RRAN` header: offsets at `+8`, `+0xc`, `+0x10` point to sub-chunks (`RRFD` at `+0x14` in GLOBAL id 1).
3. `FAM ` id 1 begins `e0 34 03 00 70 42 01 00 …`; the two leading dwords (`0x334e0`, `0x14270`) look like sizes or offsets.
4. Document every field.

- [ ] **Step 2: Tests**

Synthetic plus corpus:
- All `ANIM`, `CANS` in `GLOBAL.RSC` and the five `.CAR` files decode.
- All 269 `FAM ` records decode.
- Every frame's image reference resolves to an existing image.

- [ ] **Step 3: Implement, pass, commit**

Commit: `sprites: decode animations, CANS and rider families`.

---

### Task 5: Roadside scenery placement

**Files:**
- Create: `src/scenery.hpp`, `src/scenery.cpp`, `tests/scenery_test.cpp`
- Modify: `src/course.hpp/.cpp` (`PathSample` gains scenery references)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct SceneryObject { uint16_t sprite; int32_t lateral; int32_t height; uint8_t side; uint8_t flags; };
  // Objects anchored at a route sample (from BORR/MSRR/DZHR/CESR as established in Step 1).
  std::vector<SceneryObject> scenery_at(const Course&, uint32_t section, uint32_t sample);
  }
  ```

- [ ] **Step 1: Map chunk → object**

The row chunks `BORR` (row `+0x08`; iterator `0x00451b60`/`0x00451e20`; per-record flag word unpacked by `0x00451a90`), `MSRR` (`+0x0c`; `0x004522a0`/`0x00452580`), `DZHR` (`+0x1c`; `0x00452060`/`0x00452230`) and `CESR` (`+0x24`; `0x00452610`/`0x00452690`, flags via `0x00452710`) feed sub-slice fields.

`0x00454240` places objects for the entries counted by sub-slice byte `+0x60` (pointer list at `+0x6c`) through `0x00438930`.

Establish which chunk holds scenery sprites, which holds hazards (for example oil or gravel), and which holds section events such as finish, police zones or checkpoints. Record the findings for plans 04/05.

- [ ] **Step 2: Oracle**

Extend `dump_frame` to log sub-slice `+0x60` and the entries at `+0x6c` (`slice.N.road.R.objects.K.*`). Recapture `city-scenery`.

- [ ] **Step 3: Test**

`tests/scenery_test.cpp` compares `scenery_at` for every slice in the golden with the dumped entries (skips if absent), plus a synthetic chunk test.

- [ ] **Step 4: Implement, pass, commit**

Commit: `course: decode roadside scenery placement`.

---

### Task 6: Scaled sprite blitter

**Files:**
- Create: `src/sprite_raster.hpp`, `src/sprite_raster.cpp`, `tests/sprite_raster_test.cpp`

**Interfaces:**
- Consumes: `rr::Image8`, `rr::Framebuffer`, `rr::ViewMode`
- Produces:
  ```cpp
  namespace rr {
  struct SpriteDraw { const Image8* image; int32_t x_q16, y_q16; int32_t scale_q16; bool flip; int32_t clip_bottom_q16; const uint8_t* remap /* 256 entries or nullptr */; };
  void draw_sprite(Framebuffer&, const ViewMode&, const SpriteDraw&);
  }
  ```

- [ ] **Step 1: Trace**

Find the sprite blit among the `0x0044d300` dispatch targets (sprite node types from Task 1). Document:
- The anchor point (bottom-centre?)
- The scale stepping (Q16 source step per destination pixel)
- Transparency
- Flip
- The remap table (the family palette remap from Task 4)
- Clipping against the occlusion row

- [ ] **Step 2: Failing tests**

For example (adjust the anchor to the documented rule): a 2×2 sprite at scale 2.0 anchored bottom-centre at (100, 100) covers x 98..101 and y 96..99. Transparent pixels leave the framebuffer unchanged. `flip` mirrors. A sprite below `clip_bottom` is cut.

- [ ] **Step 3: Port, pass, commit**

Commit: `render: port scaled sprite blitter`.

---

### Task 7: Sprites in the display list, numeric then pixel oracle

**Files:**
- Modify: `src/scene.hpp/.cpp` (sprite `DrawNode` kinds and the original sort key), `src/world_renderer.cpp` (draw sprites)
- Create: `tests/oracle_display_test.cpp`
- Modify: `tests/oracle_frame_test.cpp` (drop the no-sprites frame restriction from plan 02 Task 6)

**Interfaces:**
- Consumes: everything above; `rr::Scene`
- Produces: `void add_scenery(Scene&, const std::vector<RoadSlice>&, const std::vector<RouteSample>&, const Course&, const SpriteLibrary&)`, where `struct SpriteLibrary { std::vector<Image8> cels; /* plus families, bobs */ };` is loaded once per race.

- [ ] **Step 1: Numeric test**

`oracle_display_test` rebuilds each golden frame's scene (roads and scenery only; skip dynamic vehicle nodes until plans 04/05) and asserts that node order, positions and scale equal the dumped `node.*` for the scenery nodes. Watch it fail.

- [ ] **Step 2: Implement**

Port the scenery node insertion into `scene.cpp`, including the depth sort key and tie-breaking.

- [ ] **Step 3: Pixel test**

`oracle_frame_test` must reach 100% identical pixels on `city-scenery` frames where no vehicle is in view.

- [ ] **Step 4: Viewer**

Course screens now show scenery.

- [ ] **Step 5: Docs and commit**

Update docs, then commit: `render: roadside scenery in the original display list order`.
