# Review Correctness Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) conventions and [REVIEW-ORCHESTRATION.md](REVIEW-ORCHESTRATION.md) first.

**Goal:** Fix the five confirmed defects from the October 13 code review and remove the dead parts of `rr::Race`, each in its own small, independently reviewable commit.

**Architecture:** Local fixes only. No interface used by another plan changes, except two new free functions (`write_file_atomic`, `clear_rgba_mip_cache`/`palette_generation`) that plans 29 and 30 consume. Every task is independent (wave W1). Tasks 1 and 4 were merged into one task because both edit `run_race`.

**Tech Stack:** C++20 (C++17-compatible code), SDL3, CTest, the repo's no-framework test style.

**Spec:** [../agent-review/CODE-REVIEW-2026-10-13.md](../agent-review/CODE-REVIEW-2026-10-13.md), findings 1–5 and 8.

## Global Constraints

- The simulation must not change: `race-soak` output and the `oracle-sim` diagnostic report are byte-identical before and after every task. Task 6 is **sim-sensitive**.
- No original data in tests; the new tests are synthetic and must always run (no exit 77).
- Every new test file is registered in the `foreach(t ...)` list in `CMakeLists.txt`. The orchestrator applies CMake hunks at merge.
- Each task leaves `cmake --build <dir> && ctest --test-dir <dir>` green in its own build dir.

## Review Focus

1. **A job that throws on several bands at once.** Exactly one exception must reach the caller, and the pool must stay usable afterwards (Task 2 test).
2. **A cache reference held across a clear.** `clear_rgba_mip_cache()` must never run while a frame is being drawn. Task 3 documents and asserts that it runs only from course load.
3. **A save target directory that does not exist, or a stale `.tmp` left by a crash.** The atomic write creates the file or fails without touching the original (Task 5 test).
4. **Restarting while interpolation or the pause menu is active.** After a restart the first frame must show the new race, not a blend with the old one (Task 1).
5. **Removing `Rider::invulnerable_until` breaks a reader outside `src/`.** Task 6 greps `tests/` and `tools/` too.

---

### Task 1: Race-loop regressions: restart after finish, and post-effect reset on resume

**Wave:** W1 · **Owns:** `src/game_main.cpp` · **Profile:** mechanical, one worker (only task in W1 on this file)

**Files:**
- Modify: `src/game_main.cpp` (`Game::restart` near line 243, `run_race` near lines 1047–1170)

**Interfaces:**
- Produces: `void Game::reset_view_history()`, which later plans keep.

- [x] **Step 1: Confirm the defects in the current tree.**
  Run `rg -n "restart = true|if \(restart\)|take_clock_reset" src/game_main.cpp`.
  Expected: three `restart = true` sites, no `if (restart)`, two `take_clock_reset` sites, and only the second one sets `g.post_teleported`.

- [x] **Step 2: Add the view-history reset to `Game`** (next to `post_teleported`):

```cpp
    // Clear interpolation/blur history: the next frame shows the live race
    // without blending across a discontinuity (restart, resume, teleport).
    void reset_view_history() const {
        post_teleported = true;
        view_ready = false;
    }
```
  `view_ready` must become `mutable bool view_ready = false;` to match `post_teleported`.

- [x] **Step 3: Call it from `Game::restart()`.** Add `reset_view_history();` as the last statement, after `countdown = countdown_ticks;`.

- [x] **Step 4: Restore the restart consumer** in `run_race`, immediately after the first `if (!running) break;` following the event loop (the position it had before `aa96b53`):

```cpp
        if (!running) break;
        if (restart) g.restart();   // Enter / pad South / tap after a quick-race finish (README)
```

- [x] **Step 5: Make both clock-reset sites identical.** Add this lambda before the `while (running)` loop:

```cpp
    auto reset_clock = [&](uint64_t now_ns) {
        touch.clear(); clock.reset(); last = now_ns; g.reset_view_history();
    };
```
  In the event loop, replace `{ touch.clear(); clock.reset(); last = SDL_GetTicksNS(); }` with `reset_clock(SDL_GetTicksNS());`. After `const uint64_t now = SDL_GetTicksNS();`, replace the second site's body with `reset_clock(now);`.

- [x] **Step 6: Build and check the warning is gone.**
  Run `cmake --build build-wt-25-1 --target rr-game 2>&1 | rg "unused-but-set|restart"`. Expected: no output. With `-Wall`, `restart` is no longer set-but-unused.

- [x] **Step 7: Headless smoke and full suite.**
  Run `ctest --test-dir build-wt-25-1 -R "first-run-smoke|race-soak" --output-on-failure` (pass), then `build-wt-25-1/rr-game --assets ../cdrom/ROADRASH --race --ticks 600 --autopilot --screenshot /tmp/rr-25-1.bmp; echo $?` (exit 0; skip if there is no disc).

- [x] **Step 8: Manual check (record in the handoff).** Run a quick race (`make run`, pick Thrash), finish it, and press Enter: a new countdown starts. Pause with Escape, wait 2 s and resume with motion blur enabled (Enhancements screen): no ghost of the pre-pause frame. Plan 28 Task 1 adds an automated test of the action mapping.

- [x] **Step 9: Commit.**
```bash
git add src/game_main.cpp
git commit -m "game: restore restart after finish; reset view history on clock reset"
```

---

### Task 2: Render-worker exceptions reach the caller

**Wave:** W1 · **Owns:** `src/render_threads.cpp`, `tests/render_threads_test.cpp` · **Profile:** mechanical

**Files:**
- Modify: `src/render_threads.cpp` (`Impl`, `RenderThreads::run`)
- Create: `tests/render_threads_test.cpp`
- CMake hunk: add `render_threads` to the `foreach(t ...)` test list.

- [x] **Step 1: Write the failing test** `tests/render_threads_test.cpp`:

```cpp
#include "render_threads.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    for (int threads : {1, 2, 4}) {
        rr::RenderThreads pool(threads);
        std::string message;
        try {
            pool.run(4, 100, [](int band, int, int) {
                if (band == 1 || band == 2) throw std::runtime_error("band " + std::to_string(band));
            });
        } catch (const std::runtime_error& e) { message = e.what(); }
        check(message == "band 1" || message == "band 2", "one band error reaches the caller");
        std::atomic<int> ran{0};
        pool.run(4, 100, [&](int, int, int) { ++ran; });
        check(ran == 4, "pool usable after a failed run");
    }
    if (failures == 0) std::cout << "render_threads OK\n";
    return failures ? 1 : 0;
}
```

- [x] **Step 2: Run it and confirm it fails.** Run `cmake --build build-wt-25-2 --target render_threads-test && build-wt-25-2/render_threads-test; echo $?`. Expected: abort (`libc++abi: terminating`, exit 134) at `threads == 2`.

- [x] **Step 3: Implement.** In `Impl` add `#include <exception>` and the member `std::exception_ptr first_error;`. Replace the `if (work) { ... }` block in `loop()`:

```cpp
            if (work) {
                std::exception_ptr error;
                try { work(); } catch (...) { error = std::current_exception(); }
                std::lock_guard<std::mutex> lock(mutex);
                if (error && !first_error) first_error = error;
                if (--pending == 0) done.notify_all();
            }
```
  At the end of `RenderThreads::run`, replace the final wait:

```cpp
    std::unique_lock<std::mutex> lock(impl.mutex);
    impl.done.wait(lock, [&] { return impl.pending == 0; });
    if (impl.first_error) {
        std::exception_ptr error = std::exchange(impl.first_error, nullptr);
        lock.unlock();
        std::rethrow_exception(error);
    }
```
  Add `#include <utility>`.

- [x] **Step 4: Run the test and the render suite.** Run `build-wt-25-2/render_threads-test` (`render_threads OK`), then `ctest --test-dir build-wt-25-2 -R "render_threads|hires-render|raster-rgb|widescreen" --output-on-failure`.

- [x] **Step 5: TSan.** Run `RR_BUILD_DIR=build-wt-25-2-tsan sh tools/tsan.sh` if the script accepts a build dir; otherwise configure `-DRR_TSAN=ON` in `build-wt-25-2-tsan` and run `render_threads-test` there. Expected: no reports.

- [x] **Step 6: Commit.**
```bash
git add src/render_threads.cpp tests/render_threads_test.cpp
git commit -m "render: propagate band-job exceptions instead of terminating"
```

---

### Task 3: Full-colour mip cache: palette generation and invalidation

**Wave:** W1 · **Owns:** `src/raster_rgb.hpp`, `src/raster_rgb.cpp`, `tests/rgba_cache_test.cpp`; one line in `src/game_main.cpp` **applied by the orchestrator after Task 1 merges** · **Profile:** mechanical

**Files:**
- Modify: `src/raster_rgb.hpp`, `src/raster_rgb.cpp:183-213` and `:388`
- Create: `tests/rgba_cache_test.cpp`
- CMake hunk: add `rgba_cache` to the test list.
- Follow-up hunk for `src/game_main.cpp` (orchestrator): the first line of `Game::load_course` becomes `rr::clear_rgba_mip_cache();   // textures below are rebuilt; pointers may be reused`.

**Interfaces:**
- Produces (in `raster_rgb.hpp`):
```cpp
uint32_t palette_generation(const Palette&);   // FNV-1a over rgbx; equal palettes -> equal value
void clear_rgba_mip_cache();                     // only between frames (course load / teardown)
size_t rgba_mip_cache_size();                    // test hook
```

- [x] **Step 1: Write the failing test** `tests/rgba_cache_test.cpp`:

```cpp
#include "raster_rgb.hpp"
#include "cel.hpp"
#include "framebuffer.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    rr::Palette pal{};
    pal.rgbx[4 * 1 + 0] = 255;                       // index 1 = red
    pal.rgbx[4 * 2 + 1] = 255;                       // index 2 = green
    rr::CelTexture tex;
    rr::CelMip mip; mip.width = 1; mip.height = 1; mip.stride_shift = 0; mip.pixels = {1};
    tex.mips.push_back(mip);

    rr::clear_rgba_mip_cache();
    check(rr::rgba_mips(tex, pal, rr::palette_generation(pal))[0].premultiplied[0] == 0xffff0000u, "red");
    // A rebuilt texture at the same address (course reload) must not see stale mips.
    tex.mips[0].pixels = {2};
    rr::clear_rgba_mip_cache();
    check(rr::rgba_mips(tex, pal, rr::palette_generation(pal))[0].premultiplied[0] == 0xff00ff00u, "green after clear");
    // A palette change alone selects a new entry.
    pal.rgbx[4 * 2 + 2] = 255;
    check(rr::rgba_mips(tex, pal, rr::palette_generation(pal))[0].premultiplied[0] == 0xff00ffffu, "palette change");
    check(rr::rgba_mip_cache_size() == 2, "two generations cached");
    rr::Palette same = pal;
    check(rr::palette_generation(same) == rr::palette_generation(pal), "equal palettes, equal generation");
    if (failures == 0) std::cout << "rgba_cache OK\n";
    return failures ? 1 : 0;
}
```

- [x] **Step 2: Run it and confirm it fails to compile** (`palette_generation` is undeclared).

- [x] **Step 3: Implement** in `raster_rgb.cpp` next to `g_cache`:

```cpp
uint32_t palette_generation(const Palette& p) {
    uint32_t h = 2166136261u;
    for (uint8_t b : p.rgbx) { h ^= b; h *= 16777619u; }
    return h;
}
void clear_rgba_mip_cache() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_cache.clear();
}
size_t rgba_mip_cache_size() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_cache.size();
}
```
  These must be defined outside the anonymous namespace. In `render_nodes_rgb_impl`, replace `const uint32_t generation = 0;  // bumped on ...` with `const uint32_t generation = palette_generation(palette);   // per call; content-keyed`. Add the three declarations to `raster_rgb.hpp`, with the comment that `clear_rgba_mip_cache` invalidates references returned by `rgba_mips`.

- [x] **Step 4: Run** `rgba_cache-test` (OK) and `ctest -R "raster-rgb|hires-render|post-chain|fog"`.

- [x] **Step 5: Commit** (the orchestrator then applies the `load_course` hunk in its own commit, `game: clear RGBA mip cache on course load`).
```bash
git add src/raster_rgb.hpp src/raster_rgb.cpp tests/rgba_cache_test.cpp
git commit -m "render: content-keyed palette generation and RGBA mip cache invalidation"
```

---

### Task 4: (merged into Task 1)

---

### Task 5: Atomic file writes for saves, settings and the upscale cache

**Wave:** W1 · **Owns:** `src/atomic_file.hpp`, `src/atomic_file.cpp`, `tests/atomic_file_test.cpp`, `src/settings.cpp` (`save_settings` only), `src/frontend.cpp` (`save_current` only), `src/upscale_cache.cpp` (the store function only) · **Profile:** mechanical

**Files:**
- Create: `src/atomic_file.hpp`, `src/atomic_file.cpp`, `tests/atomic_file_test.cpp`
- Modify: `src/settings.cpp:231-240`, `src/frontend.cpp:844-857`, `src/upscale_cache.cpp:~196-209`
- CMake hunk: add `src/atomic_file.cpp` to `rr-resource`, and `atomic_file` to the test list.

**Interfaces:**
- Produces:
```cpp
namespace rr {
// Writes `path` via `path + ".tmp"`, flush + fsync, then rename over the
// target. Returns false and leaves any existing file untouched on failure.
bool write_file_atomic(const std::filesystem::path& path, const void* data, size_t size);
}
```

- [x] **Step 1: Write the failing test** `tests/atomic_file_test.cpp`:

```cpp
#include "atomic_file.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
namespace fs = std::filesystem;
static std::string slurp(const fs::path& p) { std::ifstream in(p, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    const fs::path dir = fs::temp_directory_path() / ("rr-atomic-" + std::to_string(::getpid()));
    fs::remove_all(dir); fs::create_directories(dir);
    const fs::path f = dir / "save.RRS";
    check(rr::write_file_atomic(f, "first", 5) && slurp(f) == "first", "create");
    check(rr::write_file_atomic(f, "second!", 7) && slurp(f) == "second!", "replace");
    { std::ofstream stale(dir / "save.RRS.tmp"); stale << "garbage"; }
    check(rr::write_file_atomic(f, "third", 5) && slurp(f) == "third", "stale tmp overwritten");
    check(!fs::exists(dir / "save.RRS.tmp"), "no tmp left behind");
    fs::create_directory(dir / "blocked.RRS.tmp");   // temp path cannot be opened as a file
    { std::ofstream orig(dir / "blocked.RRS"); orig << "keep"; }
    check(!rr::write_file_atomic(dir / "blocked.RRS", "lost", 4), "failure reported");
    check(slurp(dir / "blocked.RRS") == "keep", "original untouched on failure");
    check(!rr::write_file_atomic(dir / "missing" / "x.RRS", "x", 1), "missing directory fails");
    fs::remove_all(dir);
    if (failures == 0) std::cout << "atomic_file OK\n";
    return failures ? 1 : 0;
}
```
  Add `#include <unistd.h>` for `getpid`.

- [x] **Step 2: Run it and confirm the link fails** (`write_file_atomic` undefined).

- [x] **Step 3: Implement** `src/atomic_file.cpp`:

```cpp
#include "atomic_file.hpp"
#include <cstdio>
#include <system_error>
#if !defined(_WIN32)
#include <unistd.h>
#endif
namespace rr {
bool write_file_atomic(const std::filesystem::path& path, const void* data, size_t size) {
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    std::FILE* f = std::fopen(tmp.string().c_str(), "wb");
    if (!f) return false;
    bool ok = size == 0 || std::fwrite(data, 1, size, f) == size;
    ok = std::fflush(f) == 0 && ok;
#if !defined(_WIN32)
    ok = ::fsync(::fileno(f)) == 0 && ok;
#endif
    ok = std::fclose(f) == 0 && ok;
    std::error_code ec;
    if (ok) std::filesystem::rename(tmp, path, ec);
    if (!ok || ec) { std::filesystem::remove(tmp, ec); return false; }
    return true;
}
}
```
  The header declares the function, with `#include <filesystem>` and `<cstddef>`.

- [x] **Step 4: Adopt it at the three call sites.**
  - `save_settings`: `const std::string text = format_settings(s); return write_file_atomic(path, text.data(), text.size());`
  - `Frontend::save_current`: replace the `ofstream` block with `if (!write_file_atomic(save_file_, bytes.data(), bytes.size())) throw std::runtime_error("write failed");`
  - `upscale_cache.cpp` store: replace the temp-write/rename block with `if (!write_file_atomic(p, bytes.data(), bytes.size())) return false;`, keeping the surrounding `try`.

- [x] **Step 5: Run** `ctest -R "atomic_file|settings|frontend|career|upscale_cache" --output-on-failure` (frontend may skip with 77 without the disc; report it).

- [x] **Step 6: Commit.**
```bash
git add src/atomic_file.hpp src/atomic_file.cpp tests/atomic_file_test.cpp src/settings.cpp src/frontend.cpp src/upscale_cache.cpp
git commit -m "io: atomic, fsynced writes for saves, settings and the upscale cache"
```

---

### Task 6: Remove dead state and the EXE copy from `rr::Race`

**Wave:** W1 · **Owns:** `src/race.hpp`, `src/race.cpp`, one line of `src/game_main.cpp` (`attack_hit_tick`, **applied by the orchestrator**) · **Profile:** **sim-sensitive**

**Files:**
- Modify: `src/race.hpp` (duplicated `set_demo_hold` comment, `Rider::invulnerable_until`, `attack_hit_tick`, `personalities_exe_`), `src/race.cpp` (ctor near line 127, `ai_can_overtake` near line 1393, dead locals near lines 1498 and 1718, line 1982)

**Interfaces:**
- Unchanged public `Race` constructor signature (plan 26 depends on it).

- [x] **Step 1: Record the baseline** in `analysis/review-25-6/before/`: run `ctest --test-dir build-wt-25-6 -R "race-soak|oracle-sim|race|crash|actor_traffic" -V > before/ctest.txt`, and copy `build-wt-25-6/oracle-diagnostic.json`.

- [x] **Step 2: Check the readers.** Run `rg -n "invulnerable_until|attack_hit_tick\b|personalities_exe_" src tests tools`. Expected: only `race.hpp`, `race.cpp:1982`, `race.cpp:127/1393/1395` and `game_main.cpp:501`.

- [x] **Step 3: Edit.**
  - Delete the first of the two identical 7-line comment blocks above `set_demo_hold`.
  - Delete `int32_t invulnerable_until = 0;` and `target->invulnerable_until = target->hit_until;`.
  - Delete `attack_hit_tick`. The orchestrator changes `game_main.cpp:501` to `rr::attack_legacy_hit_tick` in the same merge.
  - Delete the unused `next`, `void* u; (void)u;` (near line 1498) and the unused `b` (near line 1718).
  - Replace `std::vector<uint8_t> personalities_exe_;` with:
```cpp
    // 0x00465a10: per-level overtake eagerness, read once from the EXE; empty
    // image -> no table (the draw alone decides, as before).
    std::optional<std::array<int32_t,5>> overtake_eagerness_;
```
  - In the ctor initializer, drop `personalities_exe_(exe_image)` and add to the body:
```cpp
    if (!exe_image.empty()) {
        std::array<int32_t,5> t{};
        for (uint32_t i = 0; i < 5; ++i) t[i] = exe_word(exe_image, 0x00465a10 + 4u * i);
        overtake_eagerness_ = t;
    }
```
  - In `ai_can_overtake`: `if (level_ >= 0 && level_ < 5 && overtake_eagerness_) table = (*overtake_eagerness_)[size_t(level_)] * r.bike.i32(0x18);`

- [x] **Step 4: Verify the behaviour is identical.** Re-run Step 1 into `after/`. `diff -r analysis/review-25-6/before analysis/review-25-6/after` must show no differences in test outcomes, and the `oracle-diagnostic.json` signature fields must be equal.

- [x] **Step 5: Commit.**
```bash
git add src/race.hpp src/race.cpp
git commit -m "race: drop dead aliases and locals; keep only the 0x00465a10 table, not the EXE copy"
```


### Task 7: Sanitizer-reported memory errors (session findings, October 13)

**Wave:** W1 · **Owns:** `src/texture.cpp`, `tests/texture_edge_test.cpp`, `tests/widescreen_test.cpp` · **Profile:** mechanical

**Background:** the plan-22 C++20 session ran the sanitizer gate (`build-san`) and found three heap-buffer-overflow reports. All three reproduce on the C++17 tree (verified via a `build-san17` worktree against commit `57e6b3b`), so they pre-date C++20 and are in scope here as confirmed defects, not toolchain fallout. ASan stack excerpts are in the session handoff.

**Defects:**
1. `texture_edge_wide` (`src/texture.cpp:107-120`): `__asan_memcpy` WRITE of size 24 past a 46,080-byte `RasterScratch64` region. The `for (y = 0; y < height; ++y)` sentinel-fill loop writes every row of a scratch sized to the surface height, but the caller in `draw_textured_quad_wide` (`texture.cpp:154`) constructs per-band `side`/`seg` scratches and/or the `memcpy`-style row assignment writes a 24-byte `TextureEdge64` past the vector end when `height` exceeds the scratch size. Reproduce first: the `hires-render` suite aborts under ASan; `race-soak` aborts the same way.
2. Same site, second report (`texture.cpp:120` via `race-soak`): same root cause as (1).
3. `widescreen_test.cpp:80` (`rd.bike.i32(0x28)`): READ of size 4, 40 bytes past a 2,280-byte `Rider` vector region. The test constructs `rr::Race` with the default empty `opponents` vector, so `riders()` holds only the player and `riders()[1]` reads past the end. Test-only OOB (production code indexes valid riders), but the test is wrong and ASan-clean CI requires fixing it: construct the race with one opponent (or guard on `riders().size()`).

**Interfaces:** unchanged. No golden-affecting arithmetic changes; edge math stays bit-identical.

```bash
git add src/texture.cpp tests/texture_edge_test.cpp tests/widescreen_test.cpp
git commit -m "render: bound wide-edge scratch writes; fix widescreen test rider count"
```
