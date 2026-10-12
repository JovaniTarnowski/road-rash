# Render infrastructure notes (plans 19/20/21)

Baseline acceptance stays 640x480. Enhanced drawing is out of baseline: with
every upscale/enhancement off, output is bit-identical and all goldens pass.
Reference EXE SHA-256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.

## Baseline precision preserved (plan 19)

- `src/view.hpp`: `view_mode_scaled(1)` returns `view_mode(true)` exactly
  (multiplier 1, shift 15, mask `0xfffffffe`). Scales 2..4 multiply by `s` in
  64 bits before the floor divide and keep the sub-pixel bit. Out-of-range
  scales throw; no giant surface allocations from hand-edited settings.
- `src/texture.*`: the baseline `texture_edge` / `draw_textured_quad` keep the
  original narrow/wrap Q16 arithmetic (`fixed`, `product_q16`, `wrap_*`) and
  the 480-row tables with the fixed 479 bottom clamp. Widening vertex x/y to
  32 bits alone does NOT remove Q16 overflow (`fixed` shifts into a 32-bit
  accumulator and wraps; x=40000 is unrepresentable as signed 32-bit Q16), so
  large geometry must be clipped geometrically with coherent UVs before
  entering the original arithmetic, or take the wide path below.
- New large-size paths use 64-bit edge/intermediate math
  (`TextureVertexWide`, `texture_edge_wide`, `draw_textured_quad_wide`) with
  the same fill rules (right exclusive, bottom inclusive, pre-swap test).
- `world_renderer.cpp` untouched: the explicit i16 narrowing, UV inset ±1 and
  unsigned mip index still feed the baseline path.

## Signed mip selection, scratch, LOD (research rows 19)

- `texture_mip_level_signed` returns negative for upscaled levels above mip 0
  (-1 = first 2x, -2 = 4x) and non-negative for the original index. `size_t`
  cannot represent upscaled levels; UV shifts by a negative are undefined, so
  callers branch on the sign. Original levels keep the 16-bit
  sampling-storage path; upscaled levels (stride may exceed 256) use plain
  bounds-checked addressing with texel-center/inset coordinates per scale,
  never a blindly multiplied ±1 inset.
- Per-worker `RasterScratch` sized to the surface height when the scale
  changes; no allocation per quad, no shared edge arrays.
- LOD-aware scale tests live in `tests/texture_test.cpp` (signed selection,
  1440-row scratch span, ±40000 clipping without wrap, upscaled addressing).
  No universal 97%-at-higher-scale golden is blessed: downsampling enhanced
  geometry changes interiors, not only edges.
- Band contract in `src/raster.hpp` (`band_rows`): original node order within
  each band, edge steps from original endpoints before band clipping, one
  scratch per worker, top/bottom inclusive + right exclusive, textures
  immutable until workers join. `tests/raster_test.cpp` covers 1/2/4/7 bands
  plus a band-crossing polygon. TSan runs separately from ASan/UBSan.
- `effective_render_scale` policy (510-high race window, 480-high menus,
  density, presentation rectangle) is NOT implemented here; left for the
  settings owner.

## MMPX, cache, parser/ZIP validation (research row 20)

- `src/mmpx.*`: MMPX 2x/4x on indexed images from the paper's rule listing
  (McGuire & Gagiu 2021, Listings 3-4), integer arithmetic, deterministic.
  `remap(mmpx(img)) == mmpx(remap(img))` is NOT asserted: arbitrary relabelling
  can change a brightness tie-break. The game upscales index images once, then
  remaps at draw time. Brightness is the 32-bit RGB sum (a general transparent
  colour exceeds 16 bits); index 0 is transparent only when the caller says so
  (flat/opaque textures legitimately contain index 0).
- `tests/mmpx_test.cpp`: palette preservation (200 random images), sizes,
  flat/dither, 4-connected diagonal, silhouette halo, determinism with a
  printed FNV-1a hash ("bump mmpx_version" on change). Reference-output
  validation against the author's outputs is still open.
- `src/upscale_cache.*`: `.rrup` cache keyed by content hash,
  resource/subframe identity, method/version, palette/remap and alpha policy,
  scale. Bounds-checked reads; malformed treated as missing; temp-write then
  rename; memory/disk growth capped; traversal/duplicates/symlinks/inflated
  sizes rejected. Invalid optional entries fall back to original art.
- `unzip_checked` + `import_pack` join the fuzz work: stored + deflate entries
  with bounded inflation, multi-disk rejected. `tests/upscale_cache_test.cpp`
  covers store/find, re-import miss, stale version, AI-vs-MMPX preference,
  every truncation, interrupted stores, pack 1-imported/2-skipped, and ZIP
  rejection cases.
- GPU screen-filter state (`SDL_CreateGPURenderState`) is NOT implemented
  here; it needs a GPU renderer capability check on each backend.

## Projection contracts (research row 21)

- Widescreen centers exactly once: builders center at `view_center_q16(wide)`
  (`src/view.hpp`), `ViewMode::map` adds no origin. Already-built 4:3 lists
  must be rebuilt at the new width, not translated (side actors/scenery were
  culled). Odd width 427 keeps the half pixel in Q16. UI/video layouts stay
  4:3 independently of world culling.
- Equal-depth perspective quads MUST call the original affine
  `draw_textured_quad` (documented in `src/texture.hpp`): float arithmetic and
  span subdivision do not reproduce integer wrap/rounding.
- Draw-distance extension is NOT implemented: the naive reciprocal law
  disagrees on 4297/4351 depth entries and 48/64 slices exceed the 4352-entry
  table. Kept open until an exact generating law is recovered.
- RGBA conversion, continuous LOD/trilinear, fog/grading, post chain: not in
  this slice; their contracts (transparency flags, palette generations, LUT
  applied once, history cleared on pause/reset) belong to the enhancement
  owner.
- Interpolation snapshots need stable actor generation ids (not pool indices),
  camera route/position, and `TickClock::remainder_` units; not implemented
  here.

## Files changed

- `src/view.hpp`: `multiplier`, `view_mode_scaled`, `view_center_q16`.
- `src/texture.hpp/.cpp`: `TextureVertexWide`, `RasterScratch`,
  `texture_edge_wide`, `texture_mip_level_signed`,
  `draw_textured_quad_wide` (baseline path unchanged).
- `src/raster.hpp/.cpp`: `band_rows` (fill path unchanged).
- `src/mmpx.hpp/.cpp`, `src/upscale_cache.hpp/.cpp`: new.
- `tests/view_test.cpp`, `tests/raster_test.cpp`, `tests/texture_test.cpp`:
  extended. `tests/mmpx_test.cpp`, `tests/upscale_cache_test.cpp`: new.
- `CMakeLists.txt`: new sources + tests registered.

## Open questions

1. Reference-output validation of MMPX against the author's algorithm outputs
   (one-pixel diagonals, checkerboards, boundaries with expected patterns).
2. Animation flicker measurement before AI on animated assets (out of slice).
3. Exact depth-table generating law for 48/64 slices (4200+ entries disagree).
4. `effective_render_scale` size-vs-CPU-p95 policy and match-display rule.
5. GPU renderer capability matrix for screen filters/post chain.
