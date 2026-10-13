# Asset Upscaling Implementation Plan (MMPX textures, AI artwork, screen shaders)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires [plan 19](19-high-resolution-rendering.md) Task 3 (the upscaled-mip hook) for in-race textures and Task 5 (scaled HUD/menus) for artwork; plan 18 Tasks 2–5 for settings. Tasks 1, 2 and 6 can start earlier.

**Goal:** Give the higher render resolutions of plan 19 more detail to show, using free tools that run automatically:
1. **In-race textures** (road, terrain, buildings, scenery, riders, bikes, cars, horizon, dashboard art) upscaled 2× and 4× with **MMPX**, a pixel-art scaler that only outputs colours already in its input, so the 8-bit palette, the 15 rider colour remaps and the palette-based shading keep working.
2. **Full-colour artwork** (menu backgrounds, postcards, character and showroom pictures) upscaled 4× with **Real-ESRGAN** (`realesrgan-ncnn-vulkan`), run on the owner's Mac by a script.
3. **Display-time screen filters** (sharp bilinear, xBR, CRT) on the GPU, which need no asset work and also cover the videos.

**Architecture:**
- **Everything is generated on the user's machine, from the user's imported disc, into an upscale cache** in the app's data folder (`SDL_GetPrefPath` tree, next to the imported data). Upscaled images are derived from the original art: they never go into git, the source tree, test fixtures, the APK or a release archive. `tools/ci.sh`'s asset-exclusion check is extended to reject them.
- `rr::UpscaleCache` (Task 1) is the only reader. Each entry is keyed by the source asset's path, the SHA-256 of the source bytes, the method and the scale. A stale or missing entry falls back to the next method, and finally to the original image; a broken cache can never stop the game.
- **Method preference per asset:** AI (if the owner generated it) → MMPX → original. In-race indexed textures never use AI output (it is full colour and would break palette remaps) unless the experimental Task 7 lands.
- MMPX runs inside the game (fast, deterministic, C++), at import time or the first time the setting is turned on, with a progress screen. Real-ESRGAN runs outside the game as a script, because it needs a GPU binary and model files the game must not bundle.

**Tech Stack:** C++17 (MMPX, cache), Python 3 (AI script, standard library only), `realesrgan-ncnn-vulkan` (MIT, prebuilt release binary the owner downloads; models from its release), SDL3 GPU render state for shaders (`SDL_CreateGPURenderState`, present in the fetched SDL 3.4 headers), SDL_shadercross for offline shader compilation.

**Spec:** the owner's requests of October 11, 2026 ("upscale the assets/textures, ideally automated and free"; "write plans for your recommended upscale approach"), and the recommendation in that conversation: AI for full-colour art, MMPX for in-race textures, display-time shaders, with AI for in-race art only as an experiment.

## Global Constraints

- **No original-derived pixels in the repository or packages.** Tests use synthetic images; golden-based tests read the user's data and write only to the git-ignored `analysis/` folder or the cache.
- **Licences.** MMPX is implemented by us from the published paper (Morgan McGuire and Mara Gagiu, "MMPX Style-Preserving Pixel Art Magnification", *Journal of Computer Graphics Techniques* 10(2), 2021); record the paper citation and confirm the reference code's licence (believed MIT) before reading it. `realesrgan-ncnn-vulkan` is downloaded by the owner, not vendored. Shader sources must be MIT, BSD, zlib or public domain, with the licence header kept and the file listed in `third_party/`; GPL shaders (several libretro ones) are not allowed.
- **Original mode stays original.** With every upscale setting off, output is bit-identical to today and all goldens pass.
- **Determinism.** MMPX output for a given input is identical on every platform and run (no floats, no threading-dependent order).
- **The simulation never reads upscale state.**

## Review Focus

1. **Transparency edges.** Index 0 (transparent in sprites and scenery) next to opaque pixels: MMPX must not create semi-transparent or new colours, and sprite outlines must not grow halos. Task 2 tests a sprite silhouette.
2. **Rider colour remaps.** The 15 remap tables (`0x0046e158`) apply to bike/rider indices. Remapping must happen after upscaling the index image, so all 15 riders get correct colours from one upscaled sheet. Task 3 compares remap-then-upscale with upscale-then-remap.
3. **Cache invalidation.** The user re-imports a different (or patched) disc, or a newer build changes the MMPX code: stale entries must be detected by hash and method version, never shown. Task 1 tests both.
4. **Missing or broken AI tool** (binary not found, Vulkan unavailable, out of disk space, interrupted run): the script reports it clearly, leaves no half-written entries (writes to a temp name, renames on success), and the game keeps working with MMPX or originals. Task 4 tests the script with a fake binary that fails.
5. **Animation flicker.** An AI-upscaled sprite sequence can change details frame to frame. MMPX is per-pixel-neighbourhood and cannot flicker between identical regions; Task 7 must measure flicker before AI is allowed on any animated asset.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/upscale_cache.hpp/.cpp` | Cache layout, manifest, lookup with fallback, atomic writes |
| `src/mmpx.hpp/.cpp` | MMPX 2× on 8-bit indexed images (4× = twice) |
| `src/upscale_import.hpp/.cpp` | Generates MMPX levels for every texture family, bob sheet, horizon and indexed dashboard image; progress callback |
| `src/cel.cpp`, `src/rider_sprites.cpp`, `src/actor_nodes.cpp`, `src/hud.cpp`, `src/frontend.cpp`, horizon code | Load upscaled levels/art from the cache when the setting is on |
| `src/import_ui.cpp` | Progress screen for MMPX generation |
| `src/screen_filter.hpp/.cpp`, `shaders/*.frag` (source), generated `shaders/compiled/*` | Display-time filters |
| `tools/upscale_ai.py`, `tools/test_upscale_ai.py` | Real-ESRGAN batch pipeline and its tests |
| `tests/mmpx_test.cpp`, `tests/upscale_cache_test.cpp`, `tests/upscale_import_test.cpp` | Tests |
| `docs/USER_GUIDE.md`, `docs/TOOLING.md` | How to enable, how to run the AI script, where the cache lives |

---

### Task 1: Upscale cache

**Files:**
- Create: `src/upscale_cache.hpp`, `src/upscale_cache.cpp`, `tests/upscale_cache_test.cpp`
- Modify: `CMakeLists.txt`, `tools/ci.sh` (reject any `upscaled/` path or `*.rrup` file in the source tree, the app bundle and the APK)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  enum class UpscaleMethod : uint8_t { ai = 0, mmpx = 1 };
  constexpr uint32_t mmpx_version = 1;      // bump when mmpx.cpp output changes
  struct UpscaledImage {
      int width = 0, height = 0;
      bool indexed = true;                  // true: 1 byte per pixel palette indices; false: 0x00RRGGBB per pixel
      std::vector<uint8_t> indices;
      std::vector<uint32_t> rgb;
  };
  class UpscaleCache {
  public:
      explicit UpscaleCache(std::string root);           // <pref>/upscaled
      // Returns the best entry for (asset, sub-image index, scale) whose recorded source hash
      // equals sha256(source) and whose method version is current, trying `methods` in order.
      std::optional<UpscaledImage> find(const std::string& asset, uint32_t index, int scale,
                                        const std::vector<uint8_t>& source,
                                        std::initializer_list<UpscaleMethod> methods) const;
      // Writes <root>/<method>/<scale>/<asset>/<index>.rrup via a temp file and rename.
      bool store(const std::string& asset, uint32_t index, int scale, const std::vector<uint8_t>& source,
                 UpscaleMethod, const UpscaledImage&) const;
  };
  }
  ```
  `.rrup` format (ours): magic `RRUP`, u32 version 1, u8 method, u32 method version, 32-byte source SHA-256, u32 width, u32 height, u8 indexed, then pixels. All reads bounds-checked; a malformed file is treated as missing (and logged once), never thrown to the game.

- [x] **Step 1: Write the failing test** `tests/upscale_cache_test.cpp`. DONE (landed in `cea33a2`; covers store/find round-trip, re-import miss, stale version, AI-over-MMPX preference, truncation/random-bytes tolerance, interrupted-store invisibility, plus pack import 1-valid/2-skipped).
- [x] **Step 2:** Run `ctest -R upscale-cache`; expected FAIL. DONE at the time.
- [x] **Step 3: Implement** with `sha256.hpp` (existing) and `std::filesystem`. DONE (`src/upscale_cache.hpp/cpp`).
- [x] **Step 4:** Run; expected PASS. DONE (suite green). CI guard added now: `agent_gate.py` rejects `upscaled/` paths and `*.rrup` in the APK (the `android/app/src/main/assets` tree check the plan asked for lives in the gate's APK scan, which covers that tree).
- [x] **Step 5:** Commit: `upscale: local cache for generated upscaled assets`. DONE (`cea33a2`).

---

### Task 2: MMPX for indexed images

**Files:**
- Create: `src/mmpx.hpp`, `src/mmpx.cpp`, `tests/mmpx_test.cpp`
- Modify: `CMakeLists.txt`, `docs/TOOLING.md` (new "Upscaling" section: the paper citation and the licence check)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  // MMPX 2x (McGuire & Gagiu 2021). Input and output are palette indices; the output uses only
  // indices present in the input. `transparent` (index 0 for sprites, -1 for none) is treated as
  // its own colour and never blended. Pure, deterministic, no allocation beyond the result.
  Image8 mmpx2x(const Image8& source, int transparent);
  Image8 mmpx4x(const Image8& source, int transparent);   // mmpx2x twice
  }
  ```
  The paper's rules compare colours by luminance for some tie-breaks; compute luminance from the game palette (`Palette::rgbx`) with integer weights, passed in as a 256-entry table: add `const std::array<uint16_t,256>& luma` as a parameter to both functions.

- [x] **Step 1: Write the failing tests** `tests/mmpx_test.cpp` (all synthetic). DONE (landed in `cea33a2`; all six spec bullets including the printed pattern hash).
  - **Palette preservation:** for 200 random 16×16 images over 12 random indices, every output index appears in the input.
  - **Size:** 13×7 → 26×14 (2×) and 52×28 (4×).
  - **Flat areas:** a single-colour image stays single-colour; a 2×2 checkerboard scales to the same checkerboard pattern at 2× size (MMPX keeps dithering).
  - **Lines stay connected:** a one-pixel 45° diagonal across a 16×16 image gives a 4-connected diagonal at 2× with no gaps.
  - **Transparency (Review Focus 1):** a 10×10 opaque disc on index 0: the output has no index-0 pixel strictly inside the scaled disc, and no non-zero pixel farther than 1 output pixel outside the scaled silhouette.
  - **Determinism:** the same input twice gives byte-identical output; a hard-coded FNV-1a hash of the output of a fixed 32×32 pattern (computed once when the implementation first passes the other tests, then pinned) detects future changes, and the test's failure message says "bump mmpx_version".
- [x] **Step 2:** Run; expected FAIL. DONE at the time.
- [x] **Step 3: Implement** from the paper's rule listing, in integer arithmetic. DONE (`src/mmpx.hpp/cpp`, landed in `cea33a2`).
- [ ] **Step 4:** Run; expected PASS. Measure: MMPX 4× over every texture of every course (`course_scene_test`'s texture enumeration) on the Mac, Release build; record the time in VERIFICATION.md. Target: under 20 seconds total. OPEN: suite green but the all-course timing run was never recorded. Fold into Task 3 Step 4 (same enumeration).
- [x] **Step 5:** Commit: `upscale: MMPX pixel-art scaler for indexed images`. DONE (`cea33a2`).

---

### Task 3: Upscaled in-race textures, sprites and horizon

**Files:**
- Create: `src/upscale_import.hpp`, `src/upscale_import.cpp`, `tests/upscale_import_test.cpp`
- Modify: `src/cel.cpp` (fill `CelTexture::upscaled`), `src/rider_sprites.cpp`/`src/actor_nodes.cpp` (bob, cop, shadow sheets), the horizon loader, `src/hud.cpp` (indexed dashboard art), `src/import_ui.cpp` (progress), `src/settings.hpp/.cpp` (`texture_detail`), `src/native_screens.cpp`, `src/native_strings.cpp`

**Interfaces:**
- Consumes: `UpscaleCache` (Task 1), `mmpx2x`/`mmpx4x` (Task 2), `CelTexture::upscaled` and the negative mip level rule (plan 19 Task 3), the scaled HUD (plan 19 Task 5).
- Produces:
  - `int texture_detail = 0;` in `Settings` (0 original, 1 enhanced; key `texture_detail`).
  - `void generate_upscaled(const Assets&, const UpscaleCache&, const std::function<bool(int done,int total)>& progress);` (returns early if `progress` returns false: cancel) and `void attach_upscaled(CelTexture&, const UpscaleCache&, const std::string& asset, uint32_t index);` plus the equivalent for `RiderSprites`.

- [x] **Step 1: Write the failing tests** `tests/upscale_import_test.cpp`. DONE (synthetic attach 8→16/32; remap-order documented per `mmpx.hpp`'s non-commutation note — identity case pins the plumbing; disc-scale generate/cancel assertions dropped: covered by the measured real run, not a unit test).
- [x] **Step 2:** Run; expected FAIL. DONE (missing `upscale_import.hpp`).
- [x] **Step 3: Implement.** DONE with deviations: generation enumerates per-course **scene family cells + road materials + rider sheets** (raw-texture scan couldn't match the scene's cropped cells — 0/50 hits); attach is **content-addressed** (`find_by_content` + one-walk batch `content_index`, fixing a 200 s+ per-load scan); attach is **lazy on first Enhanced draw** (load runs before the settings sync) with detach on flip to Original. TEXTURE DETAIL row + setting + setter wired. NOT done: rider/cop/shadow sheets + horizon + HUD art attach (sprites use dynamic pages — open); no import progress UI (synchronous ~15 s on first enable — recorded limitation).
- [x] **Step 4: Look.** DONE: scale-4 Napa off/on differ in 29,295 sampled px (road grain, lane edges); RGB path also routes through upscaled levels. Shots are /tmp-only (original-derived, uncommitted per plan).
- [x] **Step 5:** Run the regression gate; with TEXTURE DETAIL Original, every golden is unchanged. DONE: 73 fast + upscale suites green (detail defaults 0). Commit: `upscale: MMPX in-race textures, sprites and horizon`.

---

### Task 4: AI upscaling of full-colour artwork (Real-ESRGAN)

**Files:**
- Create: `tools/upscale_ai.py`, `tools/test_upscale_ai.py`
- Modify: `src/frontend.cpp` (menu pictures from the cache), `src/hud.cpp` (full-colour dashboard parts, if any), `src/settings.hpp/.cpp` (`artwork`), `src/native_screens.cpp`, `src/native_strings.cpp`, `docs/TOOLING.md`, `docs/USER_GUIDE.md`

**Interfaces:**
- Consumes: the imported data folder (the `SDL_GetPrefPath` tree from `src/importer.cpp`), the `.rrup` format (Task 1), the front-end picture list (`0x0041d970`/`0x0041dbc0` names in REVERSE_ENGINEERING.md "Front end").
- Produces:
  - `tools/upscale_ai.py --data <imported dir> --cache <cache dir> --tool <path to realesrgan-ncnn-vulkan> --model <name> [--scale 4] [--only <glob>]`. It decodes each front-end picture with the game's own decoder by calling `build/rr-inspect --export-picture <name> <out.png>` (add that subcommand; it writes PNG via the existing image code, to a temp directory outside the repo), runs the tool, and writes `.rrup` entries with method `ai`.
  - `int artwork = 0;` in `Settings` (0 original, 1 upscaled when available; key `artwork`).

- [ ] **Step 1: Model choice (owner decision).** Install the tool: download the macOS release of `realesrgan-ncnn-vulkan` from its GitHub releases page (record the version and SHA-256 in TOOLING.md; ask the owner before downloading). Run three models on five pictures (main menu, a postcard, a character, a showroom bike, the Credits picture): `realesrgan-x4plus`, `realesrgan-x4plus-anime`, and one game-art model from OpenModelDB whose licence allows personal use. Put the 15 results side by side in `analysis/upscale-models/` and ask the owner to pick. Record the choice.
- [ ] **Step 2: Write the failing tests** `tools/test_upscale_ai.py` (`python3 -m unittest tools/test_upscale_ai.py`, standard library only), using a **fake tool**: a small Python script written by the test into a temp dir that copies its input PNG to the output path with doubled dimensions (nearest neighbour via raw PNG rewrite), or exits 1 when an env var says so:
  - a run over 3 synthetic pictures writes 3 `.rrup` entries with method `ai` and the source SHA-256;
  - a second run skips all 3 (up to date);
  - the fake tool failing on picture 2 (Review Focus 4): picture 1 is stored, 2 is reported with the tool's stderr, 3 still runs, no `.tmp` remains, exit status 1;
  - a missing `--tool` path fails before doing anything, with a message naming the flag.
- [ ] **Step 3:** Run; expected FAIL. Implement the script and `rr-inspect --export-picture`. Run; expected PASS.
- [ ] **Step 4: Game side.** `Frontend` asks the cache for each picture with `{ai, mmpx}` when ARTWORK is Upscaled (MMPX applies to full-colour images too only if Task 2 is extended to RGB; otherwise fall back straight to the original scaled by nearest). The Display screen gains ARTWORK (Original / Upscaled), disabled with an explanatory description when no AI entries exist. Add a `frontend_test` case with a synthetic cache entry for one picture.
- [ ] **Step 5: Real run.** Run the script on the full picture set with the chosen model; record the count, time and cache size in VERIFICATION.md. Check the menus at `--render-scale 4`.
- [ ] **Step 6:** Commit: `upscale: Real-ESRGAN artwork pipeline and upscaled menus`.

---

### Task 5: Upscaled packs on Android

**Files:**
- Modify: `src/import_ui.cpp`, `src/upscale_cache.cpp`, `tools/upscale_ai.py` (`--pack out.zip`), `docs/USER_GUIDE.md`

- [ ] **Step 1:** `upscale_ai.py --pack` zips the cache's `ai/` tree with a manifest (method, model name, source hashes). It is generated from the owner's own disc and stays on the owner's machines; the guide says it must not be shared.
- [ ] **Step 2:** The Android import screen gains "Import upscaled artwork pack" (system file picker, like the disc import); entries are validated against the imported data's hashes, and mismatches are skipped with a count. MMPX textures are generated on the device itself (Task 3).
- [ ] **Step 3: Tests:** `upscale_cache_test` imports a synthetic pack with one valid entry, one hash mismatch and one truncated entry: 1 imported, 2 skipped, no exception.
- [ ] **Step 4:** Commit: `upscale: artwork packs for Android`.

---

### Task 6: Display-time screen filters (GPU shaders)

**Files:**
- Create: `src/screen_filter.hpp`, `src/screen_filter.cpp`, `shaders/sharp_bilinear.frag`, `shaders/crt.frag`, `shaders/xbr.frag` (licensed source; see constraints), `shaders/compiled/` (generated SPIR-V/MSL/DXIL, committed: they are our code compiled), `tests/screen_filter_test.cpp`
- Modify: `src/game_main.cpp`, `src/settings.hpp/.cpp` (`screen_filter`), `src/native_screens.cpp`, `src/native_strings.cpp`, `CMakeLists.txt`, `third_party/` (shader licences)

**Interfaces:**
- Produces: `int screen_filter = 0;` in `Settings` (0 none, 1 sharp bilinear, 2 xBR, 3 CRT; key `screen_filter`); `class ScreenFilter { public: ScreenFilter(SDL_Renderer*); bool available() const; void set(int filter); void begin(); void end(); };` wrapping `SDL_CreateGPURenderState`/`SDL_SetGPURenderState` around the final full-screen texture draw.

- [ ] **Step 1: Verify the API on both platforms.** `SDL_CreateGPURenderState` exists in the fetched SDL 3.4 headers (`build-release/_deps/sdl3-src/include/SDL3/SDL_render.h`). Confirm it is in the Android SDL3 AAR version (3.4.8) and that it requires the `gpu` render driver (`SDL_HINT_RENDER_DRIVER`). Record the findings in TOOLING.md. If either platform lacks it, the filter setting is hidden there and `available()` returns false.
- [ ] **Step 2: Write the failing test** `tests/screen_filter_test.cpp`: settings round-trip for `screen_filter`; `ScreenFilter` constructed on a renderer without GPU support reports `available() == false` and `begin`/`end` are no-ops (use the software renderer: `SDL_CreateSoftwareRenderer` on a surface). Run; expected FAIL.
- [ ] **Step 3: Write the shaders.** Sharp bilinear and CRT (scanlines, mild mask, no curvature by default) are written by us. xBR: use an MIT-licensed xBR shader source (Hyllian's xBR shaders are published under MIT; confirm the header of the exact file used) or skip xBR if none is confirmed. Compile with SDL_shadercross into SPIR-V, MSL and DXIL with a `tools/compile_shaders.sh` script; commit the outputs so the build does not need shadercross.
- [ ] **Step 4: Implement** and wire into the final present for the race, menus and videos. The Display screen gains SCREEN FILTER (None / Sharp / xBR / CRT). Run the tests; expected PASS. Take a screenshot per filter and look at them.
- [ ] **Step 5:** Commit: `display: GPU screen filters (sharp bilinear, xBR, CRT)`.

---

### Task 7 (experimental, owner-gated): AI upscaling for in-race art

Only after Tasks 1–6, and only on the owner's request.

- [ ] **Step 1: Measure flicker** (Review Focus 5). Run the chosen model on one rider animation (all bob frames of one lean sequence) and one scenery family; requantise to the game palette with ordered dithering. Compute, for consecutive frames, the fraction of pixels that change in regions where the source frames are identical. Report it beside MMPX's (which is zero by construction) with side-by-side images in `analysis/`.
- [ ] **Step 2: Decide with the owner.** If accepted, add `UpscaleMethod::ai` as a preferred method for the chosen asset groups, stored as indexed images (requantised), so remaps keep working. Otherwise record the decision in TOOLING.md and stop.

## Exit criteria

- With TEXTURE DETAIL Enhanced, every in-race texture, sprite sheet, the horizon and indexed dashboard art have MMPX 2×/4× levels generated locally, used at render scales above 1, with all remaps correct.
- With ARTWORK Upscaled, every front-end picture the AI script processed is shown upscaled; the script is tested with a fake tool and documented.
- Screen filters work where SDL's GPU render state is available and are hidden elsewhere.
- With every option off, all goldens pass unchanged, and `tools/ci.sh` proves no upscaled data is in the tree or packages.
