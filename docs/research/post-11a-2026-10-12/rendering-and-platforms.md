# Renderer and platform plan review

This is an implementation/test contract audit of plans14–15 and19–23. No new
renderer, dependency installation or platform build was performed here.

## Plan 19: size, precision and raster work

The existing low view is logical320×240. High view maps Q16 through shift15
then clears bit0, forcing even high-resolution vertices. Keep that exact path
for baseline output. Scaled mapping can use a64-bit product and floor division,
but validate scale range explicitly; do not accept negative scales or giant
surface allocations from hand-edited settings.

Current constraints found in source:

| File | Constraint and required handling |
| --- | --- |
| `view.hpp` | Q16 map/floor/mask; logical and surface dimensions are distinct |
| `texture.hpp` | Vertex x/y i16; UV u/v u16; edge table480 entries; mip result `size_t` |
| `texture.cpp` | Two480-row arrays per quad, fixed479 bottom clamp, edge math Q16 in32-bit wrap |
| `world_renderer.cpp` | Explicit i16 narrowing before textured raster; UV inset ±1; mip index unsigned |
| `actor_render.cpp` | Logical width already parameterized, logical horizon90; Q16 screen culling |
| `road.hpp/.cpp` | Fixed logical center160, height240 and33 slices; depth lookup depth/2 |
| `scene.hpp` | `occlusion_row` and hill points are logical/Q16, not high-resolution rows |
| `course_scene.hpp` | Lazy mutable animation cache; build/resolve before parallel raster |
| `actor_nodes.hpp` | Mutable remapped-page cache; prepare before distributing immutable nodes |
| `hud.cpp` |640×480 composition, high artwork/layout only; pen is not yet original-exact |

Widening vertex x/y to32bits is insufficient by itself: `texture.cpp::fixed`
shifts into a32-bit Q16 accumulator and wraps. An x40000 vertex still cannot
be represented as a signed32-bit Q16 value. Split paths or clip geometrically
with coherent UVs before entering the original arithmetic. Preserve original
wrap/narrow behavior at scale1; widening it globally can change goldens with
offscreen geometry. New large-size paths can use64-bit edge/intermediate math.

Plan19's negative mip levels require a signed result or explicit
`{original/upscaled,index,scale}` selection struct. Current `size_t` cannot
represent them safely, and UV shifts by a negative value are undefined. Keep
original16-bit sampling-storage semantics for imported original mips, while
using ordinary checked addressing for larger upscaled levels (stride may
exceed256). Specify texel-center/inset coordinates at each scale instead of
blindly multiplying the ±1 edge inset.

Downsampling enhanced geometry with finer mip selection will change texture
**interiors**, not only edges. Thus97% pixel equality against baseline at a
higher render scale is not a justified universal acceptance threshold.
Separate tests: unchanged scale1 exact replay; nearest-scaled image identity
with geometry/mip frozen; coverage and edge bounds for the precision path;
analytical/pattern texture tests for LOD. Measure higher-scale quality on the
corpus without turning unmeasured97% into a blessed golden.

Band rasterization must retain original node order **within each band**, and
calculate edge steps from the original polygon endpoints before clipping to
the band's rows. Recomputing interpolation from clipped endpoints changes
rounding. Give each worker its own preallocated scratch, sized to height; no
shared edge arrays. Raster writes must honor top/bottom inclusive and right
exclusive. Texture resources remain immutable and alive until workers join.
No SDL render calls, lazy cache mutation, or scene construction on raster
workers. Compare1/2/4/7 workers on synthetic crossing polygons and all available
original lists, then run TSan separately from ASan/UBSan.

`effective_render_scale` must account for the510-high native race window,
480-high menus, device pixel density and the actual presentation rectangle.
The plan's640s×480s fit test and CPU-p95 constraint are two different policies;
define a stable size policy first and apply a measured performance cap only
where data exists. Match-display's example2560×1600→3 is correct for4:3
surfaces, but does not mean arbitrary display dimensions are native rendering.

## Plan 20: MMPX and cache corrections

The author's [MMPX paper, Listing3 and Listing4](https://casual-effects.com/research/McGuire2021PixelArt/McGuire2021PixelArt.pdf)
uses RGB equality and alpha-aware brightness tie-breaks. It preserves the
input color palette, but arbitrary color relabeling can change a tie-break.
Therefore `remap(mmpx(img)) == mmpx(remap(img))` is not generally true even
for a bijective rider remap. It is valid only if comparisons and ordering used
by every rule are preserved. Remove that asserted test requirement.

Options: choose canonical-palette scaling once then remap at draw time (valid
indices, documented shared silhouette), or generate/cache each remapped image
when exact MMPX on each rider color is desired. Do not claim the first is
algorithmically identical to the second. The paper's brightness measure uses
alpha and RGB sum, not standard weighted perceptual luma. A general transparent
color's result can exceed16bits; use32bits and preserve index0 as transparent
only when the texture's flags say it is transparent. Some flat/opaque textures
legitimately contain index0.

Validate MMPX against the author's algorithm/reference outputs, not just a
hash calculated from the implementation under test. One-pixel diagonals,
checkerboards and boundaries need explicit expected patterns. Upscaling frames
independently does not inherently prove zero animation flicker: neighbor changes
can alter the output around otherwise identical source pixels.

Cache keys should include source content hash, resource/subframe/crop identity,
method/version, palette/remap policy, alpha policy and scale. Validate dimensions,
stride, channel format, compression sizes and checksums before allocation.
Cap total memory/cache disk growth; temporary-write then rename; reject path
traversal, duplicate ZIP names, excessive inflated size and symlinks in packs.
An invalid optional upscale entry should fall back to original art without
affecting imported-disc verification. Add synthetic ZIP/cache/fuzz cases.

`SDL_CreateGPURenderState` and `SDL_SetGPURenderState` are available since3.4.0
and apply to the GPU renderer, on its creating thread. Check the actual renderer
name/capability; native default Metal, software and web GLES renderers cannot
be assumed to support this feature. The expected fetched path
`build-release/_deps/sdl3-src` was absent here; inspect the actual owned build
or pinned archive rather than treating the plan's path as evidence.
[SDL creation](https://wiki.libsdl.org/SDL3/SDL_CreateGPURenderState),
[SDL state application](https://wiki.libsdl.org/SDL3/SDL_SetGPURenderState).
Restore previous GPU state after filters; own shader/target lifetimes; compile
and verify the exact backend formats. Hide unavailable effects gracefully.

## Plan 21: enhancements need independent contracts

Interpolation snapshots need stable actor **generation** identifiers, not
traffic pool indices alone (a slot can respawn between ticks). Capture camera
route/position, actor route/ground/roll and a stable animation/render selection.
Use modular shortest-path angle interpolation; pause/reset/background/teleport
must invalidate history. `TickClock::remainder_` is in scaled nanosecond units;
derive alpha from those units, not a nominal rounded16,666,667ns interval.
Render-state snapshots are separate from the exact simulation snapshot.

Perspective interpolation uses `(Σλ·u/z)/(Σλ·1/z)` and similarly forv.
Original projected-depth lookup is not demonstrably simple1/z at all depths;
define the enhanced camera/depth law explicitly, including what clipped/subdivided
vertices represent. Keep billboard depth constant. Equal-depth exact tests
should call the existing affine path rather than expect floating arithmetic
and16-pixel divisions to reproduce all integer wrap/rounding automatically.
Plan21's proposed midpoint formula is not a general barycentric oracle.

RGBA conversion must respect per-texture transparency/flat flags; index0 is
not universally transparent. Key conversion caches by palette/remap generation
and dimensions; RGB filtering must not mutate the original indexed texture.
The existing mip function returns an integer minimum of axis LODs, with no
fractional part. Introduce a separately specified continuous LOD for trilinear,
instead of using a nonexistent fraction. Premultiplication, alpha-test threshold
and whether edge alpha blends or is discarded are enhanced-mode choices.

Depth extension evidence: original `0x00443a00` calls projection with0x21
(33 slices); `read_depth_table` imports4352 values at0x0046f098, indexed by
Q8 depth/2. A naive `min(163840,2^24/depth)` agrees at depth512 but disagrees
on4297 of4351 nonzero-index entries, max error103053; last entry1928. It is
not an exact continuation law.48/64 slices can exceed imported capacity
(6144/8192 indices versus4352). Keep extension open until an exact generating
law is recovered, or make a separately agreed enhanced projection model while
preserving every original entry. Never extrapolate silently from one anchor.

Widescreen needs coordinated center/cull changes: `RoadSlice::screen_x` and
road projection currently hard-code160; actor projection accepts logical width.
If builders center at wide_width/2 **and** ViewMode adds another half-width
origin, the scene shifts twice. Choose one coordinate convention. Already-built
4:3 display lists lack side actors/scenery, so translation alone cannot test
new visibility. At16:9 logical width427, the left offset53.5 needs Q16 precision.
Do not assert a centered crop is exact until clipping/subdivision/LOD and odd
width centering preserve it; test those cases. Restrict native UI/video layouts
to4:3 independently of world culling.

Postprocessing is GPU-backend-dependent and follows the same capability rule
as plan20. Grade is listed both in RGB raster and GPU PostChain; apply it once.
Clear history on pause, restart and renderer reset. Presets require measured
performance, not the speculative1.3–2× or120Hz targets as proof.

## Plans 14 and 15: device and release acceptance

ADB showed no attached devices. No AIR X hardware behavior was observed.
Require an explicit serial in the diagnostic script when more than one device
exists; avoid global `adb logcat -c` which erases unrelated evidence. Capture
app-specific logs/PID and verify launch/import state before taking a screenshot.
Do not fill a user's storage volume to induce ENOSPC. Use a debug injected
space/write-failure provider or bounded disposable fixture with explicit cleanup.
SAF content URIs require SDL_IO support; desktop filesystem checks do not prove
Android import. Preserve debug data before changing signing keys/uninstalling.

Touch visual and hit-test rectangles must use one coordinate system. SDL safe
area is window/client coordinates; touches are normalized and renderer logical
coordinates may include letterboxing. Transform both into the same space and
test nonzero origins, high-DPI,4:3 letterboxes and cutouts.
[SDL safe-area semantics](https://wiki.libsdl.org/SDL3/SDL_GetWindowSafeArea).

Frame interval includes pacing/vsync; CPU render time does not. Report both,
exclude startup/loading from steady-state p95, and record elapsed/tick/present
counts. `FrameStats::dropped_ticks()` in the plan actually counts frames hitting
the20-step cap; rename or count lost simulated time explicitly. Fixed
`SDL_Delay(20)` loops cannot establish60 presented FPS even with a fast raster.

Release plan15 needs an explicit required test/scenario matrix for11/16/17/13/18
and any enabled optional19–23 work. No-asset tests may legitimately skip in
development; final required evidence may not. Verify case counts, complete
trace receipts, real human-driver proof and required fields, not simply absence
of `<skipped>` in JUnit. Synthetic fixture success is not full-device acceptance.

The plan15 `run | tee; grep ACCEPTANCE OK` sample was probed with `false`
inside its function on this host: it exited1 and did not print the success
banner. It was not found broken here. Prefer explicit command status capture
or a shell with `pipefail` for future changes; never reuse an old success log
or convert a command failure to success when displaying a tail. Use unique
logs and an XML parser to select required skipped tests. A fresh clone's tests
must receive the chosen asset/golden paths explicitly; many current CTest
defaults use the repository's sibling `cdrom`, absent beside `/tmp/rr-accept`.

## Plan 22: hardening that tests real code

C++20 does not make signed arithmetic overflow defined. Preserve explicit
wrap and field widths through warning/tidy fixes. ByteReader should reject
unsupported integral/bool decoding rather than bit-casting arbitrary bytes to
invalid bool representation. Do not mass-convert parsers to satisfy stylistic
checks before the corpus and oracle semantics are pinned.

Fuzzer flags must instrument `rr-resource`'s parser objects as well as the
tiny harness. Otherwise coverage and sanitizer checks miss most of the work.
Use a separate instrumented library/tree; don't combine ASan with TSan. Bound
size/count products, aggregate output, nested traversal work and dimensions
before allocation. A512MB RSS ceiling is an emergency stop, not a parser bound.
Limit global EXE/table readers that currently index directly; include the new
MIDS/SBK/ZIP/cache and oracle schema readers as they land.

Existing baseline `oracle-sim`/strict simulation acceptance is not full parity.
Record expected first divergence and scenario coverage before a toolchain
switch, then require no drift; do not require a false all-green baseline or
bless it to proceed. Release acceptance still requires closing those gaps.
Count actual compiler diagnostics by stable(file,check,site), including headers;
parallel builds can duplicate diagnostics. Third-party targets remain outside
project `-Werror`. Pin the actual compiler and available tidy checks.

Official [Clang TSan documentation](https://clang.llvm.org/docs/ThreadSanitizer.html)
lists Darwin ARM64 support. Run callback/import/lifecycle and eventual raster
worker scenarios; a headless invocation which never opens audio does not test
callback races. Suppressions need demonstrated external false positives,
not just an SDL frame somewhere in the stack.

## Plan 23: contracts before the main-loop refactor

A separate web agent owns `cmake/emscripten.cmake`/Task1; those files were only
read. Its CMake draft exists, so the older "no work started" handoff is stale.
Do not reinstall or duplicate its toolchain. Task3's `game_main.cpp` refactor
must wait for coordinated ownership and baseline screenshots.

`AppResult::success` ambiguously means both screen completion and app quit in
the plan's sketch. Specify distinct running/transition/quit-success/quit-failure
results (or an explicit transition object). A finished video/race must execute
its continuation exactly once; cancellation must return to the correct menu.
Inventory all nested frontend→video→race transitions before moving loops.
Fixed-clock headless paths must call the same event/iterate logic, and compare
indexed or canonical RGBA **frames before presentation**. Browser canvas
screenshots can vary with CSS scale/devicePixelRatio/filters; they are not
the strongest native-vs-Wasm renderer comparison.

ImportJob's host needs source byte length, validated request bounds, exact
supplied size, one outstanding request token and cancellation generation. A
late File promise must not supply bytes to a replacement job. The sink's
`write/end_file/commit` cannot be synchronously boolean for asynchronous OPFS
without explicit pending/acknowledged states. Stage under a generation, persist
verified manifest last, promote only after all writes close; quota/cancel/crash
must preserve an earlier valid import. `File.slice` clamps ranges, so never
accept a short read silently. Validate ISO continuation/sector/directory bounds.

AssetBackend also needs asynchronous catalog enumeration (the plan's prefix
preload lacks a list API), explicit loading/failed/ready states, deduplicated
prefetch and callback-on-main-thread lifetime guarantees. Preserve `Assets`
case-insensitive path normalization; the disc uses uppercase paths while
native call sites often use mixed case. OPFS/IDB object keys must canonicalize.
`read()` copies can double memory for a video; expose shared immutable buffers
or account for simultaneous JS/wasm/decoder allocations. Do not promise that
releasing a video makes grown wasm linear memory shrink.

Node's host-file bridge and browser persistence are different builds/modes:
[Emscripten filesystem documentation](https://emscripten.org/docs/api_reference/Filesystem-API.html)
describes NODERAWFS for real Node filesystem access and IDBFS synchronization
for persistent browser files. Read settings/save mount asynchronously before
constructing career state, serialize flushes, and handle quota/flush failure;
don't rely on an asynchronous unload flush. SIMD, threads, exception catching,
test exit status and browser deployment flags must be verified on the pinned
emsdk. No web compile or browser play was performed by this research session.

The synthetic import smoke fixture must include a bootable synthetic asset
set or an explicit import-only harness; a valid tiny ISO/hash table alone
cannot construct the production game's menus. Hash-table substitution must
be impossible in release builds. Imported game data belongs in browser
storage, never in the generated `.data`/wasm/source map/bundle. Check embedded
and compressed content as well as filename/hash lists; keep the original
goldens and samples out of packaging. Publishing remains the user's decision.
