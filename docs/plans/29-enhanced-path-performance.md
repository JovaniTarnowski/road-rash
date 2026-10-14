# Enhanced Render Path Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) conventions and [REVIEW-ORCHESTRATION.md](REVIEW-ORCHESTRATION.md) first.

**Goal:** Make the full-colour (bilinear, fog, upscaled) path and the threaded indexed path allocation-free per frame, and run the full-colour path on the render pool. Output stays bit-identical.

**Architecture:** First a synthetic benchmark and an allocation-counting test harness (Task 1). Then three independent fixes, each proven by "pixels identical, zero allocations after warm-up, time measured": cache upscaled RGBA levels and drop per-node vectors (Task 2), band-thread the RGB path with a single-threaded mip-resolve pre-pass (Task 3), and give worker bands persistent scratch with no per-band `std::function` copies (Task 4).

**Tech Stack:** C++20, `std::thread` pool (`RenderThreads`), CTest, a global `operator new` counter in test binaries.

**Spec:** [../agent-review/CODE-REVIEW-2026-10-13.md](../agent-review/CODE-REVIEW-2026-10-13.md), findings 9–11. Depends on plan 25 Tasks 2–3 (exception propagation, content-keyed cache).

## Global Constraints

- Original rendering (`enhancements` all off) is untouched and stays the fidelity baseline. The indexed path's output must stay byte-identical (`hires-render`, `oracle-raster`, `oracle-scene`).
- RGB-path output is byte-identical to the current single-threaded RGB output for every synthetic scene in the benchmark, at 1, 2, 4 and 7 threads.
- Rendering must not consume simulation randomness or touch `Race` (00-INDEX).
- `clear_rgba_mip_cache()` runs only between frames (plan 25 Task 3); no task may call it from a worker.

## Review Focus

1. **Incremental float edge-walks started at a band's top row.** Starting the walk at the band top instead of the polygon top changes float rounding. Bands must reproduce the one-thread stepping, either by stepping from the polygon's top and discarding rows above the band, or by proving the closed form is identical (Task 3 tests 7 threads on prime heights).
2. **Upscaled-level cache keys.** Base and upscaled levels of one texture must not collide (Task 2 test).
3. **Threads reading the mip cache while a new key is inserted.** `std::map` node references are stable, but the lookup itself needs the lock. Task 3 resolves all mips before dispatch, so workers never touch the cache.
4. **Odd surface sizes** (1366×768 match-display, Hor+ widths): bands with zero rows, and heights smaller than the thread count (test).
5. **An exception in a band** (an unspecialized rider texture) must reach the caller (plan 25 Task 2) and leave the framebuffer usable.

---

### Task 1: Render benchmark and allocation counter

**Wave:** W3 · **Owns:** `tests/bench_render.cpp`, `tests/alloc_counter.hpp`, CMake hunk (`rr-bench-render`, opt-in like `RR_BENCH_WEB`) · **Profile:** mechanical

**Interfaces:**
- Produces:
```cpp
// tests/alloc_counter.hpp: include in exactly one TU per test binary.
namespace rr::test {
inline std::atomic<size_t> allocations{0};
inline std::atomic<bool> counting{false};
struct CountAllocs {               // RAII: counts operator new calls in scope
    CountAllocs() { allocations = 0; counting = true; }
    ~CountAllocs() { counting = false; }
    size_t count() const { return allocations.load(); }
};
}
void* operator new(std::size_t n);   // defined in the header (inline-safe via a guard macro)
void operator delete(void* p) noexcept;
```
  Implementation: `operator new` calls `std::malloc`, increments `allocations` when `counting` is set, and throws `std::bad_alloc` on null. `operator delete` calls `std::free`. Guard the definitions with `#ifdef RR_ALLOC_COUNTER_DEFINE` and define that macro in exactly one TU.
- `rr-bench-render [--threads N] [--scale S] [--frames F]` prints `indexed_ms`, `rgb_ms`, `rgb_fog_ms`, `rgb_upscaled_ms` and a pixel checksum for each path.

- [x] **Step 1:** Build the synthetic scene in `bench_render.cpp`: 400 textured nodes (8×8 and 64×64 mips, half transparent), 100 flat nodes, and 50 textures with one `upscaled` level (dense 2× copy). Spread the nodes over a 640×480 view mapped to `640*S × 480*S`. Use only synthetic data, as in `bench_web.cpp`.
- [x] **Step 2:** Run with `--threads 1 2 4 7` and `--scale 1 2 4`. Record the medians of 5 runs in the handoff; the orchestrator copies them to `VERIFICATION.md` as the baseline.
- [x] **Step 3:** Commit: `bench: synthetic render benchmark and allocation counter`.

---

> Baseline (Apple M1 class, this turn, ms/frame, checksums stable across threads):
> | scale | indexed t1/t2/t4/t7 | rgb t1 | rgb_fog t1 | rgb_upscaled t1 |
> | --- | --- | --- | --- | --- |
> | 1 | 11.9/8.7/6.5/5.7 | 50.0 | 203.3 | 53.8 |
> | 2 | 47.1/47.3/34.2/29.0 | 88.8 | 232.7 | 85.9 |
> | 4 | 118.2/110.2/114.0/93.2 | 168.1 | 291.1 | 161.9 |
> Checksums identical across thread counts on every path (single-threaded RGB ignores the pool).
> Task 3 result: `raster_rgb_threads` byte-identical at 1/2/4/7 threads x 640x480/1366x768/641x479/64x5, fog on/off; ASan + TSan clean. `rgb_ms` t1->t4: 46.8 -> 18.2 (0.39x, beats the 0.5x target); `rgb_fog_ms`: 190.3 -> 64.2. Real bug found: capturing a `thread_local` vector by reference into a worker lambda sees the WORKER's empty instance — `resolved` is now a plain reused static (resolve completes before dispatch; pool mutex orders it), scratch is per-band. Flat/edge `thread_local`s stay (set-then-use on one thread).
> Task 2 result: `raster_rgb_alloc` 0 allocations, checksum `861b81d25eb5f7db` pinned; `rgb_upscaled_ms` 85.9 -> 84.6 (noise); RGB suites 8/8 green. Deviations: edge buffers hoisted to `thread_local` in `draw_rgb_impl` (the 4 vectors x 500 quads = 2000 allocs dominated); `flat_mip` is `thread_local` with per-node `[0] = argb` store (also fixed a dropped flat colour in the first cut).

### Task 2: No per-node allocations in the RGB path

**Wave:** W4 · **Owns:** `src/raster_rgb.cpp`, `src/raster_rgb.hpp`, `tests/raster_rgb_alloc_test.cpp` · **Profile:** careful-mechanical

**Interfaces:**
- Changes the cache key to `(const CelTexture*, uint32_t generation, int level)`: `level = 0` is the base mip chain and `level = k+1` is `upscaled[k]`. Adds the `rgba_mips(const CelTexture&, const Palette&, uint32_t generation, int level)` overload. The existing 3-argument form calls it with `level = 0`.
- `draw_rgb_impl` takes `const RgbaMip* mips, size_t count` instead of `const std::vector<RgbaMip>&`.

- [x] **Step 1: Failing test** `tests/raster_rgb_alloc_test.cpp` (define `RR_ALLOC_COUNTER_DEFINE`). Build the Task 1 scene (copy the builder into a shared `tests/bench_scene.hpp` so both binaries use one definition). Render a warm-up frame with `render_nodes_rgb_fog`, then a second frame inside `rr::test::CountAllocs c;` and check `c.count() == 0`. Also checksum both frames and assert the second equals a frame rendered by the pre-change code. Store the expected checksum as a constant obtained by running the test once on the merge base, and record that in the handoff.
- [x] **Step 2:** Run it; expect a nonzero allocation count.
- [x] **Step 3: Implement.** (a) Upscaled path: replace `rgba_mips_for_test(...)` on the hot path with `rgba_mips(texture, palette, generation, int(up) + 1)`, whose conversion builds the dense `CelMip` exactly as the current lambda does. (b) Flat quads: keep one `RgbaMip flat_mip{1, 1, {0}}` per call outside the loop, set `flat_mip.premultiplied[0] = argb;`, and pass `&flat_mip, 1`. (c) `draw_rgb_impl` takes pointer and count. Leave `rgba_mips_for_test` in place for tests only.
- [x] **Step 4:** Run the alloc test (0 allocations, same checksum) plus `ctest -R "raster-rgb|rgba_cache|fog|post-chain|hires-render"`. Benchmark: record `rgb_upscaled_ms` before and after.
- [x] **Step 5:** Commit: `render: cache upscaled RGBA levels; no per-node allocation in the RGB path`.

---

### Task 3: Band-threaded RGB path

**Wave:** W4 (after Task 2 merges; same file) · **Owns:** `src/raster_rgb.cpp`, `tests/raster_rgb_threads_test.cpp` · **Profile:** careful

- [x] **Step 1: Failing test.** For thread counts {1, 2, 4, 7} and surface sizes {640×480, 1366×768, 641×479, 64×5}, render the bench scene with fog on and off. Every buffer must be byte-identical to the 1-thread result. Use a thread count above the height for the 64×5 case. Add a timing assertion only in the benchmark, never in the test.
- [x] **Step 2: Implement.** (a) Resolve pass, single-threaded, before dispatch: for each node compute everything the current loop computes up to the draw call (mapped points, normalization, flip, fog corners, mip pointer and level), into a reused `std::vector<ResolvedNode>` member of a `thread_local` or caller-owned scratch. Reserve it once; `clear()` per frame. (b) `pool.run(bands, fb.height(), job)`, where each band draws all resolved nodes with `RasterClip{0, top, w-1, bottom}`. (c) Make the edge walk band-invariant: step from the polygon's top row as today, skipping writes outside the clip rows. Do not reseed the walk at the clip top unless a test proves identity. Remove `(void)pool;`.
- [x] **Step 3:** Tests and benchmark (`rgb_ms` at 4 threads ≤ 0.5× the 1-thread time on an M-series Mac is the target, not a gate). ASan and TSan runs of `raster_rgb_threads-test` must be clean.
- [x] **Step 4:** Commit: `render: band-parallel full-colour raster, bit-identical to one thread`.

---

### Task 4: Persistent band scratch in the indexed threaded path

**Wave:** W5 · **Owns:** `src/render_threads.cpp`, `src/render_threads.hpp`, `tests/render_threads_alloc_test.cpp` · **Profile:** careful

- [ ] **Step 1: Failing test:** `render_nodes_threaded` on the bench scene at scale 2 and 4 threads, with zero allocations on the second frame (atomic counter; workers allocate on other threads, which the global counter sees).
- [ ] **Step 2: Implement.** `Impl` keeps a `std::vector<RasterScratch64>` with one entry per band, resized only when the height grows. The queue holds `struct Job { int band, top, bottom; }`, and the shared `const std::function<void(int,int,int)>*` is set once per `run`, so no `std::function` is copied per band. Pass the band's scratch to the job through a new `run` overload, or let `render_nodes_threaded` own per-band scratch via `pool.scratch(band)`. Choose one, document it in the header, and keep the plan 25 exception propagation.
- [ ] **Step 3:** `hires-render` must stay byte-identical (1/2/4/7 threads). Alloc test 0. Record `indexed_ms` before and after.
- [ ] **Step 4:** Commit: `render: persistent band scratch, no per-frame allocation in the pool`.
