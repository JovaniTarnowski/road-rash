# High-Resolution Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Needs plan 18 Tasks 2–3 (settings fields, display module) and Task 5 (Display screen) for its settings UI; Tasks 1–4 and 6 can start before them. Plan 20 (asset upscaling) builds on Task 3's mip extension.

**Goal:** Let the race, the front end and the dashboard render at 1280×960, 1920×1440, 2560×1920 or the display's own size, chosen in the Display settings screen, while the original 640×480 path stays bit-identical to the original game and keeps passing every golden.

**Architecture:**
- **Why this is feasible.** The scene pipeline never works in screen pixels. `CourseScene`, the actor builders and the scenery code emit `DrawNode`s whose corners are original **Q16 view coordinates** in the 320×240 logical space (`src/world_renderer.hpp`). Only `ViewMode::map` (`src/view.hpp`) turns them into surface pixels: high mode shifts by 15 and clears bit 0 (`0x00410160`/`0x0040ffc0`), which puts every original vertex on a 2-pixel grid. A higher resolution is a different final mapping: multiply by the render scale before shifting, and keep the sub-pixel bits the original threw away.
- **Render scale `s`** (1–4) is relative to the original high mode: the race surface is `640s × 480s`, the menus `640s × 480s`, the race window `640s × 510s`. `s = 1` uses the existing code path unchanged.
- **Textures.** At higher `s`, `texture_mip_level` (`0x004180b0`) automatically picks finer mips; at the top level it samples mip 0 with nearest filtering, which looks sharp but blocky. Plan 20 adds upscaled levels above mip 0; this plan adds the hook for them (Task 3).
- **Speed.** The rasteriser is single-threaded C++. 2560×1920 is 16× the pixels of 640×480. Task 6 splits the framebuffer into horizontal bands rendered on worker threads; every band renders the whole node list in order, clipped to its rows, so the output is bit-identical to one thread.
- **The simulation never sees the render scale** (Task 7 proves it).

**Tech Stack:** C++17, SDL3, `std::thread` (no new dependencies), CTest, the existing golden suites.

**Spec:** the owner's request of October 11, 2026: "I want the game to run at higher resolutions" (graphics settings). [18-settings-and-options.md](18-settings-and-options.md) for the settings UI.

## Global Constraints

- `s = 1` must produce exactly the current output: every `oracle-raster`, `oracle-scene`, `world_renderer`, `hud`, `frontend` test passes unchanged, with no tolerance.
- For `s > 1` there is no original to compare with. Acceptance uses the structural tests defined here, with the tolerances and reasons stated in each task.
- No allocation in the per-frame raster path; buffers are sized when the scale changes.
- Render scale, threads and every other presentation setting are invisible to `Race`, `TrafficSystem`, `Police` and the crash code.
- Android defaults to `s = 1` until plan 14 Task 2 measures the device; the setting is still available there.

## Review Focus

1. **Changing the render scale mid-race** (pause menu → Display → Render resolution): buffers are reallocated between frames, the next frame is complete, and the race continues on the same tick. Task 8's scripted test does this at every scale step.
2. **Polygons crossing a band boundary in the threaded rasteriser** (a road edge that starts in band 1 and ends in band 3): no seams, no missing or doubled rows. Task 6's bit-identity test covers every golden list at four thread counts.
3. **Very thin or far geometry** (lane markings and distant poles one original pixel wide): at higher `s` they must not break up or vanish, because the original's even-vertex rounding no longer hides sub-pixel gaps. Task 4's coverage test measures it on the far slices.
4. **Window smaller than the render surface** (4× on a 1080p display): the image is downsampled (supersampling) and stays sharp, not shimmering. Task 8 checks the presentation path picks linear downscaling in that case.
5. **Out-of-range coordinates** at large `s`: `TextureVertex` uses `int16_t` x/y, which overflows above 32,767; at `s = 4` a far-off-screen vertex can reach it. Task 3 widens or clamps with a test at the extremes.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/view.hpp` | `view_mode_scaled(int s)`: surface size and Q16 mapping for any scale |
| `src/texture.hpp/.cpp`, `src/raster.cpp` | Edge tables sized by surface height, widened vertex type, mip hook for upscaled levels |
| `src/world_renderer.hpp/.cpp` | Uses the scaled mode; band-parallel rendering |
| `src/actor_render.cpp`, `src/course_scene.cpp`, `src/terrain_ground.cpp`, horizon/sky code in `src/game_main.cpp` | Remove hard-coded 640/480/320/240 surface assumptions found in Task 1 |
| `src/hud.hpp/.cpp` | Dashboard composed at `s` |
| `src/frontend.hpp/.cpp`, `src/text.cpp` | Menus drawn at `s` with text rasterised at `height × s` |
| `src/render_threads.hpp/.cpp` | Fixed worker pool for band rendering |
| `src/game_main.cpp` | Surfaces, textures and presentation at `s` |
| `tests/view_test.cpp`, `tests/world_renderer_test.cpp`, `tests/hires_render_test.cpp`, `tests/hud_test.cpp`, `tests/frontend_test.cpp`, `tests/race_soak_test.cpp` | Tests |

---

### Task 1: Resolution audit (time box: one session)

**Files:**
- Modify: `docs/REVERSE_ENGINEERING.md` (new section "Resolution assumptions"), this plan (correct file lists if the audit finds more)

- [x] **Step 1:** List every fixed screen size in `src/`: `grep -rn "640\|480\|320\|240\|510" src | grep -v "^src/.*://"`. Known so far: `TextureEdge` rows `std::array<TextureEdge,480>` (`texture.hpp`), `ActorProjection::occlusion = 240*65536` and `actor_node(..., logical_width=320)` (`actor_render.hpp`), the race framebuffer and textures in `game_main.cpp`, `RgbImage` 640×480 in `hud.cpp`/`frontend.cpp`. For each: is it a **logical** constant (320×240 view space, must stay) or a **surface** constant (must scale)? (Recorded in REVERSE_ENGINEERING.md "Resolution assumptions": logical = occlusion/ceiling/320 widths, menu boxes, dash-art validation; surface = edge tables, horizon writer, race framebuffers/textures, `display.apply(480)`.)
- [x] **Step 2: Precision check.** Over the Napa sweep views (`course_scene_test`'s 2,509 views), count how many node vertices have non-zero low 15 bits in their Q16 coordinates. Record the result: if most vertices carry sub-pixel bits, higher resolutions gain real geometric precision; if the projection already rounds to whole logical pixels somewhere upstream, record where (address) — higher resolutions then look sharper but keep the original's geometry steps. (Result: 4,807,112/5,392,376 = 89.15 % sub-pixel — real precision gain; the original discards them at `0x00410160`/`0x0040ffc0`.)
- [x] **Step 3:** Find every place the scene build or drawing calls `Race::random()` or another `rand()` consumer during a **render** (the terrain/scenery random-call order in plan 11 Task 3 is the suspect). Record whether scene construction is tied to simulation ticks or to presented frames. Task 7 depends on this answer. (Answer: none — scenery/hazard RNG runs at course setup with an isolated stream; `draw_nodes`/`render_nodes` consume no RNG. Task 7 needs no relocation.)
- [x] **Step 4:** Commit: `docs: resolution assumptions and render-time randomness audit`.

---

### Task 2: Scaled view mapping

**Files:**
- Modify: `src/view.hpp`, `tests/view_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  // ViewMode gains a multiplier; map() becomes floor(q16 * multiplier / 2^coordinate_shift) & mask.
  // view_mode(true) keeps multiplier 1, shift 15, mask 0xfffffffe (original).
  // s = 1 returns view_mode(true) exactly. s = 2..4: surface 640s x 480s, multiplier s, shift 15, mask 0xffffffff.
  ViewMode view_mode_scaled(int s);
  }
  ```

- [x] **Step 1: Write the failing test.** DONE: `tests/view_test.cpp` scaled-mapping block (scale-1 identity over 12 edge values, scale-3 surface + floor(q16·6/65536) + sub-pixel + overflow checks, out-of-range throws).
- [x] **Step 2:** Run `ctest -R view --output-on-failure`. DONE: passes (`view-test`: 0 failures, verified October 13, 2026; implementation already in tree via `cea33a2`).
- [x] **Step 3: Implement.** DONE: `ViewMode::multiplier`, 64-bit `scaled` in `map`, `view_mode_scaled` (s=1 returns `view_mode(true)` exactly; s=2..4 surface 640s×480s, mask `0xffffffff`).
- [x] **Step 4:** Run the test and the whole suite. DONE: view-test green; goldens unchanged (s=1 path identical).
- [x] **Step 5:** Commit: `view: scaled Q16 mapping for higher render resolutions` (landed as part of `cea33a2`; test block verified in-tree October 13, 2026).

---

### Task 3: Raster and texture paths at any surface size

**Files:**
- Modify: `src/texture.hpp`, `src/texture.cpp`, `src/raster.cpp`, `src/world_renderer.cpp`, `tests/texture_test.cpp`, `tests/world_renderer_test.cpp`

**Interfaces:**
- Consumes: `view_mode_scaled` (Task 2).
- Produces:
  - `TextureVertex` x/y become `int32_t` (Review Focus 5); `texture_edge` takes a `std::vector<TextureEdge>&` sized to the surface height, owned by a per-framebuffer `RasterScratch` struct created with the framebuffer (no allocation per quad).
  - `CelTexture` gains `std::vector<CelMip> upscaled;` (index 0 = 2× of mip 0, index 1 = 4×), empty by default. `texture_mip_level` may return a negative level when the projected size exceeds mip 0 and `upscaled` is non-empty; `render_nodes` then samples `upscaled[-level-1]` with uv scaled by `2^(-level)`. With `upscaled` empty, behaviour is exactly today's. Note: original mips sample through `sampling_storage`, which emulates the original span functions' 16-bit byte addressing (`src/cel.hpp`). A 4× level of a 256-wide texture is 1,024 wide and cannot use that path, so upscaled levels get their own textured-span function with plain bounds-checked addressing; original levels keep the emulated path unchanged.

- [x] **Step 1: Write the failing tests.**
  - `texture_test`: a vertex at x = 40,000 and x = −40,000 rasterises (clipped) without wrap; an edge table for a 1,440-row surface fills every row.
  - `world_renderer_test`: a synthetic 16×16 checkerboard texture with a synthetic 32×32 `upscaled[0]` level (each 2×2 block a distinct index, so the test can tell which level was sampled). A quad covering 64×64 surface pixels samples `upscaled[0]`; the same quad at 8×8 pixels samples mip 0 or below; with `upscaled` empty the 64×64 quad samples mip 0.
  - `world_renderer_test`: a flat quad whose corners are Q16 (10.5, 20.25)…(100.75, 90.5) logical pixels covers, at `s = 3`, a pixel count within `perimeter_in_pixels` of `9 ×` the `s = 1` count (edge pixels may round differently; interior must scale exactly).
- [x] **Step 2:** Run; expected FAIL.
- [x] **Step 3: Implement** as specified. Keep the original fill rules (right edge exclusive, bottom inclusive) unchanged at every scale.
- [x] **Step 4:** Run the tests plus `oracle-raster`, `oracle-scene`, `world_renderer`. Expected: PASS, goldens unchanged.
- [x] **Step 5:** Commit: `raster: any surface size; upscaled mip hook`.

---

### Task 4: The race scene at scale

**Files:**
- Create: `tests/hires_render_test.cpp` (CTest `hires-render`; synthetic part always runs, golden part skips 77)
- Modify: the files Task 1 listed as surface constants (expected: `actor_render.cpp`, `terrain_ground.cpp`, sky/horizon drawing and the race framebuffer in `game_main.cpp`), `CMakeLists.txt`

- [x] **Step 1: Write the failing test** `hires_render_test`:
  - **Golden replay at scale.** For each frame of the existing original display-list goldens (`napa-moving` frames 0/150/300/750 and the start frame): render at `s = 1` (must equal the golden exactly, as today) and at `s = 2, 3, 4`. Downsample each `s` image by taking the pixel at `(s·x + s/2, s·y + s/2)` and compare with the `s = 1` image. Require ≥ 97 % equal pixels. Reason for the tolerance: the original snaps vertices to even 640-pixel coordinates; the scaled path keeps sub-pixel positions, so polygon edges move by up to one original pixel. Interiors must match.
  - **Thin geometry (Review Focus 3).** On the far slices (depth > 24 of 33) of the same frames, every lane-marking node visible at `s = 1` covers at least `s` pixels at scale `s`.
  - **Sweep.** All 2,509 Napa sweep views render at `s = 4` with no raster exception.
- [x] **Step 2:** Run; expected FAIL (surface constants still fixed).
- [x] **Step 3: Fix** each surface constant from Task 1's list: sky/horizon rows and scroll offsets are multiplied by `s`; the horizon image is scaled by nearest neighbour until plan 20 supplies an upscaled one; `occlusion` and `logical_width` stay logical but their conversion to surface rows goes through `ViewMode::map`.
- [x] **Step 4:** Run; expected PASS. Render one screenshot per scale with `build/rr-game --assets ../cdrom/ROADRASH --race --render-scale <s> --ticks 900 --autopilot --screenshot analysis/hires-<s>.bmp` (add `--render-scale`) and look at them.
- [x] **Step 5:** Commit: `render: race scene at 2x-4x render scale`.

---

### Task 5: Dashboard, front end and videos at scale

**Files:**
- Modify: `src/hud.hpp/.cpp`, `src/frontend.hpp/.cpp`, `src/text.cpp`, `src/game_main.cpp` (video presentation), `tests/hud_test.cpp`, `tests/frontend_test.cpp`

**Interfaces:**
- Produces: `Hud::compose(const HudState&, const PhysicsTables&, int scale) const` and `Frontend::draw(RgbImage&, int scale) const`. `scale = 1` keeps today's output exactly; the old signatures become wrappers with `scale = 1`.

- [x] **Step 1: Write the failing tests.**
  - `hud_test`: for all 24 style combinations, `compose(..., 1)` equals today's image; `compose(..., 2)` is 1280 wide and its downsample (pixel `(2x+1, 2y+1)`) equals the scale-1 image on ≥ 98 % of pixels (needles and text are re-rasterised at the new size, so their edges differ).
  - `frontend_test`: on the main menu and the Restroom, every item's focus rectangle at scale 2 is exactly twice its scale-1 rectangle, and every text box's ink lies inside its doubled box.
- [x] **Step 2:** Run; expected FAIL.
- [x] **Step 3: Implement.** Artwork (dashboard, menu pictures) is scaled by nearest neighbour here; plan 20 swaps in upscaled art through the same call. Text is rasterised by `TextRenderer` at `height × scale` with the shadow offset `2 × scale`. Needles use the pen from plan 16 Task 6 with width × scale (or the current line with width × scale if Task 6 has not landed). Videos stay at their native size and are scaled on the GPU with the Display screen's filter (plan 20 Task 6 adds shaders).
- [x] **Step 4:** Run; expected PASS. Commit: `hud, frontend: draw at render scale with high-resolution text`.

---

### Task 6: Band-parallel rasterisation

**Files:**
- Create: `src/render_threads.hpp`, `src/render_threads.cpp`
- Modify: `src/world_renderer.hpp/.cpp`, `src/framebuffer.cpp` (`to_rgba` per band), `tests/world_renderer_test.cpp`, `tests/hires_render_test.cpp`, `CMakeLists.txt` (link `Threads::Threads`)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  class RenderThreads {
  public:
      explicit RenderThreads(int threads);          // 1 = run inline, no threads created
      // Calls job(band, top_row, bottom_row_inclusive) for `bands` equal row ranges and waits.
      void run(int bands, int height, const std::function<void(int,int,int)>& job);
  };
  void render_nodes(Framebuffer&, const std::vector<DrawNode>&, const ViewMode&, uint32_t flat_mask, RenderThreads&);
  }
  ```
  Each band calls the existing node loop with `RasterClip{0, top, width, bottom}`. Threads are created once and reused (no thread creation per frame).

- [x] **Step 1: Write the failing test.** For every golden display list and `s = 1..4`, render with 1, 2, 4 and 7 threads (7 makes uneven bands); all images are byte-identical to the 1-thread image (Review Focus 2).
- [x] **Step 2:** Run; expected FAIL (no overload).
- [x] **Step 3: Implement.** Check that `draw_flat_quad` and `draw_textured_quad` honour `clip.top`/`clip.bottom` for every row (fix the raster if a row outside the clip is ever written; the test will show it). Default thread count: `std::min(8, SDL_GetNumLogicalCPUCores() - 1)`, at least 1.
- [x] **Step 4: Measure.** Add `--bench` frame-time output if plan 14 Task 2 has not (p50/p95/max over 3,600 frames). Record on the Mac in VERIFICATION.md: race frame time at `s = 1..4` with 1 thread and with the default. The default render scale "Match display" (Task 8) may not pick a scale whose p95 exceeds 12 ms on this machine; record the rule.
- [x] **Step 5:** Commit: `render: band-parallel rasterisation, bit-identical to one thread`.

---

### Task 7: The simulation does not depend on the render scale

**Files:**
- Modify: `tests/race_soak_test.cpp` (plan 16 Task 7; create a minimal version if that task has not landed), plus whatever Task 1 Step 3 found

- [x] **Step 1:** If Task 1 found render-time `rand()` use, move it to the tick that builds the scene state, keeping the original call order (that is a plan 11 Task 3 rule; coordinate if it is in progress), so drawing extra or fewer frames cannot move the random stream.
- [x] **Step 2: Write the test.** Run a 3,600-tick Napa race with seven opponents twice: once rendering every tick at `s = 1`, once at `s = 4` with frames skipped randomly (render on 50 % of ticks). The per-tick snapshot hashes must be identical.
- [x] **Step 3:** Run; fix until it passes. Commit: `test: race results independent of render scale and frame rate`.

---

### Task 8: Render resolution setting and presentation

**Files:**
- Modify: `src/settings.hpp/.cpp` (`render_scale`), `src/display.hpp/.cpp`, `src/native_screens.cpp`, `src/native_strings.cpp`, `src/game_main.cpp`, `tests/settings_test.cpp`, `tests/display_test.cpp`, `tests/frontend_test.cpp`, `docs/USER_GUIDE.md`

**Interfaces:**
- Consumes: plan 18's `Settings`, `DisplayConfig`, Display screen and native popups.
- Produces: `int render_scale = 1;` in `Settings` (0 = match display, 1–4; key `render_scale`); `int effective_render_scale(const Settings&, int drawable_w, int drawable_h, bool mobile)` in `display.hpp`.

- [x] **Step 1: Write the failing tests.**
  - `settings_test`: `render_scale` round-trips; `render_scale=5` and `=-1` fall back to the default.
  - `display_test`: `effective_render_scale` with setting 0 picks the largest `s ≤ 4` with `640s ≤ drawable_w` and `480s ≤ drawable_h` (2560×1600 → 3; 1920×1080 → 2; 800×600 → 1); a fixed setting is returned unchanged; the original Low or Inset resolution choice (`high_resolution == false`) forces 1. When the render surface is larger than the drawable, `DisplayConfig::texture_scale` is linear for the race texture whatever the filter setting (supersampling, Review Focus 4).
  - `frontend_test`: the Display screen has a RENDER RESOLUTION item with choices "640 × 480 (original)", "1280 × 960", "1920 × 1440", "2560 × 1920", "Match display"; when Graphics Options' Resolution is Low or Inset the item is shown disabled (`item_flag::light`) with a description saying why.
  - Review Focus 1: a scripted race opens the pause menu at tick 300, steps the render scale 1→2→3→4→0, resumes, and the next frame is complete at each step (no black or partial rows) and the tick counter is unchanged.
- [x] **Step 2:** Run; expected FAIL.
- [x] **Step 3: Implement.** The race, menu and video paths query `effective_render_scale` each time the drawable size or the setting changes, and reallocate their framebuffers and SDL textures between frames. The logical presentation becomes `640s × 480s` (510s for the race).
- [x] **Step 4:** Run the tests and the regression gate. USER_GUIDE: the setting, its cost, and that 640 × 480 is the original-exact mode.
- [x] **Step 5:** Commit: `settings: render resolution up to 2560x1920 and match-display`.

---

### Task 9 (decision-gated): Widescreen

The owner asked for widescreen on October 12, 2026. Step 1 (the trace) runs as part of this plan; the implementation is [plan 21](21-enhanced-visuals.md) Task 8, so Step 2 below applies only if the trace shows Hor+ is not feasible. Not part of this plan's exit criteria.

- [x] **Step 1: Trace (time box: one session).** DONE (October 13, 2026): answer in `REVERSE_ENGINEERING.md` ("Plan 19 Task 9" section). Road panels clip against `DAT_0046667c` (320) in `0x0040bde0`; the `0x00443af0` loop has no width test (0x1f+1 strips, fixed order); scenery visibility is bucket + vertical window in `0x00438610`, horizontal cull lives in the `0x0044b630` callback against `DAT_0046667c`; actors cull the same way in `0x0044ba40` (centre `(w&~1)*0x8000`, edges `0 < right && left < w*0x10000`); `DAT_00475618` (448) is a fixed scale constant, not width; registration (`0x00438450`) is depth-windowed. Growing 320→426 only widens early-outs — Hor+ feasible, no rebuild-timing risk. Port: `view_width`, two `160*65536`/`320` hardcodes in `course_scene.cpp`, `actor_render.cpp` already takes `logical_width`.
- [ ] **Step 2: Decide with the owner.** Present: what extends naturally (road, hills), what pops in at the sides (scenery and riders culled by the original), and the HUD layout (centred 4:3 dashboard with side panels). Implement only on an explicit yes, as a separate plan.

## Exit criteria

- The game renders the race, dashboard and menus at 640×480 (bit-identical to today), 1280×960, 1920×1440, 2560×1920 and match-display, chosen in the Display screen and applied live.
- `hires-render`, the band bit-identity test and the render-scale independence test pass in Debug, UBSan and Release trees.
- Measured frame times per scale are in VERIFICATION.md, and the match-display rule respects them.
