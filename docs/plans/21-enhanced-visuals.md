# Enhanced Visuals Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires [plan 19](19-high-resolution-rendering.md) Tasks 1–3 and 7 (resolution audit, scaled mapping, raster at any size, render independence) and [plan 18](18-settings-and-options.md) Tasks 2–5 (settings, Display screen). Task 7 shares plan 20 Task 6's GPU filter infrastructure. Supersampling is already plan 19 Task 8 and is not repeated here.

**Goal:** Optional modern graphics on top of the faithful renderer, each one a setting that defaults off:
1. smooth motion at 120/144 Hz and above (frame interpolation);
2. perspective-correct textures;
3. full-colour rendering with bilinear/trilinear texture filtering;
4. distance fog and colour grading;
5. a longer draw distance;
6. bloom, vignette and motion blur;
7. widescreen.

**Architecture:**
- **Two rendering modes.** *Original* is today's path: 8-bit indexed framebuffer, affine textures, 33 slices, 4:3, and it must stay bit-identical to the original game. *Enhanced* is the same scene pipeline with optional stages switched on. Every enhancement is gated by its own setting, and with all of them off the enhanced path must produce exactly the original output (each task has a test for this).
- **The scene pipeline is unchanged at its core.** `CourseScene`, the actor builders and the scenery code emit `DrawNode`s with Q16 view coordinates; `render_nodes` rasterises them in display-list order (painter's algorithm, no depth buffer). Enhancements add data to `DrawNode` (per-vertex depth) and add alternative raster paths; they never reorder the display list.
- **The simulation is untouched.** Interpolation and every visual setting read simulation state; nothing writes it. Plan 19 Task 7's independence test is extended to every setting here.
- **Where the work runs.** Rasterisation stays on the CPU (threaded per plan 19 Task 6). Post-processing (grading, bloom, vignette, motion blur) runs on the GPU through SDL's GPU render state, like plan 20's screen filters, and is hidden where that API is missing.

**Tech Stack:** C++17 (C++20 once plan 22 Task 1 lands), SDL3 3.4 (`SDL_CreateGPURenderState`), SDL_shadercross (offline shader compilation), CTest.

**Spec:** the owner's request of October 12, 2026 to plan the enhancements listed in the session 26 conversation: high-refresh smoothing, perspective-correct textures, (supersampling, already plan 19), smooth texture filtering, distance fog and colour grading, longer draw distance, light post-processing, widescreen.

## Global Constraints

- **Original stays exact.** With every enhancement off: every `oracle-raster`, `oracle-scene`, `hires-render`, `world_renderer`, `hud` and `frontend` test passes unchanged, with no tolerance.
- **The simulation never sees a visual setting.** Plan 19 Task 7's hash test runs with every enhancement on and must give the same per-tick hashes as with all of them off.
- **No original data in the source tree.** Colour-grading lookup tables are generated procedurally by our code; fog colours are computed at runtime from the user's imported sky/horizon art.
- **Performance budget.** Enhanced settings are measured on the Mac (`--bench`, plan 19 Task 6) and the "Enhanced" preset may only include settings that keep p95 ≤ 8.3 ms (120 Hz) at render scale 2 on the owner's Mac. Record the numbers in VERIFICATION.md.
- Floating point is allowed in enhanced rendering code (it is presentation, not simulation), but results must be deterministic for a given platform so tests can pin them.

## Review Focus

1. **Teleports during interpolation.** A crash reset, a remount, a split commit, a rider respawn or a camera cut must not be drawn as a smear between two far-apart positions. Task 2 tests each discontinuity explicitly.
2. **Transparent texels under filtering.** Bilinear filtering of sprites and scenery blends colour index 0 (transparent) into edges, giving dark halos. Task 4 uses alpha-tested, premultiplied filtering and tests a sprite edge.
3. **Rider colour remaps and palette changes in full-colour mode.** Remaps must be applied before the conversion to colour, and any runtime palette change in the original (to be traced) must still show. Task 4's "all enhancements off in full-colour mode equals the indexed path" test catches both.
4. **Draw distance beyond the original's data.** Projecting past 33 slices can reach the course end, split windows or terrain sections the original never built at that distance. Task 6 sweeps every course and branch at the longest setting.
5. **Widescreen culling.** Riders, cars and scenery that the original culls just outside the 4:3 view must appear at the sides rather than pop in. Task 8's centre-crop test proves the 4:3 centre is unchanged and its pop-in measurement covers the sides.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/settings.hpp/.cpp` | `Enhancements` sub-struct and preset |
| `src/view_state.hpp/.cpp` | Snapshot of everything the renderer reads per tick; wrap-aware blending |
| `src/session.hpp/.cpp` | `TickClock::alpha_q16()` |
| `src/world_renderer.hpp/.cpp` | `DrawNode::view_depth`; enhanced render entry |
| `src/texture_perspective.cpp` | Perspective-correct textured spans |
| `src/framebuffer_rgb.hpp/.cpp`, `src/raster_rgb.cpp` | Full-colour framebuffer and filtered raster |
| `src/fog.hpp/.cpp` | Fog factor, fog colour from the course sky |
| `src/course_scene.cpp`, `src/road.cpp` | Per-vertex depth; variable slice count |
| `src/post_chain.hpp/.cpp`, `shaders/*.frag` | Grading LUT, bloom, vignette, motion blur on the GPU |
| `src/view.hpp`, `src/hud.cpp`, `src/frontend.cpp` | Widescreen logical width, HUD and menu layout |
| `src/native_screens.cpp`, `src/native_strings.cpp` | VISUAL STYLE item and the Enhancements screen |
| `tests/view_state_test.cpp`, `tests/texture_perspective_test.cpp`, `tests/raster_rgb_test.cpp`, `tests/fog_test.cpp`, `tests/post_chain_test.cpp`, `tests/widescreen_test.cpp`, `tests/hires_render_test.cpp` | Tests |

---

### Task 1: Enhancement settings and the original/enhanced switch

**Files:**
- Modify: `src/settings.hpp`, `src/settings.cpp`, `tests/settings_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  enum class TextureFilter : uint8_t { nearest = 0, bilinear = 1, trilinear = 2 };
  enum class Grade : uint8_t { none = 0, warm = 1, vivid = 2, dusk = 3 };
  struct Enhancements {
      bool interpolate = false;          // Task 2
      bool perspective = false;          // Task 3
      bool full_colour = false;          // Task 4 (implied by filter != nearest)
      TextureFilter filter = TextureFilter::nearest;
      int fog = 0;                       // Task 5: 0..100 strength
      Grade grade = Grade::none;         // Task 5
      int draw_slices = 33;              // Task 6: 33 (original), 48, 64
      bool bloom = false, vignette = false, motion_blur = false;   // Task 7
      int widescreen = 0;                // Task 8: 0 = 4:3, 1 = 16:10, 2 = 16:9, 3 = match display
      bool any() const;                  // true if any field differs from the defaults
  };
  Enhancements enhanced_preset();        // the measured "Enhanced" preset (Task 9 fills it)
  // in struct Settings:
  Enhancements enhancements;
  }
  ```
  Keys: `enh_interpolate`, `enh_perspective`, `enh_full_colour`, `enh_filter`, `enh_fog`, `enh_grade`, `enh_draw_slices`, `enh_bloom`, `enh_vignette`, `enh_motion_blur`, `enh_widescreen`.

- [ ] **Step 1: Write the failing test** in `tests/settings_test.cpp`:

```cpp
    {
        rr::Settings e;
        e.enhancements.interpolate = true; e.enhancements.filter = rr::TextureFilter::trilinear; e.enhancements.fog = 40;
        e.enhancements.grade = rr::Grade::dusk; e.enhancements.draw_slices = 64; e.enhancements.widescreen = 2;
        const rr::Settings f = rr::parse_settings(rr::format_settings(e));
        check(f.enhancements.interpolate && f.enhancements.filter == rr::TextureFilter::trilinear && f.enhancements.fog == 40 &&
              f.enhancements.grade == rr::Grade::dusk && f.enhancements.draw_slices == 64 && f.enhancements.widescreen == 2,
              "enhancements round trip");
        const rr::Settings g2 = rr::parse_settings("enh_draw_slices=40\nenh_fog=101\nenh_filter=3\nenh_widescreen=9\n");
        check(g2.enhancements.draw_slices == 33 && g2.enhancements.fog == 0 && g2.enhancements.filter == rr::TextureFilter::nearest &&
              g2.enhancements.widescreen == 0, "only 33/48/64 slices and in-range values accepted");
        check(!rr::default_settings().enhancements.any(), "defaults are the original renderer");
    }
```

- [x] **Step 2:** Run `ctest -R settings`; expected compile failure. DONE (no member `enhancements`/`TextureFilter`).
- [x] **Step 3: Implement** the struct, `any()` (compare with a default-constructed `Enhancements`), parsing with the ranges above (`draw_slices` accepts exactly 33, 48, 64), formatting, and `enhanced_preset()` returning all-off for now. DONE. Deviation: `widescreen` is `Widescreen` enum, not `int` (reuses the Task-8 setting; `enh_widescreen` key; `Game::logical_width` prefers it when `any()`).
- [x] **Step 4:** Run; expected PASS. DONE: settings green; 67 fast suites green. Commit: `settings: enhanced-visuals options (all off by default)`.

---

### Task 2: High-refresh frame interpolation

**Files:**
- Create: `src/view_state.hpp`, `src/view_state.cpp`, `tests/view_state_test.cpp`
- Modify: `src/session.hpp`, `src/session.cpp` (`TickClock::alpha_q16`), `src/game_main.cpp` (capture after each tick; build the frame from the blended state), the scene/actor builders that read `Race` directly for positions (`Game::rider_nodes` and `course_canvas.draw` in `game_main.cpp`, `CourseScene` camera input), `CMakeLists.txt`

**Interfaces:**
- Consumes: `Race` (camera position, camera, riders' bike/rider objects, crash actors), `TrafficSystem` cars, cops, plan 19 Task 7's guarantee that drawing does not consume `rand()`.
- Produces:
  ```cpp
  namespace rr {
  struct ActorPose { uint32_t id = 0; uint32_t position_q8 = 0; int32_t lateral_q8 = 0, height_q8 = 0, heading = 0, roll = 0; int route = 0; bool visible = false; };
  struct ViewState {
      uint32_t camera_position_q8 = 0;   // Race::camera_position()
      Camera camera;                     // x, height
      int camera_route = 0;
      std::vector<ActorPose> actors;     // riders, crash riders, cars, cops, in a stable id order; capacity reserved once
      uint32_t tick = 0;
  };
  void capture_view(ViewState&, const Race&, const TrafficSystem*);            // no allocation after the first call
  // alpha_q16 in [0, 65536]. Wrap-aware on uint32 positions. An actor (or the camera) whose route changed,
  // whose id is new or gone, or whose position jumped by more than `teleport_q8` is not blended: it is drawn at `b`.
  void blend_view(ViewState& out, const ViewState& a, const ViewState& b, uint32_t alpha_q16, uint32_t teleport_q8 = 8 << 8);
  }
  // TickClock: uint32_t alpha_q16() const;   // remainder_ / tick length, in Q16
  ```
  Animation frames, sprite views and every non-positional value come from the newest tick (`b`); only positions, heights and angles are blended. The renderer draws one tick behind the simulation; the input-to-photon delay grows by at most one tick (16.7 ms), which USER_GUIDE states.

- [ ] **Step 1: Write the failing test** `tests/view_state_test.cpp` (synthetic):

```cpp
#include "view_state.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << "\n"; } };
    rr::ViewState a, b, out;
    a.camera_position_q8 = 1000; b.camera_position_q8 = 1256;
    a.actors = {{1, 5000, 0, 0, 0, 0, 0, true}}; b.actors = {{1, 5100, 256, 0, 0, 0, 0, true}};
    rr::blend_view(out, a, b, 0);       check(out.camera_position_q8 == 1000 && out.actors[0].position_q8 == 5000, "alpha 0 is a");
    rr::blend_view(out, a, b, 65536);   check(out.camera_position_q8 == 1256 && out.actors[0].position_q8 == 5100, "alpha 1 is b");
    rr::blend_view(out, a, b, 32768);   check(out.camera_position_q8 == 1128 && out.actors[0].lateral_q8 == 128, "halfway");
    a.camera_position_q8 = 0xffffff80u; b.camera_position_q8 = 0x80;
    rr::blend_view(out, a, b, 32768);   check(out.camera_position_q8 == 0, "wraps forward through zero");
    b.actors[0].route = 1;              // split commit
    rr::blend_view(out, a, b, 32768);   check(out.actors[0].position_q8 == 5100, "route change is not blended");
    b.actors[0].route = 0; b.actors[0].position_q8 = 5000 + (64 << 8);   // crash reset / remount jump
    rr::blend_view(out, a, b, 32768);   check(out.actors[0].position_q8 == b.actors[0].position_q8, "teleport is not blended");
    b.actors.push_back({2, 9000, 0, 0, 0, 0, 0, true});                  // new car
    rr::blend_view(out, a, b, 32768);   check(out.actors.size() == 2 && out.actors[1].position_q8 == 9000, "new actor drawn at b");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
```

  Also add to `session_test`: `TickClock` after advancing 1.5 ticks reports `alpha_q16() == 32768`.
- [x] **Step 2:** Run; expected FAIL. DONE (link error on `blend_view`; no member `alpha_q16`).
- [x] **Step 3: Implement** `view_state.cpp` and `alpha_q16`. DONE. Capture covers riders, crash riders, active cars; camera x/height + route. Race loop captures after tick batches, blends before draw (off: alpha=65536 = live, exact). Rider quads, traffic cars, project_road camera read the blend; peds, horizon heading, HUD text stay live (documented limits).
- [x] **Step 4: Tests.** DONE. `hires-render` green; off-mode headless shot bit-identical to the pre-change baseline (a35b485 tree: 0 sampled diffs); interpolation-on independence extends the Task-7 soak (same hash); crash/remount teleport rule holds over scripted crash transitions. Split-commit route-change rule covered in view_state_test (route change → draw at b).
- [x] **Step 5: Present faster than 60 Hz.** DEFERRED: no 120 Hz display on this machine; the loop already draws every vsync when steps=0 (blend reuses the last pair with fresh alpha), so high-refresh presentation works when the display allows it. No measurement recorded.
- [x] **Step 6:** Commit: `render: interpolated frames for high refresh rates`.

---

### Task 3: Perspective-correct textures

**Files:**
- Create: `src/texture_perspective.cpp`, `tests/texture_perspective_test.cpp`
- Modify: `src/world_renderer.hpp/.cpp` (`DrawNode::view_depth`), `src/texture.hpp`, the panel builders (`src/road_panels.cpp`, `src/terrain_panels.cpp`, `src/building_panels.cpp`, `src/terrain_ground.cpp`, `src/course_scene.cpp`), `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  // DrawNode gains:
  std::array<int32_t,4> view_depth{};   // Q8 slices ahead of the camera per corner; all zero = unknown (affine)
  // texture.hpp:
  struct PerspectiveVertex { int32_t x = 0, y = 0; float u = 0, v = 0, inv_z = 0; };
  void draw_textured_quad_perspective(Framebuffer&, const std::array<PerspectiveVertex,4>&, const CelMip&, bool transparent, RasterClip);
  ```
  Panels built from road slices fill `view_depth` from the slice depths (`RoadSlice::depth`, Q8). Billboards (sprites, scenery cards) face the camera and stay affine (their depth is constant across the quad). The perspective span function interpolates `u/z`, `v/z`, `1/z` along edges and divides every 16 pixels (classic span subdivision), with the same fill rules as the affine raster.

- [ ] **Step 1: Write the failing tests** `tests/texture_perspective_test.cpp` (synthetic):
  - a quad with all four depths equal renders byte-identical to `draw_textured_quad` on the same vertices (an affine quad is a special case);
  - a road-like trapezoid (bottom edge 200 px wide at depth 1, top edge 50 px at depth 4) textured with a 64×64 texture whose rows are a vertical gradient: at the screen row halfway between top and bottom, the sampled texture row equals the analytic perspective value `v = 64 · (1/z_mid − 1/z_near) / (1/z_far − 1/z_near)` within ±1 texel, and the affine path is measurably different there (documents the original's warp);
  - fill rule: the same trapezoid covers exactly the pixels the affine version covers.
- [ ] **Step 2:** Run; expected FAIL.
- [ ] **Step 3: Implement**; `render_nodes` uses the perspective path when `perspective` is on and the node has non-zero depths, otherwise today's path.
- [ ] **Step 4:** Run the tests and the whole suite (with the setting off nothing changes). Screenshot comparison at render scale 2, on and off, at a sharp bend near the camera. Commit: `render: optional perspective-correct textures`.

---

### Task 4: Full-colour raster and bilinear/trilinear filtering

**Files:**
- Create: `src/framebuffer_rgb.hpp`, `src/framebuffer_rgb.cpp`, `src/raster_rgb.cpp`, `tests/raster_rgb_test.cpp`
- Modify: `src/world_renderer.hpp/.cpp` (enhanced entry), `src/cel.cpp` (RGBA copies of mips on demand), `src/actor_nodes.cpp` (RGBA rider pages after the remap), `src/game_main.cpp` (present the RGB frame directly), `docs/REVERSE_ENGINEERING.md` (palette trace)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  class FramebufferRgb { public: FramebufferRgb(int w, int h); int width() const; int height() const; uint32_t* row(int y); void clear(uint32_t argb); };
  struct RgbaMip { int width = 0, height = 0; std::vector<uint32_t> premultiplied; };   // alpha 0 where index 0 was transparent
  // Converted once per (texture, palette generation); remaps are applied to the index image before conversion.
  const std::vector<RgbaMip>& rgba_mips(const CelTexture&, const Palette&, uint32_t palette_generation);
  void render_nodes_rgb(FramebufferRgb&, const std::vector<DrawNode>&, const ViewMode&, uint32_t flat_mask,
                        const Palette&, const Enhancements&, RenderThreads&);
  }
  ```
  Filtering is alpha-tested on premultiplied texels (blend the four texels, then discard if alpha < 0.5, then un-premultiply), which removes dark halos (Review Focus 2). Trilinear blends the two nearest mip levels using the same level choice as `texture_mip_level` plus its fractional part.

- [ ] **Step 1: Trace (time box: half a session).** Does the original change the palette during a race (flashes, fades, the arrest or crash effects, the sky) or use palette ranges for shading? Search the disassembly for writes to the palette upload path used by `0x00448f90`. Record the answer. If it does, `palette_generation` increments at each change and the RGB textures are re-converted (cached per generation).
- [ ] **Step 2: Write the failing tests** `tests/raster_rgb_test.cpp`:
  - **Equivalence (Review Focus 3):** for every golden display list available (skip 77 without them) and a synthetic one, `render_nodes_rgb` with `filter = nearest` and all enhancements off produces exactly `Framebuffer::to_rgba` of the indexed render, at render scales 1 and 2.
  - **Halo (Review Focus 2):** a 16×16 sprite texture (a white disc on index 0) magnified 8× with bilinear filtering: no output pixel outside the disc's scaled silhouette (±1 px) is non-transparent, and no pixel inside is darker than the disc colour.
  - **Bilinear value:** a 2×2 texture (black, white / white, black) stretched to 64×64: the centre pixel is mid-grey ±2.
  - **Trilinear:** a quad shrinking smoothly across a mip boundary has no step larger than 8 levels in mean brightness between adjacent rows.
- [ ] **Step 3:** Run; expected FAIL. **Implement.** The game uses the RGB path when `full_colour` is on or `filter != nearest`.
- [ ] **Step 4:** Run; expected PASS. Measure frame time at scales 1–4 with bilinear and trilinear; record it.
- [ ] **Step 5:** Commit: `render: full-colour raster with bilinear and trilinear filtering`.

---

### Task 5: Distance fog and colour grading

**Files:**
- Create: `src/fog.hpp`, `src/fog.cpp`, `tests/fog_test.cpp`
- Modify: `src/raster_rgb.cpp` (per-vertex fog, interpolated like colour), `src/post_chain.cpp` (grading; created in Task 7 if not yet, minimal here), `src/course_scene.cpp` (sky colour)

**Interfaces:**
- Consumes: `DrawNode::view_depth` (Task 3), the RGB path (Task 4).
- Produces:
  ```cpp
  namespace rr {
  // 0 at the camera, rising to `strength`/100 at the last drawn slice; exponential-squared falloff.
  uint16_t fog_factor_q8(int32_t depth_q8, int draw_slices, int strength);
  // Average colour of the bottom 8 rows of the course's horizon image: the fog colour (no data stored in source).
  uint32_t fog_colour(const RgbImage& horizon_bottom);
  // Procedural 17x17x17 grading LUT for a Grade; Grade::none is the identity.
  std::vector<uint32_t> grade_lut(Grade);
  }
  ```
  Nodes with unknown depth (zero) take the depth of their slice from the scene builder; billboards get their own depth (`ActorProjection::depth`), so riders and scenery fog consistently with the road.

- [ ] **Step 1: Write the failing tests** `tests/fog_test.cpp`: `fog_factor_q8(0, 33, 100) == 0`; it is monotonic non-decreasing in depth; at the last slice with strength 100 it is 256; strength 0 is 0 everywhere; `grade_lut(Grade::none)` maps every lattice point to itself; every LUT entry of every grade is a valid colour and the LUT is monotonic in luminance along the grey axis (no grade inverts brightness).
- [ ] **Step 2:** Run; expected FAIL. **Implement.** Fog applies in the RGB raster before blending into the framebuffer; the sky and horizon are not fogged. The far fog also fades in nodes appearing at the far slice, which softens pop-in.
- [ ] **Step 3:** With fog 0 and grade none, the Task 4 equivalence test still passes. Screenshot each grade and fog at 30/60/100 on Napa and CITY. Commit: `render: distance fog and colour grading`.

---

### Task 6: Longer draw distance

**Files:**
- Modify: `src/road.hpp/.cpp` (`view_slices` becomes a parameter), `src/course_scene.cpp` and panel builders, `src/scenery.cpp`, actor visibility in `game_main.cpp`, `tests/course_scene_test.cpp`, `tests/hires_render_test.cpp`, `docs/REVERSE_ENGINEERING.md`

- [ ] **Step 1: Trace (time box: one session).** How is the 33-slice limit built in: `0x00443a00` projection, the depth table length and its values (`read_depth_table`), the far-depth and occlusion row logic (`src/scene.hpp`: `far_depth`, `occlusion_row`), terrain and building panel construction ranges, scenery visibility, and actor visibility. Can the depth table be extended with the same law the original's values follow (fit the existing entries; record the formula and its maximum error over the original entries, which must be zero for the extension to be trusted)? Record the findings.
- [ ] **Step 2: Write the failing tests.** `project_road(..., slices = 33)` equals today's output for every Napa sweep view. With 48 and 64 slices, the sweep over every course and branch at level 0 and 4 (Review Focus 4) renders without exceptions; far slices past the course end are not projected; inside split/merge windows both roadways continue to the new far limit.
- [ ] **Step 3:** Run; expected FAIL. **Implement** the parameter, the extended depth table (only if Step 1 found an exact law; otherwise stop and report), and widen terrain, building, scenery and actor ranges to the new far limit. With 33 slices, every golden is unchanged.
- [ ] **Step 4:** Measure frame times at 33/48/64 slices and record them. Commit: `render: optional longer draw distance (48/64 slices)`.

---

### Task 7: Bloom, vignette and motion blur

**Files:**
- Create: `src/post_chain.hpp`, `src/post_chain.cpp`, `shaders/grade.frag`, `shaders/bloom_extract.frag`, `shaders/blur.frag`, `shaders/composite.frag`, `shaders/motion_blur.frag`, `tests/post_chain_test.cpp`
- Modify: `src/game_main.cpp` (final present), plan 20's `src/screen_filter.*` (merged into the chain if it exists: the chain runs grading → bloom → motion blur → vignette → screen filter), `tools/compile_shaders.sh` (plan 20 Task 6; create it here if that task has not landed), `CMakeLists.txt`

**Interfaces:**
- Produces: `class PostChain { public: PostChain(SDL_Renderer*); bool available() const; void configure(const Enhancements&, Grade); void apply(SDL_Texture* frame, float camera_speed); };` Motion blur blends with the previous frame by a factor that grows with the player's speed (passed in, read from the drawn `ViewState`; never from the simulation directly) and is zero when paused.

- [ ] **Step 1: Write the failing test** `tests/post_chain_test.cpp`: on a software renderer (no GPU state) `available()` is false and `apply` leaves the texture untouched; settings for bloom/vignette/motion blur round-trip (Task 1 already covers parsing; here check `configure` stores them); the motion-blur factor is 0 at speed 0, ≤ 0.5 at the maximum speed, and 0 for the first frame after a pause or a teleport.
- [ ] **Step 2:** Run; expected FAIL. **Implement** the shaders (our own code) and compile them with SDL_shadercross to SPIR-V, MSL and DXIL, committing the outputs.
- [ ] **Step 3:** Screenshots of each effect alone and together on Napa at speed. Commit: `render: bloom, vignette and motion blur`.

---

### Task 8: Widescreen

**Files:**
- Create: `tests/widescreen_test.cpp`
- Modify: `src/view.hpp` (logical width and x origin), the cull points found by plan 19 Task 9's trace (expected: road panel clipping, scenery and actor visibility), `src/hud.cpp` (centred dashboard with side panels), `src/frontend.cpp` (4:3 menus pillarboxed over a blurred, darkened copy of the screen picture), `src/game_main.cpp`, `docs/REVERSE_ENGINEERING.md`

**Interfaces:**
- Consumes: plan 19 Task 9 Step 1's trace (run it first if it has not been done; it is the time-boxed investigation of how the original culls at the sides).
- Produces: `ViewMode::logical_width` (320 original; 384 for 16:10, 427 for 16:9, or from the display) and `origin_q16_x = (logical_width − 320) / 2 · 65536`, applied in `map` before scaling. The vertical field of view is unchanged (Hor+): the 4:3 picture is the centre of the wide one.

- [x] **Step 1: Write the failing test** `tests/widescreen_test.cpp`: DONE. Seam 1 (real-race geometry: rider 1 at tick 200 shifted to the 320 cull edge — side actor kept at 426) FAILED before the fix, passes after. Seam 2 (golden replay: 16:9 centre crop == 4:3 exactly + side bands reveal ~30% new content) passes throughout (golden nodes are pre-culled; the seam-1 probe is the true red/green). Pop-in count kept informational (15 candidates; trace found no pop-in mechanism).
- [x] **Step 2:** Run; expected FAIL. DONE (`side actor visible at 426`, exit 1).
- [x] **Step 3: Implement.** DONE. `Widescreen` setting (off/16:10/16:9/match, `widescreen` key) + Display WIDESCREEN row/popup/setter; `logical_width_for`/`view_mode_wide` (426 for 16:9; match clamps 320..426); width threaded through `draw_nodes`, `road_panels`, `scenery_node`, `traffic_draw_nodes`/`traffic_car_node`, `ActorNodes::rider`, `pedestrian_node` (centres `(w&~1)*32768`, culls `left>=w`); race surface `wide*2s x 480s`, horizon spans wide, centred 4:3 dash + runtime edge-column side panels, wide logical presentation with letterbox, menus/videos stay 640 (auto-pillarbox). Deviation: 16:9 width is 426 (plan said 427; 426 = 320+2×53 keeps even centre); menus pillarbox over clear colour, not a blurred copy (no GPU blur infra).
- [x] **Step 4:** Run; expected PASS. DONE: widescreen 0 failures; 67 fast + race-soak + hires-render green; 16:9 screenshot verified (wider road/shoulders/trees, centred dash path). USER_GUIDE documents the mode. Commit: `render: Hor+ widescreen`.

---

### Task 9: Presets, Enhancements screen and measured defaults

**Files:**
- Modify: `src/native_screens.cpp`, `src/native_strings.cpp`, `src/frontend.cpp`, `src/settings.cpp` (`enhanced_preset`), `tests/frontend_test.cpp`, `docs/USER_GUIDE.md`, `docs/VERIFICATION.md`

- [ ] **Step 1:** The Display screen gains VISUAL STYLE: Original (all off), Enhanced (`enhanced_preset()`), Custom. Custom enters a native ENHANCEMENTS screen, built like plan 18 Task 5's Display screen, with one item per field of `Enhancements`; items that need something unavailable (GPU post-processing, render scale above 1 for filtering to matter) are disabled with a description saying why.
- [ ] **Step 2: Measure and set the preset.** On the owner's Mac at render scale 2, measure p95 for each enhancement alone and for combinations; `enhanced_preset()` includes the largest set that keeps p95 ≤ 8.3 ms (Global Constraints). Record the table in VERIFICATION.md and the chosen preset in USER_GUIDE.
- [ ] **Step 3: Tests.** `frontend_test`: choosing Original sets every field to default; choosing Enhanced sets exactly `enhanced_preset()`; changing any field in Custom shows VISUAL STYLE as Custom. Controller-only navigation of the Enhancements screen works.
- [ ] **Step 4:** Run the regression gate. Commit: `frontend: visual style presets and Enhancements screen`.

## Exit criteria

- Each enhancement works on its own and with the others, behind its own setting, all off by default.
- With all enhancements off, every original-exact test passes unchanged; with all on, the simulation hash test passes.
- Interpolation shows no teleport smears; filtered sprites have no halos; the widescreen centre crop equals the 4:3 frame.
- The Enhanced preset's frame times are measured and recorded.
