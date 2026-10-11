# Foundation and Verification Oracle Implementation Plan

> **Status (October 11, 2026 review):** Tasks 1–6 and 8 done; pixels come from in-process capture at `0x004439f5`, not cnc-ddraw. **Task 7 (deterministic runs) was never implemented**: `run.sh` exports `RR_ORACLE_SEED/INPUT/RECORD` but the DLL ignores them. Replaced by [plan 10](10-simulation-oracle.md). This plan is kept as background for addresses and findings; follow the newer plans for remaining work.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first; its conventions apply to every task.

**Goal:** Put the project under version control, add the shared runtime pieces every later plan needs (asset access, 8-bit framebuffer, golden-file reader), and build a Wine-based oracle that dumps the original game's internal state so native code can be tested against ground truth.

**Architecture:**
- **Native side:** focused modules in `src/` (flat layout, as today), tested by CTest.
- **Oracle side:** a small i686 Windows DLL, built with mingw-w64, that replaces the game's `AWEMAN32.DLL`. It forwards the single imported ordinal to the real DLL and, at load time, patches known call sites in `ROADRASH.EXE` to run logging hooks.
- **Link between them:** the hooks write tab-separated golden files that native tests read.

**Tech Stack:** C++17, SDL3, CMake/CTest, mingw-w64 (i686), Wine Staging 11.18, cnc-ddraw (already installed in the prefix).

**Spec:** [00-INDEX.md](00-INDEX.md) (conventions, definition of done), [../REVERSE_ENGINEERING.md](../REVERSE_ENGINEERING.md) (addresses used below).

## Global Constraints

- The original `ROADRASH.EXE` on disk is never modified. All patching happens in memory, in the oracle process only.
- The oracle runs from a **separate copy** of the install (`prefix/drive_c/ElectronicArts/RoadRashOracle`); the user's playable install stays untouched.
- Golden files live in `native-port/analysis/oracle/` and are git-ignored; tests that need them exit with code 77 (CTest SKIP) when they are absent.
- Every patch site is verified byte-for-byte before writing. On mismatch the DLL logs an error and patches nothing.
- Reference EXE SHA-256: `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.

## Review Focus

1. **Oracle process crash from a bad patch:** verification must refuse to patch if any expected byte differs. The Task 4 test covers this by running against a deliberately modified byte buffer.
2. **Golden files from a different game build or course:** the native test must check the recorded EXE hash and course name and fail loudly on mismatch. Covered in Task 3's reader and Task 6's test.
3. **Missing golden data on a fresh checkout:** tests must SKIP, not fail. Covered in Task 6 with a CTest `SKIP_RETURN_CODE`.
4. **Non-determinism across oracle runs** (time-seeded RNG, keyboard timing): Task 7's two-run comparison must produce byte-identical dumps before any simulation golden test is trusted.
5. **Truncated or partially written golden files** (Wine killed mid-run): the reader must reject a file whose last line lacks the `end` marker. Covered in Task 3.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/assets.hpp/.cpp` | `rr::Assets`: locate and read original files by relative path (desktop directory or Android asset/imported storage) |
| `src/framebuffer.hpp/.cpp` | `rr::Framebuffer`: 8-bit indexed surface plus palette, conversion to RGBA32 |
| `tests/golden.hpp` | Header-only golden-file reader shared by all oracle tests |
| `tests/framebuffer_test.cpp`, `tests/golden_test.cpp`, `tests/assets_test.cpp` | Unit tests |
| `oracle/aweman_proxy.c` | Hook DLL: forwarding, patching, logging |
| `oracle/aweman_proxy.def` | Export definition forwarding ordinal 2 |
| `oracle/build.sh` | Build the DLL with mingw-w64 |
| `oracle/install.sh` | Create the separate oracle install and place the DLL |
| `oracle/run.sh` | Run one oracle session with environment-selected options |
| `tests/oracle_road_test.cpp` | Native vs original road projection comparison |

---

### Task 1: Git baseline

**Files:**
- Modify: `.gitignore`
- Create: repository

**Interfaces:**
- Consumes: nothing
- Produces: a git repository at `native-port/` with a baseline commit that all later tasks commit on top of

- [ ] **Step 1: Extend `.gitignore`**

Append these lines to `/Users/archit/Dev/rr/native-port/.gitignore`:
```
analysis/oracle/
analysis/*.bmp
oracle/build/
.DS_Store
```

- [ ] **Step 2: Check that nothing large or original is staged**

Run: `cd /Users/archit/Dev/rr/native-port && git init && git add -A && git status --short | awk '{print $2}' | xargs -I{} du -k {} | sort -n | tail -5`

Expected:
- The largest entries are `third_party/SDL3-devel-3.4.8-android.zip` and `android/app/libs/SDL3-3.4.8.aar` (both third-party and licensed).
- No `.RSC`, `.CRS`, `.RRI`, `.BOB`, `.EXE` or `.AVI` files. Check with `git status --short | grep -iE '\.(rsc|crs|rri|bob|exe|avi|wav|rra)$'`, which must print nothing.

- [ ] **Step 3: Commit**

```bash
git -C /Users/archit/Dev/rr/native-port commit -m "chore: baseline of native port research

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Asset access and 8-bit framebuffer

The original renders into an 8-bit palettised DirectDraw surface. Rendering natively into the same kind of buffer keeps colour indices, palette tricks (the half-brightness palette built at `0x00448f90`) and pixel-exact comparison with oracle screenshots possible.

**Files:**
- Create: `src/assets.hpp`, `src/assets.cpp`, `src/framebuffer.hpp`, `src/framebuffer.cpp`
- Create: `tests/framebuffer_test.cpp`, `tests/assets_test.cpp`
- Modify: `CMakeLists.txt` (add sources to `rr-resource`, add tests)
- Modify: `src/viewer.cpp` (replace its local `read_file` with `rr::Assets`)

**Interfaces:**
- Produces:
  - `class rr::Assets { public: explicit Assets(std::string root); std::vector<uint8_t> read(const std::string& relative) const; bool exists(const std::string& relative) const; const std::string& root() const; };`
    - Paths use `/`. `read` throws `std::runtime_error` naming the path when the file is missing.
  - `struct rr::Palette { std::array<uint8_t,1024> rgbx; static Palette from_raw(const std::vector<uint8_t>& bytes); };`
    - Construction mirrors `0x00448f90`: entry 0 is forced to black.
  - `class rr::Framebuffer { public: Framebuffer(int width,int height); int width() const; int height() const; uint8_t* row(int y); const uint8_t* row(int y) const; void clear(uint8_t index); void to_rgba(const Palette&, uint8_t* out, int pitch) const; };`

- [ ] **Step 1: Write the failing tests**

`tests/framebuffer_test.cpp`:
```cpp
#include "framebuffer.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    std::vector<uint8_t> raw(1024);
    for (int i = 0; i < 256; ++i) { raw[4*i] = uint8_t(i); raw[4*i+1] = uint8_t(255-i); raw[4*i+2] = 7; raw[4*i+3] = 0; }
    auto pal = rr::Palette::from_raw(raw);
    check(pal.rgbx[0]==0 && pal.rgbx[1]==0 && pal.rgbx[2]==0, "entry 0 forced black (0x00448f90)");
    check(pal.rgbx[4*5]==5 && pal.rgbx[4*5+1]==250 && pal.rgbx[4*5+2]==7, "entries copied");
    try { rr::Palette::from_raw(std::vector<uint8_t>(1023)); check(false, "reject short palette"); } catch (const std::runtime_error&) {}
    rr::Framebuffer fb(4, 2);
    fb.clear(5);
    fb.row(1)[3] = 9;
    std::vector<uint8_t> rgba(4*2*4);
    fb.to_rgba(pal, rgba.data(), 16);
    check(rgba[0]==5 && rgba[1]==250 && rgba[2]==7 && rgba[3]==255, "index 5 -> rgb, opaque");
    check(rgba[16+12]==9 && rgba[16+13]==246, "row pitch honoured");
    try { rr::Framebuffer bad(0, 10); check(false, "reject empty framebuffer"); } catch (const std::runtime_error&) {}
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
```

`tests/assets_test.cpp` (uses a temporary directory, so it always runs):
```cpp
#include "assets.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    auto dir = std::filesystem::temp_directory_path() / "rr-assets-test";
    std::filesystem::create_directories(dir / "DATA");
    { std::ofstream(dir / "DATA" / "X.BIN", std::ios::binary) << "abc"; }
    rr::Assets assets(dir.string());
    check(assets.exists("DATA/X.BIN"), "exists");
    check(!assets.exists("DATA/NOPE.BIN"), "missing");
    auto bytes = assets.read("DATA/X.BIN");
    check(bytes.size()==3 && bytes[0]=='a', "read contents");
    try { assets.read("DATA/NOPE.BIN"); check(false, "throw on missing"); }
    catch (const std::runtime_error& e) { check(std::string(e.what()).find("NOPE.BIN") != std::string::npos, "error names path"); }
    std::filesystem::remove_all(dir);
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
```

Add to `CMakeLists.txt` inside `if(BUILD_TESTING)`:
```cmake
        foreach(t framebuffer assets)
            add_executable(${t}-test tests/${t}_test.cpp)
            target_link_libraries(${t}-test PRIVATE rr-resource SDL3::SDL3)
            add_test(NAME ${t} COMMAND ${t}-test)
        endforeach()
```
Also change the library line to `add_library(rr-resource STATIC src/resource.cpp src/course.cpp src/road.cpp src/assets.cpp src/framebuffer.cpp)` and add `target_link_libraries(rr-resource PUBLIC SDL3::SDL3)`, since `assets.cpp` uses `SDL_LoadFile` so that Android APK assets work.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build --parallel 2>&1 | grep -m3 error`
Expected: compile errors, `framebuffer.hpp` / `assets.hpp` not found.

- [ ] **Step 3: Implement**

`src/framebuffer.hpp`:
```cpp
#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rr {
struct Palette {
    std::array<uint8_t,1024> rgbx{};
    // PALETTE.RAW: 256 x {r,g,b,flags}. 0x00448f90 forces entry 0 to black.
    static Palette from_raw(const std::vector<uint8_t>& bytes);
};

class Framebuffer {
public:
    Framebuffer(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    uint8_t* row(int y) { return pixels_.data() + size_t(y)*width_; }
    const uint8_t* row(int y) const { return pixels_.data() + size_t(y)*width_; }
    void clear(uint8_t index);
    void to_rgba(const Palette& palette, uint8_t* out, int pitch) const;
private:
    int width_, height_;
    std::vector<uint8_t> pixels_;
};
}
```

`src/framebuffer.cpp`:
```cpp
#include "framebuffer.hpp"
#include <algorithm>

namespace rr {
Palette Palette::from_raw(const std::vector<uint8_t>& bytes) {
    if (bytes.size() != 1024) throw std::runtime_error("Palette must be 1024 bytes");
    Palette p;
    std::copy(bytes.begin(), bytes.end(), p.rgbx.begin());
    p.rgbx[0] = p.rgbx[1] = p.rgbx[2] = 0;
    return p;
}

Framebuffer::Framebuffer(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) throw std::runtime_error("Invalid framebuffer size");
    pixels_.assign(size_t(width)*height, 0);
}

void Framebuffer::clear(uint8_t index) { std::fill(pixels_.begin(), pixels_.end(), index); }

void Framebuffer::to_rgba(const Palette& palette, uint8_t* out, int pitch) const {
    for (int y = 0; y < height_; ++y) {
        const uint8_t* src = row(y);
        uint8_t* dst = out + size_t(y)*pitch;
        for (int x = 0; x < width_; ++x) {
            const uint8_t* c = &palette.rgbx[4*src[x]];
            dst[4*x] = c[0]; dst[4*x+1] = c[1]; dst[4*x+2] = c[2]; dst[4*x+3] = 255;
        }
    }
}
}
```

`src/assets.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rr {
// Original game files addressed by path relative to the ROADRASH directory, e.g. "DATA/GLOBAL.RSC".
class Assets {
public:
    explicit Assets(std::string root);
    std::vector<uint8_t> read(const std::string& relative) const;
    bool exists(const std::string& relative) const;
    const std::string& root() const { return root_; }
private:
    std::string root_;
};
}
```

`src/assets.cpp`:
```cpp
#include "assets.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace rr {
Assets::Assets(std::string root) : root_(std::move(root)) {}

std::vector<uint8_t> Assets::read(const std::string& relative) const {
    const std::string path = root_ + "/" + relative;
    size_t size = 0;
    void* bytes = SDL_LoadFile(path.c_str(), &size);
    if (!bytes) throw std::runtime_error(path + ": " + SDL_GetError());
    auto start = static_cast<const uint8_t*>(bytes);
    std::vector<uint8_t> result(start, start + size);
    SDL_free(bytes);
    return result;
}

bool Assets::exists(const std::string& relative) const {
    SDL_IOStream* io = SDL_IOFromFile((root_ + "/" + relative).c_str(), "rb");
    if (!io) return false;
    SDL_CloseIO(io);
    return true;
}
}
```

In `src/viewer.cpp`, delete `read_file`, construct `rr::Assets assets(root);` once in `main`, and replace every `read_file(root+"/X")` with `assets.read("X")`. Pass `assets` into `load_image` in place of `root`.

- [ ] **Step 4: Run all tests and the smoke run**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure && build/rr-native --assets ../cdrom/ROADRASH --smoke | tail -1`
Expected: all tests pass (resource-bounds, road-projection, framebuffer, assets); the smoke run ends with `Decoded MEDLY.CRS: 5634 route samples`.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "core: asset access and 8-bit framebuffer

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Golden-file format and reader

All oracle output uses one line-oriented format so every test reads it the same way.

**Format** (UTF-8 text, tab-separated):
```
# rr-oracle 1
exe	73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2
scenario	<free text, e.g. napa-branchA-auto>
frame	<n>
<key>	<signed decimal integer>
...
frame	<n+1>
...
end
```
- Keys are dotted paths, for example `camera.position`, `slice.3.road.0.screen_y`.
- Each `frame` starts a new record; `end` must be the final line.

**Files:**
- Create: `tests/golden.hpp`, `tests/golden_test.cpp`
- Modify: `CMakeLists.txt` (add `golden` to the test `foreach` list)

**Interfaces:**
- Produces:
  - `struct rr::golden::File { std::string exe, scenario; std::vector<std::map<std::string,int64_t>> frames; };`
  - `rr::golden::File rr::golden::parse(const std::string& text);` throws `std::runtime_error` on missing header, missing `end`, a key before the first frame, or a non-integer value.
  - `rr::golden::File rr::golden::load(const std::string& path);`
  - `constexpr int rr::golden::skip = 77;`
  - `std::string rr::golden::oracle_dir();` returns `$RR_ORACLE_DIR` or, if unset, `<source dir>/analysis/oracle` via a compile definition `RR_SOURCE_DIR`.

- [ ] **Step 1: Write the failing test**

`tests/golden_test.cpp`:
```cpp
#include "golden.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    const std::string ok = "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na.b\t-5\nframe\t1\na.b\t7\nend\n";
    auto f = rr::golden::parse(ok);
    check(f.exe=="abc" && f.scenario=="s" && f.frames.size()==2, "header and frames");
    check(f.frames[0].at("a.b")==-5 && f.frames[1].at("a.b")==7, "values");
    for (const char* bad : {
            "exe\tabc\nframe\t0\nend\n",                               // no magic line
            "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na\t1\n",   // truncated: no end
            "# rr-oracle 1\nexe\tabc\nscenario\ts\na\t1\nend\n",        // key before frame
            "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na\tx\nend\n" }) { // non-integer
        try { rr::golden::parse(bad); check(false, bad); } catch (const std::runtime_error&) {}
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --parallel 2>&1 | grep -m1 golden.hpp`
Expected: `golden.hpp` not found.

- [ ] **Step 3: Implement `tests/golden.hpp`**

```cpp
#pragma once
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rr::golden {
constexpr int skip = 77;
struct File {
    std::string exe, scenario;
    std::vector<std::map<std::string,int64_t>> frames;
};

inline File parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line != "# rr-oracle 1") throw std::runtime_error("Golden file: bad magic");
    File f;
    bool ended = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (ended) throw std::runtime_error("Golden file: data after end");
        if (line == "end") { ended = true; continue; }
        const auto tab = line.find('\t');
        if (tab == std::string::npos) throw std::runtime_error("Golden file: missing tab: " + line);
        const std::string key = line.substr(0, tab), value = line.substr(tab + 1);
        if (key == "exe") { f.exe = value; continue; }
        if (key == "scenario") { f.scenario = value; continue; }
        if (key == "frame") { f.frames.emplace_back(); continue; }
        if (f.frames.empty()) throw std::runtime_error("Golden file: key before frame: " + key);
        char* end = nullptr;
        const long long v = std::strtoll(value.c_str(), &end, 10);
        if (value.empty() || *end) throw std::runtime_error("Golden file: not an integer: " + line);
        f.frames.back()[key] = v;
    }
    if (!ended) throw std::runtime_error("Golden file: truncated (no end marker)");
    return f;
}

inline File load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open golden file " + path);
    std::ostringstream s; s << in.rdbuf();
    return parse(s.str());
}

inline std::string oracle_dir() {
    if (const char* d = std::getenv("RR_ORACLE_DIR")) return d;
    return std::string(RR_SOURCE_DIR) + "/analysis/oracle";
}
}
```

In `CMakeLists.txt`, add after `include(CTest)`:
```cmake
    add_compile_definitions(RR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
```
and add `golden` to the `foreach(t ...)` list from Task 2.

- [ ] **Step 4: Run tests**

Run: `cmake --build build --parallel && ctest --test-dir build -R golden --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "test: golden oracle file format and reader

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Oracle toolchain and forwarding DLL

The game statically imports `AWEMAN32.dll` ordinal 2 (AWE32 sound-font manager). Replacing that DLL with our own, forwarding ordinal 2 to the renamed original, gets our code into the process before `WinMain` runs, with no change to the EXE.

**Files:**
- Create: `oracle/aweman_proxy.c`, `oracle/aweman_proxy.def`, `oracle/build.sh`, `oracle/install.sh`, `oracle/run.sh`
- Create: `oracle/patch_test.c` (host-side test of the patch verifier, compiled with the macOS compiler)

**Interfaces:**
- Produces:
  - `oracle/build/AWEMAN32.DLL`: forwards ordinal 2 to `AWEMAN_REAL.DLL`.
  - C function `int rr_patch_call(uint8_t* site, uint32_t expected_target, void* hook)`, used by later tasks. It verifies that `site[0]==0xE8` and that the rel32 target equals `expected_target`, rewrites the rel32 to point at `hook`, and returns 1 on success or 0 without writing on mismatch.
  - Environment variables read by the DLL: `RR_ORACLE_OUT` (Windows path of the golden file; default `C:\oracle.tsv`), `RR_ORACLE_SCENARIO`, `RR_ORACLE_SEED`, `RR_ORACLE_INPUT` (replay file), `RR_ORACLE_RECORD` (input record file), `RR_ORACLE_SHOT_EVERY`.

- [ ] **Step 1: Install the toolchain**

Run: `brew install mingw-w64 && i686-w64-mingw32-gcc --version | head -1`
Expected: a version line from `i686-w64-mingw32-gcc`.

- [ ] **Step 2: Write the patch-verifier test (host build)**

`oracle/patch_test.c`. The verifier is written in portable C so it can be tested on macOS:
```c
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RR_PATCH_NO_WINDOWS 1
#include "aweman_proxy.c"

int main(void) {
    int failures = 0;
    uint8_t code[16] = {0};
    uintptr_t base = (uintptr_t)code;
    /* call to target 0x1000 bytes ahead of the next instruction */
    uint32_t target = (uint32_t)(base + 5 + 0x1000);
    code[0] = 0xE8; int32_t rel = 0x1000; memcpy(code + 1, &rel, 4);
    uint8_t hook_storage[4];
    if (!rr_patch_call(code, target, hook_storage)) { puts("FAIL patch valid site"); failures++; }
    memcpy(&rel, code + 1, 4);
    if ((uintptr_t)(code + 5) + rel != (uintptr_t)hook_storage) { puts("FAIL rel32 rewritten"); failures++; }
    uint8_t other[5] = {0x90, 0, 0, 0, 0};
    if (rr_patch_call(other, target, hook_storage)) { puts("FAIL accepted non-call"); failures++; }
    if (other[0] != 0x90) { puts("FAIL wrote on mismatch"); failures++; }
    printf("%d failures\n", failures);
    return failures != 0;
}
```

- [ ] **Step 3: Run it to verify it fails**

Run: `cc -m64 -I oracle oracle/patch_test.c -o /tmp/rr_patch_test 2>&1 | head -3`
Expected: `aweman_proxy.c` not found.

- [ ] **Step 4: Write the DLL skeleton**

`oracle/aweman_proxy.def`:
```
LIBRARY AWEMAN32
EXPORTS
    Forward2 = AWEMAN_REAL.#2 @2 NONAME
```

`oracle/aweman_proxy.c`:
```c
/* Oracle hook DLL for ROADRASH.EXE (SHA-256 73d7f974...). Replaces AWEMAN32.DLL. */
#include <stdint.h>
#include <string.h>

/* Verify an E8 rel32 call at site targeting expected_target, then redirect it to hook. */
static int rr_patch_bytes(uint8_t* site, uint32_t expected_target, void* hook) {
    int32_t rel;
    if (site[0] != 0xE8) return 0;
    memcpy(&rel, site + 1, 4);
    if ((uint32_t)((uintptr_t)(site + 5) + rel) != expected_target) return 0;
    rel = (int32_t)((uintptr_t)hook - (uintptr_t)(site + 5));
    memcpy(site + 1, &rel, 4);
    return 1;
}

#ifdef RR_PATCH_NO_WINDOWS
int rr_patch_call(uint8_t* site, uint32_t expected_target, void* hook) { return rr_patch_bytes(site, expected_target, hook); }
#else
#include <windows.h>
#include <stdio.h>

static FILE* g_out;
static char g_scenario[256] = "unnamed";

int rr_patch_call(uint8_t* site, uint32_t expected_target, void* hook) {
    DWORD old;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    int ok = rr_patch_bytes(site, expected_target, hook);
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return ok;
}

static void env(const char* name, char* buf, DWORD size, const char* fallback) {
    if (!GetEnvironmentVariableA(name, buf, size)) { strncpy(buf, fallback, size - 1); buf[size - 1] = 0; }
}

void rr_log(const char* key, long long value) { if (g_out) fprintf(g_out, "%s\t%lld\n", key, value); }
void rr_logf(const char* fmt, ...);  /* defined in later tasks if needed */

static void rr_install_hooks(void) { /* Tasks 5 and 7 add patches here. */ }

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)inst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        env("RR_ORACLE_OUT", path, sizeof path, "C:\\oracle.tsv");
        env("RR_ORACLE_SCENARIO", g_scenario, sizeof g_scenario, "unnamed");
        g_out = fopen(path, "w");
        if (g_out) {
            setvbuf(g_out, NULL, _IOFBF, 1 << 20);
            fprintf(g_out, "# rr-oracle 1\nexe\t73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2\nscenario\t%s\n", g_scenario);
        }
        rr_install_hooks();
    } else if (reason == DLL_PROCESS_DETACH && g_out) {
        fputs("end\n", g_out);
        fclose(g_out);
        g_out = NULL;
    }
    return TRUE;
}
#endif
```
Remove the `rr_logf` declaration if no later task defines it.

`oracle/build.sh`:
```sh
#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
i686-w64-mingw32-gcc -O2 -Wall -shared -static-libgcc -o build/AWEMAN32.DLL aweman_proxy.c aweman_proxy.def -Wl,--enable-stdcall-fixup
echo "built $(pwd)/build/AWEMAN32.DLL"
```

`oracle/install.sh`:
```sh
#!/bin/sh
# Create a separate oracle install so the user's playable install is never touched.
set -eu
ROOT=/Users/archit/Dev/rr
SRC="$ROOT/prefix/drive_c/ElectronicArts/RoadRash"
DST="$ROOT/prefix/drive_c/ElectronicArts/RoadRashOracle"
[ -d "$DST" ] || cp -R "$SRC" "$DST"
[ -f "$DST/AWEMAN_REAL.DLL" ] || cp "$SRC/AWEMAN32.DLL" "$DST/AWEMAN_REAL.DLL"
cp "$(dirname "$0")/build/AWEMAN32.DLL" "$DST/AWEMAN32.DLL"
shasum -a 256 "$DST/ROADRASH.EXE" | grep -q 73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2
mkdir -p "$DST/Screenshots"
echo "oracle install ready at $DST"
```

`oracle/run.sh`:
```sh
#!/bin/sh
# Usage: oracle/run.sh <scenario-name> [seconds]
# Runs the original game with hooks; writes analysis/oracle/<scenario>.tsv
set -eu
ROOT=/Users/archit/Dev/rr
NAME="$1"; SECS="${2:-60}"
WINE="$HOME/Applications/Wine Staging.app/Contents/Resources/wine/bin/wine"
export WINEPREFIX="$ROOT/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="ddraw=n,b;aweman32=n"
export RR_ORACLE_OUT="C:\\oracle-$NAME.tsv" RR_ORACLE_SCENARIO="$NAME"
[ -d "$ROOT/cdrom/ROADRASH" ] || hdiutil attach -readonly -nobrowse -mountpoint "$ROOT/cdrom" "$ROOT/Road Rash (USA) (En,Fr,De,Es,It) (Rerelease) (19970930).iso" >/dev/null
ln -sfn "$ROOT/cdrom" "$WINEPREFIX/dosdevices/d:"
cd "$WINEPREFIX/drive_c/ElectronicArts/RoadRashOracle"
"$WINE" 'C:\ElectronicArts\RoadRashOracle\ROADRASH.EXE' &
PID=$!
sleep "$SECS"
kill "$PID" 2>/dev/null || true
mkdir -p "$ROOT/native-port/analysis/oracle"
cp "$WINEPREFIX/drive_c/oracle-$NAME.tsv" "$ROOT/native-port/analysis/oracle/$NAME.tsv"
echo "wrote analysis/oracle/$NAME.tsv"
```
**Important:** killing Wine skips `DLL_PROCESS_DETACH`, so the file would lack `end`. Task 5 adds a clean-exit mechanism. Until then, runs must be ended by quitting the game from its menu.

- [ ] **Step 5: Run the host test**

Run: `cc -I oracle oracle/patch_test.c -o /tmp/rr_patch_test && /tmp/rr_patch_test`
Expected: `0 failures`.

- [ ] **Step 6: Build, install, and smoke-run**

Run: `chmod +x oracle/*.sh && oracle/build.sh && oracle/install.sh && oracle/run.sh smoke 30`
Expected: the game window opens and plays its intro; after 30 s the script copies `analysis/oracle/smoke.tsv`, whose first three lines are the header (magic, exe, scenario).

If the game fails to start with the proxy:
1. Run once with `WINEDEBUG=+loaddll` to confirm `AWEMAN_REAL.DLL` loads.
2. If mingw's `.def` forwarding by ordinal is rejected, replace it with a runtime trampoline: in `DllMain`, `LoadLibraryA("AWEMAN_REAL.DLL")`, then `GetProcAddress(h, MAKEINTRESOURCEA(2))`, and export a naked function `Forward2` that does `jmp [g_real2]`:
   ```c
   __attribute__((naked)) void Forward2(void) { __asm__("jmp *%0" :: "m"(g_real2)); }
   ```
   Change the `.def` to `Forward2 @2 NONAME`.

- [ ] **Step 7: Commit**

```bash
git add oracle && git commit -m "oracle: AWEMAN32 forwarding hook DLL, install and run scripts

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Frame hook dumping projected road slices

The frame routine `0x004439f0` (calls `0x00443a00`, `0x00450420`, `0x0044c780`) is called from four sites, all `E8 rel32`:

| Site | Bytes |
| --- | --- |
| `0x004163fa` | `e8 f1 d5 02 00` |
| `0x00416406` | `e8 e5 d5 02 00` |
| `0x0041656d` | `e8 7e d4 02 00` |
| `0x00443249` | `e8 a2 07 00 00` |

After it returns, the projected-slice array at `0x004bc800` holds 33 entries (stride `0x89` dwords = 548 bytes), filled by `0x00443d70`.

Entry fields (dword index):

| Index | Field |
| --- | --- |
| `[0]` | Ring slice pointer |
| `[1]` | Kind: 1 single, 2 dual |
| `[2]` | Sample index |
| `[4]` | Clip row |
| `[5]` | Pointer to projected road struct, half 0 |
| `[6]` | Pointer to projected road struct, half 1 |

Projected road struct (the `param_2` of `0x00444630`):

| Offset | Field |
| --- | --- |
| `+0x0c` | Sub-slice pointer (type byte at sub-slice `+0x3c`) |
| `+0x10` | Lateral x |
| `+0x14` | Height y |
| `+0x18` | Depth |
| `+0x20` | Scale |
| `+0x48` | Centre screen x (Q16) |
| `+0x4c` | Surface screen y (Q16) |
| `+0x50`, `+0x58` | Exact road edges x |
| `+0x40`, `+0x44` | Lane-rounded edges x |
| `+0x60`, `+0x68` | Shoulder edges x |
| `+0x54`, `+0x5c` | Shoulder-top rows y (type 0) |
| `+0x100` | Visible byte |

Camera globals:

| Address | Field |
| --- | --- |
| `0x004bc7e4` | Position (Q8 samples) |
| `0x004bc7d8` | x |
| `0x004bc7dc` | Height |
| `0x00473498` | Camera road half (byte) |

**Files:**
- Modify: `oracle/aweman_proxy.c`

**Interfaces:**
- Consumes: `rr_patch_call`, `rr_log` (Task 4)
- Produces golden keys per frame:
  - `site`, `camera.position`, `camera.x`, `camera.height`, `camera.half`
  - For `s` in 0..32:
    - `slice.<s>.kind`, `slice.<s>.sample`
    - For `r` in 0..kind−1: `slice.<s>.road.<r>.{type,depth,scale,x,y,centre,screen_y,left,right,lane_left,lane_right,shoulder_left,shoulder_right,height_left_row,height_right_row,visible}`

- [ ] **Step 1: Add the frame hook**

Insert into `aweman_proxy.c` above `rr_install_hooks`:
```c
typedef uint32_t (__cdecl *rr_void_fn)(void);
static rr_void_fn const orig_frame = (rr_void_fn)0x004439f0;
static uint32_t g_frame, g_site;
static int g_quit_after = -1;

#define RD32(a) (*(volatile int32_t*)(uintptr_t)(a))
#define RD8(a)  (*(volatile uint8_t*)(uintptr_t)(a))

static void dump_frame(void) {
    char key[96];
    fprintf(g_out, "frame\t%u\n", g_frame);
    rr_log("site", g_site);
    rr_log("camera.position", RD32(0x004bc7e4));
    rr_log("camera.x", RD32(0x004bc7d8));
    rr_log("camera.height", RD32(0x004bc7dc));
    rr_log("camera.half", RD8(0x00473498));
    for (int s = 0; s < 33; ++s) {
        uintptr_t e = 0x004bc800 + (uintptr_t)s * 0x89 * 4;
        int kind = RD32(e + 4);
        snprintf(key, sizeof key, "slice.%d.kind", s); rr_log(key, kind);
        snprintf(key, sizeof key, "slice.%d.sample", s); rr_log(key, RD32(e + 8));
        for (int r = 0; r < kind && r < 2; ++r) {
            uintptr_t p = (uintptr_t)RD32(e + (5 + r) * 4);
            if (!p) continue;
            static const struct { const char* name; int off; int byte; } f[] = {
                {"depth",0x18,0},{"scale",0x20,0},{"x",0x10,0},{"y",0x14,0},{"centre",0x48,0},
                {"screen_y",0x4c,0},{"left",0x50,0},{"right",0x58,0},{"lane_left",0x40,0},{"lane_right",0x44,0},
                {"shoulder_left",0x60,0},{"shoulder_right",0x68,0},{"height_left_row",0x54,0},
                {"height_right_row",0x5c,0},{"visible",0x100,1}};
            for (unsigned i = 0; i < sizeof f / sizeof f[0]; ++i) {
                snprintf(key, sizeof key, "slice.%d.road.%d.%s", s, r, f[i].name);
                rr_log(key, f[i].byte ? RD8(p + f[i].off) : RD32(p + f[i].off));
            }
            uintptr_t sub = (uintptr_t)RD32(p + 0x0c);
            snprintf(key, sizeof key, "slice.%d.road.%d.type", s, r);
            rr_log(key, sub ? RD8(sub + 0x3c) : -1);
        }
    }
}

#define FRAME_HOOK(n, site) \
    static uint32_t __cdecl hook_frame_##n(void) { \
        uint32_t result = orig_frame(); \
        g_site = site; if (g_out) dump_frame(); \
        ++g_frame; \
        if (g_quit_after >= 0 && (int)g_frame >= g_quit_after && g_out) { fputs("end\n", g_out); fclose(g_out); g_out = NULL; ExitProcess(0); } \
        return result; }
FRAME_HOOK(0, 0x004163fa) FRAME_HOOK(1, 0x00416406) FRAME_HOOK(2, 0x0041656d) FRAME_HOOK(3, 0x00443249)
```

Replace `rr_install_hooks` with:
```c
static void rr_install_hooks(void) {
    char buf[32];
    if (GetEnvironmentVariableA("RR_ORACLE_FRAMES", buf, sizeof buf)) g_quit_after = atoi(buf);
    struct { uint32_t site; void* hook; } calls[] = {
        {0x004163fa, (void*)hook_frame_0}, {0x00416406, (void*)hook_frame_1},
        {0x0041656d, (void*)hook_frame_2}, {0x00443249, (void*)hook_frame_3}};
    for (unsigned i = 0; i < 4; ++i)
        if (!rr_patch_call((uint8_t*)(uintptr_t)calls[i].site, 0x004439f0, calls[i].hook))
            fprintf(g_out, "# patch failed at %08x\n", calls[i].site);
}
```
Add `#include <stdlib.h>` at the top. In `run.sh`, pass `RR_ORACLE_FRAMES` through (`export RR_ORACLE_FRAMES="${RR_ORACLE_FRAMES:-}"`). Change the `sleep`/`kill` block to `wait "$PID"` whenever `RR_ORACLE_FRAMES` is set, so the file always ends cleanly.

- [ ] **Step 2: Build and capture a first road scenario**

Run:
```sh
oracle/build.sh && oracle/install.sh
RR_ORACLE_FRAMES=2000 oracle/run.sh napa-first-race
```
While it runs, start a race on Napa from the menus (keyboard). The run exits by itself after 2,000 frames.

Expected:
- `analysis/oracle/napa-first-race.tsv` ends with `end`.
- It contains no `# patch failed` lines.
- Frames recorded during the race have `slice.0.kind` of 1 or 2 and increasing `camera.position`.

Check with:
```sh
grep -c '^frame' analysis/oracle/napa-first-race.tsv
grep 'camera.position' analysis/oracle/napa-first-race.tsv | awk '{print $2}' | sort -n | tail -1
tail -1 analysis/oracle/napa-first-race.tsv
```

- [ ] **Step 3: Commit**

```bash
git add oracle && git commit -m "oracle: frame hook dumping camera and projected road slices

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Native road projection versus the original

**Files:**
- Create: `tests/oracle_road_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `rr::golden::load/oracle_dir/skip` (Task 3), `rr::read_course`, `rr::make_route`, `rr::project_road`, `rr::read_depth_table` (existing `src/road.hpp`, `src/course.hpp`)
- Produces: CTest `oracle-road`, which compares every captured race frame. Mismatches are reported per field with the first failing frame. Plan 02 relies on this test to prove rendering fidelity.

Notes on mapping between the dump and native values:
- The native `Camera{x,height}` is relative to the route's own road at the camera sample; the dump's `camera.x` is absolute. Use `camera.x - route[position>>8].offset`.
- The native `RoadSlice::road[r].screen_y` corresponds to dump `screen_y`. Native `screen_x(r, left)` corresponds to dump `left`, `screen_x(r, right)` to `right`, and `screen_x(r, shoulder_left/right)` to `shoulder_left/right`.
- The dump does not say which branch the route took. Try both `make_route(course,false)` and `make_route(course,true)` and use the one whose `sample` fields match; fail if neither does.

- [ ] **Step 1: Write the test**

```cpp
#include "golden.hpp"
#include "road.hpp"
#include "assets.hpp"
#include <iostream>

int main(int argc, char** argv) {
    const std::string game = argc > 1 ? argv[1] : std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    const std::string file = rr::golden::oracle_dir() + "/napa-first-race.tsv";
    rr::golden::File g;
    try { g = rr::golden::load(file); } catch (const std::exception& e) { std::cerr << "SKIP " << e.what() << '\n'; return rr::golden::skip; }
    if (g.exe != "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2") { std::cerr << "golden from other exe\n"; return 1; }
    rr::Assets assets(game);
    auto table = rr::read_depth_table(assets.read("ROADRASH.EXE"));
    auto course = rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
    const auto routes = std::array{rr::make_route(course, false), rr::make_route(course, true)};
    size_t compared = 0, mismatches = 0;
    for (size_t f = 0; f < g.frames.size(); ++f) {
        const auto& fr = g.frames[f];
        if (!fr.count("slice.0.kind") || fr.at("slice.0.kind") < 1) continue;
        const uint32_t position = uint32_t(fr.at("camera.position"));
        for (const auto& route : routes) {
            if ((position >> 8) >= route.size()) continue;
            if (uint32_t(fr.at("slice.0.sample")) != (position >> 8)) continue;
            rr::Camera cam{int32_t(fr.at("camera.x")) - route[position >> 8].offset, int32_t(fr.at("camera.height"))};
            auto slices = rr::project_road(route, position, cam, table);
            bool ok = true;
            auto expect = [&](const std::string& key, int64_t native) {
                auto it = fr.find(key);
                if (it == fr.end() || it->second == native) return;
                if (ok && mismatches < 20) std::cerr << "frame " << f << " " << key << " original " << it->second << " native " << native << '\n';
                ok = false;
            };
            for (size_t s = 0; s < slices.size(); ++s) {
                const auto p = "slice." + std::to_string(s) + ".";
                expect(p + "kind", slices[s].count);
                for (int r = 0; r < slices[s].count; ++r) {
                    const auto q = p + "road." + std::to_string(r) + ".";
                    expect(q + "screen_y", slices[s].road[r].screen_y);
                    expect(q + "left", slices[s].screen_x(r, slices[s].road[r].left));
                    expect(q + "right", slices[s].screen_x(r, slices[s].road[r].right));
                    expect(q + "shoulder_left", slices[s].screen_x(r, slices[s].road[r].shoulder_left));
                    expect(q + "shoulder_right", slices[s].screen_x(r, slices[s].road[r].shoulder_right));
                }
            }
            ++compared; if (!ok) ++mismatches;
            break;
        }
    }
    std::cout << compared << " frames compared, " << mismatches << " mismatching\n";
    if (!compared) { std::cerr << "no race frames matched either route\n"; return 1; }
    return mismatches ? 1 : 0;
}
```
Add `#include <array>` at the top.

In `CMakeLists.txt` (inside `BUILD_TESTING`):
```cmake
        add_executable(oracle-road-test tests/oracle_road_test.cpp)
        target_link_libraries(oracle-road-test PRIVATE rr-resource)
        add_test(NAME oracle-road COMMAND oracle-road-test)
        set_tests_properties(oracle-road PROPERTIES SKIP_RETURN_CODE 77)
```

- [ ] **Step 2: Run it**

Run: `cmake --build build --parallel && ctest --test-dir build -R oracle-road --output-on-failure`

Expected: either PASS, or a listing of the first mismatching fields. If it SKIPs, Task 5 was not completed.

- [ ] **Step 3: Triage mismatches without changing test expectations**

For each mismatching field:
1. Pick the first mismatching frame and compare `camera.*` and `slice.0.sample` to be sure the same position is being compared.
2. Work out whether the gap is:
   - **(a)** camera mapping in the test,
   - **(b)** a porting bug in `src/road.cpp` or `src/course.cpp`, or
   - **(c)** a known documented simplification: the rider-commit rule, the stale half-1 bend, or the merge-snapshot lane width.
3. Fix (a) and (b) in place.
4. For (c), record each deviation with its frame numbers in `docs/REVERSE_ENGINEERING.md` under "Splits and merges recovered → Preview simplifications", and exclude only those frames. The exclusion must be keyed on `slice.N.kind==2` frames following a split, and never on a blanket tolerance.

Rerun until PASS.

- [ ] **Step 4: Update docs and commit**

Add to `docs/VERIFICATION.md` the frames compared, the mismatches found and how each was resolved. Then:
```bash
git add -A && git commit -m "test: compare native road projection against original oracle dumps

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Deterministic runs: tick hook, RNG seed, input record/replay

Later plans compare simulation state tick by tick, which needs identical inputs and randomness on every run.

Facts:
- The race loop `0x004163d0` calls `0x00442e10(tick_count)` at `0x00416560`. That calls `0x0044b2e0(count)`, which runs `count` single simulation steps: `0x0044b3b0()`, a void function with no arguments, called at `0x0044b324` (`e8 87 00 00 00`). Each step increments the tick counter `0x004b7ecc`. The camera is updated once afterwards by `0x00438450` (called at `0x0044b39d`).
- CRT `srand` is `0x00455ea0`, called once at `0x00448224` (`e8 77 dc 00 00`).
- `rand` is `0x00455eb0` (MSVC LCG: `seed = seed*0x343fd + 0x269ec3`, return `(seed>>16)&0x7fff`).
- The game reads the keyboard with `USER32!GetKeyboardState` and joysticks with `WINMM!joyGetPosEx` (IAT imports).
- The player bike struct pointer is `*(uint32_t*)0x004642d8`. Known fields: `+0x24` height, `+0x138` position (Q8).

**Files:**
- Modify: `oracle/aweman_proxy.c`, `oracle/run.sh`
- Create: `oracle/determinism.sh`

**Interfaces:**
- Produces golden keys:
  - `tick.index` (hook-local step count)
  - `tick.counter` (the game's tick counter `0x004b7ecc`)
  - `player.raw.<k>` for `k` in 0..127: dwords of the player bike struct, as raw data for plan 04
  - `rng.calls` (number of `rand` calls since the previous tick)
- Replay file format (`RR_ORACLE_INPUT`), one line per tick at which the key state changes:
  ```
  <tick>\t<comma-separated virtual-key codes held>
  ```
  An empty list means all keys up.
- Record file (`RR_ORACLE_RECORD`) uses the same format.

- [ ] **Step 1: Seed hook**

```c
typedef void (__cdecl *rr_srand_fn)(unsigned);
static rr_srand_fn const orig_srand = (rr_srand_fn)0x00455ea0;
static void __cdecl hook_srand(unsigned seed) {
    char buf[32];
    if (GetEnvironmentVariableA("RR_ORACLE_SEED", buf, sizeof buf)) seed = (unsigned)strtoul(buf, NULL, 10);
    if (g_out) fprintf(g_out, "# srand %u\n", seed);
    orig_srand(seed);
}
```
In `rr_install_hooks`: `rr_patch_call((uint8_t*)0x00448224, 0x00455ea0, (void*)hook_srand);` with the same failure logging.

- [ ] **Step 2: Tick hook**

Hook the single simulation step `0x0044b3b0` (no arguments, void) at its only call site `0x0044b324`:
```c
typedef void (__cdecl *rr_step_fn)(void);
static rr_step_fn const orig_step = (rr_step_fn)0x0044b3b0;
static uint32_t g_tick;
static void __cdecl hook_tick(void) {
    rr_apply_input(g_tick);                       /* Step 3 */
    orig_step();
    if (g_out) {
        fprintf(g_out, "frame\t%u\n", g_frame);   /* tick records share the frame numbering */
        rr_log("tick.index", g_tick); rr_log("tick.counter", RD32(0x004b7ecc));
        uint32_t bike = (uint32_t)RD32(0x004642d8);
        char key[32];
        for (int k = 0; k < 128 && bike; ++k) { snprintf(key, sizeof key, "player.raw.%d", k); rr_log(key, RD32(bike + 4*k)); }
    }
    ++g_tick;
}
```
Patch `0x0044b324` → `hook_tick`, verifying target `0x0044b3b0`.

- [ ] **Step 3: Keyboard record/replay via the import table**

```c
static BYTE g_keys[256];
static int g_replaying;
static FILE* g_replay; static FILE* g_record;
static long g_next_tick = -1; static char g_next_keys[512];
static BOOL (WINAPI *real_GetKeyboardState)(PBYTE);

static void rr_read_next(void) {
    char line[600];
    g_next_tick = -1;
    if (g_replay && fgets(line, sizeof line, g_replay)) {
        char* tab = strchr(line, '\t');
        if (tab) { *tab = 0; g_next_tick = atol(line); strncpy(g_next_keys, tab + 1, sizeof g_next_keys - 1); }
    }
}
static void rr_apply_input(uint32_t tick) {
    while (g_replaying && g_next_tick >= 0 && (uint32_t)g_next_tick <= tick) {
        memset(g_keys, 0, sizeof g_keys);
        for (char* t = strtok(g_next_keys, ",\r\n"); t; t = strtok(NULL, ",\r\n")) g_keys[atoi(t) & 0xff] = 0x80;
        rr_read_next();
    }
}
static BOOL WINAPI hook_GetKeyboardState(PBYTE out) {
    if (g_replaying) { memcpy(out, g_keys, 256); return TRUE; }
    BOOL ok = real_GetKeyboardState(out);
    if (g_record && ok && memcmp(out, g_keys, 256)) {
        memcpy(g_keys, out, 256);
        fprintf(g_record, "%u\t", g_tick);
        int first = 1;
        for (int k = 0; k < 256; ++k) if (out[k] & 0x80) { fprintf(g_record, first ? "%d" : ",%d", k); first = 0; }
        fputc('\n', g_record); fflush(g_record);
    }
    return ok;
}
static MMRESULT WINAPI hook_joyGetPosEx(UINT id, LPJOYINFOEX info) { (void)id; (void)info; return JOYERR_UNPLUGGED; }

/* Replace an imported function pointer in ROADRASH.EXE's IAT. */
static void* rr_patch_iat(const char* dll, const char* name, void* hook) {
    uint8_t* base = (uint8_t*)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); d->Name; ++d) {
        if (lstrcmpiA((char*)(base + d->Name), dll)) continue;
        IMAGE_THUNK_DATA* names = (IMAGE_THUNK_DATA*)(base + d->OriginalFirstThunk);
        IMAGE_THUNK_DATA* slots = (IMAGE_THUNK_DATA*)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME* n = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (strcmp((char*)n->Name, name)) continue;
            DWORD old; void* prev = (void*)slots->u1.Function;
            VirtualProtect(&slots->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            slots->u1.Function = (uintptr_t)hook;
            VirtualProtect(&slots->u1.Function, sizeof(void*), old, &old);
            return prev;
        }
    }
    return NULL;
}
```
If the EXE's import descriptors have `OriginalFirstThunk == 0` (old linkers), iterate `FirstThunk` names by resolving each slot against `GetProcAddress(GetModuleHandleA(dll), name)` instead.

In `rr_install_hooks`:
```c
    char path[MAX_PATH];
    real_GetKeyboardState = rr_patch_iat("USER32.dll", "GetKeyboardState", (void*)hook_GetKeyboardState);
    rr_patch_iat("WINMM.dll", "joyGetPosEx", (void*)hook_joyGetPosEx);
    if (GetEnvironmentVariableA("RR_ORACLE_INPUT", path, sizeof path) && (g_replay = fopen(path, "r"))) { g_replaying = 1; rr_read_next(); }
    if (GetEnvironmentVariableA("RR_ORACLE_RECORD", path, sizeof path)) g_record = fopen(path, "w");
    if (!real_GetKeyboardState) fprintf(g_out, "# IAT patch GetKeyboardState failed\n");
```
Add `#include <mmsystem.h>`. Link with `-lwinmm` in `build.sh`.

- [ ] **Step 4: Determinism check script**

`oracle/determinism.sh`:
```sh
#!/bin/sh
# Record one session, replay it twice with a fixed seed, and require identical dumps.
set -eu
cd "$(dirname "$0")/.."
DRIVE=/Users/archit/Dev/rr/prefix/drive_c
RR_ORACLE_SEED=12345 RR_ORACLE_RECORD='C:\input-det.txt' RR_ORACLE_FRAMES=3000 oracle/run.sh det-record
for n in 1 2; do
  RR_ORACLE_SEED=12345 RR_ORACLE_INPUT='C:\input-det.txt' RR_ORACLE_FRAMES=3000 oracle/run.sh det-replay-$n
done
cmp analysis/oracle/det-replay-1.tsv analysis/oracle/det-replay-2.tsv && echo DETERMINISTIC
```
`run.sh` must export `RR_ORACLE_SEED`, `RR_ORACLE_INPUT` and `RR_ORACLE_RECORD` when set.

- [ ] **Step 5: Run it**

Run: `oracle/build.sh && oracle/install.sh && oracle/determinism.sh`

During the record run, navigate the menus and race on Napa. The replays drive themselves.

Expected: `DETERMINISTIC`.

If the replays differ, find the first differing line with `cmp -l ... | head -1`, then look up the key on that line:
- `camera.*` or `player.raw.*` differ early: some input path is not hooked. Check whether the game also calls `GetAsyncKeyState` or `GetKeyState` (not in the import list at the time of writing) or reads `WM_KEYDOWN` messages. Hook those too.
- The number of steps per frame differs: the step count passed to `0x00442e10` comes from timing (`QueryPerformanceCounter` / `timeSetEvent`). Make it deterministic by IAT-hooking `QueryPerformanceCounter` and `GetTickCount` to advance a fixed amount per `hook_tick` call. Document the change.

- [ ] **Step 6: Commit**

```bash
git add oracle && git commit -m "oracle: deterministic seed, tick dumps, keyboard record/replay

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Pixel oracle via cnc-ddraw screenshots

cnc-ddraw (the `ddraw.dll` in the install) saves the game surface to `Screenshots\` when PrintScreen (`keyscreenshot=0x2C` in `ddraw.ini`) is pressed. The hook DLL presses it every N frames so a native frame can be compared with the original at the same frame number.

**Files:**
- Modify: `oracle/aweman_proxy.c`
- Create: `tools/compare_frames.py`

**Interfaces:**
- Produces:
  - `analysis/oracle/<scenario>-shots/` holding cnc-ddraw PNGs, with a golden key `shot` (frame number) logged when a screenshot is requested
  - `tools/compare_frames.py ORIGINAL.png NATIVE.png [--mask x,y,w,h]`: prints the percentage of equal pixels and writes a diff PNG

- [ ] **Step 1: Request screenshots from the frame hook**

In the `FRAME_HOOK` macro, after `dump_frame()`:
```c
static int g_shot_every = 0;   /* set from RR_ORACLE_SHOT_EVERY in rr_install_hooks */
...
if (g_shot_every > 0 && g_frame % g_shot_every == 0) {
    HWND w = GetForegroundWindow();
    PostMessageA(w, WM_KEYDOWN, VK_SNAPSHOT, 0);
    PostMessageA(w, WM_KEYUP, VK_SNAPSHOT, 0xC0000001);
    rr_log("shot", g_frame);
}
```
In `run.sh`, after the game exits, move `RoadRashOracle/Screenshots/*` into `analysis/oracle/<scenario>-shots/`.

- [ ] **Step 2: Verify that cnc-ddraw honours posted keys**

Run: `RR_ORACLE_SHOT_EVERY=300 RR_ORACLE_FRAMES=1200 oracle/run.sh shots-test`
Expected: 4 PNGs in `analysis/oracle/shots-test-shots/`.

If none appear:
- Posted messages may be ignored. Try `keybd_event(VK_SNAPSHOT, 0, 0, 0)` followed by `keybd_event(VK_SNAPSHOT, 0, KEYEUP, 0)`.
- If that also fails, set `screenshotkey` / `keyscreenshot` to another key such as F12 (`0x7B`) in the oracle copy's `ddraw.ini` and post that key.
- Record which method worked in `docs/VERIFICATION.md`.

- [ ] **Step 3: Frame comparison tool**

`tools/compare_frames.py` (stdlib plus Pillow; install with `python3 -m pip install --user pillow`):
```python
#!/usr/bin/env python3
"""Compare an original (oracle) frame with a native frame of the same size."""
import argparse
from PIL import Image, ImageChops

p = argparse.ArgumentParser()
p.add_argument('original'); p.add_argument('native'); p.add_argument('--mask', help='x,y,w,h region to compare')
p.add_argument('--diff', default='diff.png')
a = p.parse_args()
o = Image.open(a.original).convert('RGB'); n = Image.open(a.native).convert('RGB')
if o.size != n.size:
    n = n.resize(o.size, Image.NEAREST)
if a.mask:
    x, y, w, h = map(int, a.mask.split(','))
    o = o.crop((x, y, x + w, y + h)); n = n.crop((x, y, x + w, y + h))
d = ImageChops.difference(o, n)
total = o.size[0] * o.size[1]
same = sum(1 for px in d.getdata() if px == (0, 0, 0))
d.point(lambda v: 255 if v else 0).save(a.diff)
print(f'{100.0 * same / total:.2f}% identical ({total - same} differing pixels); diff -> {a.diff}')
```

- [ ] **Step 4: Commit**

```bash
git add oracle tools/compare_frames.py && git commit -m "oracle: periodic cnc-ddraw screenshots and frame comparison tool

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Documentation

- [ ] **Step 1: Write `docs/ORACLE.md`**

Cover:
- What the oracle is and why it replaces `AWEMAN32.DLL`
- The patched sites and their expected bytes (the Task 5 and 7 tables)
- The environment variables
- The golden format
- How to record and replay input
- How to add a new dump field: read the struct in the decompiled function, add an `rr_log` line, rebuild, reinstall
- Known limitations

- [ ] **Step 2: Update PROGRESS, VERIFICATION, README**

- `PROGRESS.md`: add rows "Verification oracle" and "8-bit framebuffer" to the status table, and a timeline entry.
- `VERIFICATION.md`: record the determinism result and the `oracle-road` result.
- `README.md`: link `docs/ORACLE.md`.

- [ ] **Step 3: Commit**

```bash
git add docs README.md && git commit -m "docs: oracle usage and foundation status

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
