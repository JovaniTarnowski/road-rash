# Toolchain Hardening Implementation Plan (C++20, fuzzing, ThreadSanitizer, warnings, clang-tidy)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Independent of every other plan; run it alongside them. Task 4 (ThreadSanitizer) should land before or with [plan 19](19-high-resolution-rendering.md) Task 6 (threaded rasterisation).

**Goal:** Close most of the memory- and thread-safety gap with Rust without changing language:
1. move the project to C++20;
2. fuzz every parser of user-supplied data with libFuzzer, briefly on every CI run;
3. add a ThreadSanitizer build;
4. raise compiler warnings to errors and run clang-tidy's bug-finding checks, without a big-bang cleanup.

**Architecture:** All of this is build and test infrastructure. Source changes are limited to what the tools find; each finding is fixed in its own small commit with a regression test. Two extra build trees join the existing `build` (Debug), `build-ubsan` (ASan + UBSan) and `build-release`: `build-fuzz` and `build-tsan`. They use Homebrew LLVM (`/opt/homebrew/opt/llvm`, clang 22.1.4 at the time of writing, which ships `libclang_rt.fuzzer_osx.a` and `clang-tidy`), because Apple's clang has neither libFuzzer nor clang-tidy.

**Tech Stack:** CMake ≥ 3.22, Apple clang (normal builds), Homebrew LLVM 22 (fuzzing, TSan, clang-tidy), Android NDK 28.2 clang (C++20 check), CTest.

**Spec:** the owner's request of October 12, 2026 to plan the four alternatives to a Rust rewrite: C++20, libFuzzer on every parser in CI, a TSan build, warnings as errors plus clang-tidy bounds and lifetime checks.

## Global Constraints

- **No behaviour change from the toolchain.** After the C++20 switch, every golden and every test passes with identical output. A finding fix changes behaviour only for inputs that previously crashed, hung or reached undefined behaviour.
- **Parser contract (00-INDEX):** a parser of original data either returns a value or throws `std::runtime_error`. Anything else on any input (crash, sanitizer report, another exception type, a hang, unbounded memory) is a bug.
- **No original data in the repository.** Seed corpora committed to git are synthetic (written by test code). Corpora seeded from the user's disc live only in the git-ignored `analysis/fuzz/`. A crash input found from a disc-seeded corpus is never committed: the regression test reproduces the bug with hand-made bytes.
- **Android must keep building** (`tools/ci.sh` assembles debug and release).
- **No warning or tidy suppressions without a reason** written next to them.

## Review Focus

1. **Behaviour drift from C++20.** Shifts, integer conversions and comparison rewriting can silently change results in ported code. Task 1 runs the full golden set before and after and diffs the per-tick sim hashes.
2. **Exceptions other than `std::runtime_error`.** `std::vector::at` throws `std::out_of_range`, and `std::bad_alloc` follows a huge length field. The fuzz harness treats them as bugs, and Task 3 fixes each parser to check sizes explicitly.
3. **Hangs and memory blow-ups.** A length field of `0xffffffff` must not allocate 4 GB or loop for minutes. Each fuzz target runs with `-timeout=2 -rss_limit_mb=512`.
4. **Races at shutdown and on lifecycle events.** The import worker thread, the SDL audio callback (mixer queue) and SDL event watches (`LifecycleWatch`) touch shared state; TSan runs scenarios that quit mid-import, pause during audio and background the app.
5. **Warnings that change meaning when "fixed".** A `-Wconversion` fix that adds a cast in ported code can change truncation behaviour. Every such fix in a file with original addresses must keep its goldens passing, and casts must state the original width (`uint16_t(x)` where the original stores 16 bits).

---

## File Structure

| File | Responsibility |
| --- | --- |
| `CMakeLists.txt` | C++20; `RR_FUZZ`, `RR_TSAN`, `RR_STRICT_WARNINGS` options; `rr-warnings` interface target |
| `cmake/llvm.cmake` | Locates Homebrew LLVM for the extra build trees |
| `fuzz/*.cpp` | One libFuzzer target per parser |
| `fuzz/seeds.cpp`, `fuzz/seeds/` | Generates synthetic seed inputs; committed synthetic seeds |
| `tools/fuzz.sh` | Builds and runs every target for N seconds; collects crashes |
| `tools/tsan.sh` | Builds the TSan tree and runs its tests and scenarios |
| `tools/tidy.sh`, `.clang-tidy`, `tools/tidy-baseline.txt`, `tools/warnings-baseline.txt` | Static checks with baselines |
| `tools/ci.sh` | Calls the new stages |
| `docs/TOOLING.md`, `docs/VERIFICATION.md` | How to run them; results |

---

### Task 1: Move to C++20

**Files:**
- Modify: `CMakeLists.txt` (`set(CMAKE_CXX_STANDARD 20)`), `android/app/build.gradle` or its CMake arguments if the standard is set there (check with `grep -rn "c++17\|CXX_STANDARD" android`), any source the compilers reject, `tests/view_test.cpp` (pin the guaranteed semantics), `docs/TOOLING.md`

- [ ] **Step 1: Record the baseline.** Before changing anything: run `ctest --test-dir build --output-on-failure` and save the list of passed and skipped tests. Run `oracle-sim-test` on every golden and `race-soak` (if plan 16 Task 7 has landed; otherwise a 3,600-tick headless Napa race printing a per-tick snapshot hash, added as `rr-game --race --ticks 3600 --hash-log FILE`). Save the hashes in `analysis/cpp20-before/`.
- [ ] **Step 2: Write the pinning test.** Add to `tests/view_test.cpp`:

```cpp
    // C++20 guarantees these; the ported code relies on them (arithmetic shift of negatives, two's complement).
    static_assert((-8 >> 1) == -4 && (-1 >> 31) == -1, "arithmetic right shift");
    static_assert(int32_t(uint32_t(0x80000000u)) == INT32_MIN, "two's complement conversion");
    static_assert(uint32_t(int32_t(-1)) == 0xffffffffu, "modular conversion");
```

- [ ] **Step 3: Switch** to C++20. Build the Debug, UBSan and Release trees and the Android debug build. Fix only what the compilers reject. Likely items: `u8"..."` literals (now `char8_t`), implicit `this` capture in `[=]` lambdas (deprecated), comparison operators that become ambiguous under rewriting, `volatile` compound assignments.
- [ ] **Step 4: Compare** (Review Focus 1). Re-run Step 1's commands into `analysis/cpp20-after/` and `diff -r` the two directories. Expected: identical test lists and identical hashes. Any difference is investigated and fixed before committing.
- [ ] **Step 5: Adopt the new tools in one place.** Add `src/byte_reader.hpp`, a bounds-checked reader over `std::span<const uint8_t>` that every new or fuzz-fixed parser uses:

```cpp
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace rr {
// Little-endian reads over a byte span. Every read checks bounds and throws std::runtime_error,
// never std::out_of_range, so parsers keep the project's exception contract.
class ByteReader {
public:
    explicit ByteReader(std::span<const uint8_t> bytes) : bytes_(bytes) {}
    size_t size() const { return bytes_.size(); }
    std::span<const uint8_t> bytes(size_t offset, size_t count) const {
        if (offset > bytes_.size() || count > bytes_.size() - offset) throw std::runtime_error("Read past end of data");
        return bytes_.subspan(offset, count);
    }
    template <class T> T le(size_t offset) const {
        static_assert(std::is_integral_v<T>);
        const auto b = bytes(offset, sizeof(T));
        std::array<uint8_t, sizeof(T)> raw{};
        std::memcpy(raw.data(), b.data(), sizeof(T));
        if constexpr (std::endian::native == std::endian::big) std::reverse(raw.begin(), raw.end());
        return std::bit_cast<T>(raw);
    }
private:
    std::span<const uint8_t> bytes_;
};
}
```

  Add `tests/byte_reader_test.cpp`: reads at the last valid offset succeed; one byte further throws `std::runtime_error`; `offset + count` overflow (`offset = SIZE_MAX`) throws rather than wrapping; little-endian values decode correctly. Do **not** mass-convert existing parsers; Task 3 converts the ones fuzzing finds broken.
- [ ] **Step 6:** Run the regression gate and `tools/ci.sh`. Commit: `build: C++20; ByteReader for bounds-checked parsing`.

---

### Task 2: Fuzzing infrastructure and synthetic seeds

**Files:**
- Create: `cmake/llvm.cmake`, `fuzz/fuzz_common.hpp`, `fuzz/seeds.cpp`, `tools/fuzz.sh`
- Modify: `CMakeLists.txt` (`option(RR_FUZZ ...)`), `.gitignore` (`analysis/fuzz/` is already covered by `analysis/`; confirm), `docs/TOOLING.md`

**Interfaces:**
- Produces:
  ```cpp
  // fuzz/fuzz_common.hpp
  #pragma once
  #include <cstdint>
  #include <cstdio>
  #include <cstdlib>
  #include <stdexcept>
  #include <vector>
  // Runs a parser under the project contract: return or std::runtime_error. Any other exception aborts
  // (libFuzzer records the input as a crash). Sanitizers, timeouts and RSS limits catch the rest.
  template <class F> int rr_fuzz_contract(const uint8_t* data, size_t size, F&& parse) {
      try { parse(std::vector<uint8_t>(data, data + size)); }
      catch (const std::runtime_error&) {}
      catch (const std::exception& e) { std::fprintf(stderr, "contract: unexpected %s\n", e.what()); std::abort(); }
      return 0;
  }
  ```
  `tools/fuzz.sh [seconds-per-target] [target...]` builds `build-fuzz` with Homebrew clang (`-DRR_FUZZ=ON -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++`, flags `-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined`), then runs each target with `-max_total_time=<seconds> -timeout=2 -rss_limit_mb=512` over `fuzz/seeds/<target>` (committed, synthetic) plus `analysis/fuzz/<target>` (local, may hold disc-derived seeds). Crashes go to `analysis/fuzz/crashes/<target>/`. Exit status 1 if any target found a crash.

- [ ] **Step 1:** Add `cmake/llvm.cmake` (fails with a clear message if `/opt/homebrew/opt/llvm/bin/clang++` is missing, naming `brew install llvm`; ask the owner before installing anything) and the `RR_FUZZ` option that adds one executable per `fuzz/*.cpp` linked to `rr-resource`.
- [ ] **Step 2:** Write `fuzz/seeds.cpp`, a small program (built only with `RR_FUZZ`) that writes synthetic valid inputs for each target into `fuzz/seeds/<target>/`, using the same hand-made builders the unit tests use (for example the minimal RSC directory in `tests/resource_test.cpp`, the minimal WAV in `tests/wav_test.cpp`, the ISO fixture in `tests/iso_fixture.hpp`). Commit the generated seeds (they contain no original data).
- [ ] **Step 3:** Write `tools/fuzz.sh`. Also a `--seed-from-disc` mode that copies the user's imported files into `analysis/fuzz/<target>/` (local only, never committed).
- [ ] **Step 4:** Verify with one target from Task 3 (start with `fuzz_settings`, the simplest) that `tools/fuzz.sh 10 fuzz_settings` runs and exits 0. Commit: `fuzz: libFuzzer build, synthetic seeds and runner`.

---

### Task 3: Fuzz targets for every parser, and the fixes they need

**Files:**
- Create: one file per target under `fuzz/`
- Modify: parsers as findings require, their unit tests (regression cases with hand-made bytes)

Targets (one per entry point; each body is one `rr_fuzz_contract` call):

| Target | Entry point | Notes |
| --- | --- | --- |
| `fuzz_resources` | `read_resources` | RSC directory |
| `fuzz_unpack` | `unpack_resource`, `explode(data, size, expected)` | take `expected` from the first 4 bytes, capped at 1 MiB |
| `fuzz_cel` | `read_cel`, `read_plut` | |
| `fuzz_family` | `read_family`, `read_cel_group(unpacked, 0)` | |
| `fuzz_course` | `read_course` | CRS |
| `fuzz_terrain` | `read_terrain(bytes, n)` | `n` from the first 2 bytes |
| `fuzz_cars` | `VehicleLibrary(bytes)` | CAR container |
| `fuzz_animation` | `read_animation`, `read_actor_graphics`, `read_actor_sequence(bytes, bytes)` | RRAN, CANS |
| `fuzz_bobs` | `read_bobs(bytes, 0x146)` | |
| `fuzz_avi` | `read_avi`, then `CinepakDecoder(320, 240).decode` on each video chunk | |
| `fuzz_audio` | `read_wav`, `decode_ima_adpcm(data, size, 1 + (b0 & 1), 1 + b1)` | |
| `fuzz_iso` | `Iso9660` over the input as a byte range; `list("/")` and `read` of every listed file | |
| `fuzz_pe` | `read_string_resources`, `read_string_table` | PE DLL |
| `fuzz_rri` | `decode_rri` | menu pictures |
| `fuzz_save` | `read_save(bytes, tables)` | `tables` from a synthetic `CareerTables` built in the target |
| `fuzz_settings` | `parse_settings(std::string(bytes))` | must never throw |
| `fuzz_exe_tables` | `read_physics_tables`, `read_depth_table`, `read_frontend_data`, `read_personalities`, `read_crash_config`, `read_police_config` on the input as an EXE image | these read fixed addresses; must throw cleanly on short or non-PE input |

Later plans add their own targets: `fuzz_sbk`, `fuzz_midi` (plan 13 Task 2), `fuzz_rrup` (plan 20 Task 1).

- [ ] **Step 1:** Write all targets. Example, `fuzz/fuzz_course.cpp`:

```cpp
#include "course.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) { (void)rr::read_course(bytes); });
}
```

- [ ] **Step 2: Run** `tools/fuzz.sh 300` (5 minutes per target) once with synthetic seeds and once after `--seed-from-disc`. Record per target: executions, coverage (`cov:` from libFuzzer's final line), and crashes.
- [ ] **Step 3: Fix each finding** in its own commit:
  1. minimise the input (`<target> -minimize_crash=1 <crash file>`);
  2. write a failing unit test in the parser's existing test file reproducing the problem with **hand-made** bytes (Global Constraints), run it, confirm it fails;
  3. fix the parser (prefer `ByteReader` for the failing reads; replace `.at()`-as-validation with explicit checks that throw `std::runtime_error`, Review Focus 2; cap lengths before allocating, Review Focus 3);
  4. run the test and the parser's goldens; commit `fix(<parser>): <what the fuzzer found>`.
- [ ] **Step 4:** Re-run `tools/fuzz.sh 300` until every target runs its full time with no crash. Record the final numbers in VERIFICATION.md.
- [ ] **Step 5:** Commit the targets: `fuzz: targets for every parser of user data`.

---

### Task 4: ThreadSanitizer build

**Files:**
- Create: `tools/tsan.sh`, `tools/tsan.supp` (only if needed)
- Modify: `CMakeLists.txt` (`option(RR_TSAN ...)` adds `-fsanitize=thread`), `src/game_main.cpp` (`--quit-after-import-ms`, `--background-at-tick` debug flags if missing), `docs/TOOLING.md`

- [ ] **Step 1:** `tools/tsan.sh` configures `build-tsan` with Homebrew clang and `-DRR_TSAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo`, builds, runs `ctest` with `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1`, then runs the threaded scenarios (Review Focus 4):
  - the first-run import UI quitting while the worker thread is copying (`rr-game --first-run-smoke --quit-after-import-ms 200` against a synthetic ISO fixture);
  - a headless race with audio opened to the dummy driver (`SDL_AUDIO_DRIVER=dummy`) for 1,800 ticks, pausing and resuming the mixer every 100 ticks;
  - a lifecycle background/foreground cycle (`--background-at-tick 300`);
  - once plan 19 Task 6 lands: `hires-render` with 7 render threads.
- [ ] **Step 2: Run** it. Each report is fixed in the owning code (atomics or the existing SPSC queue for audio; a join on shutdown for the import worker), with a commit per fix. A suppression goes in `tools/tsan.supp` only for SDL-internal frames, each with a comment naming the SDL function and why it is benign.
- [ ] **Step 3:** Commit: `test: ThreadSanitizer build and threaded scenarios`.

---

### Task 5: Warnings as errors with a ratchet

**Files:**
- Create: `tools/warnings-baseline.txt`, `tools/check_warnings.py`
- Modify: `CMakeLists.txt` (`rr-warnings` interface target linked by `rr-resource`, `rr-game`, tests), `tools/ci.sh`

**Interfaces:**
- Produces: `RR_STRICT_WARNINGS` (default ON for Debug): `-Wall -Wextra -Wpedantic -Werror` everywhere except `third_party/`; plus `-Wconversion -Wsign-conversion -Wshadow` reported (not errors) and counted per file by `tools/check_warnings.py`, which fails CI if any file's count rises above `tools/warnings-baseline.txt` and rewrites the baseline downward when counts fall (`--update`).

- [ ] **Step 1: Measure.** Build once with all the flags as warnings and save the counts per file and flag to `analysis/warnings-initial.txt`. Record the totals in TOOLING.md.
- [ ] **Step 2: Make `-Wall -Wextra -Wpedantic` errors.** Fix every instance (usually unused parameters and variables, missing field initialisers, sign comparisons). Run the goldens after each file touched in ported code (Review Focus 5).
- [ ] **Step 3: Ratchet the rest.** Commit `tools/warnings-baseline.txt` from the current counts and wire `check_warnings.py` into `tools/ci.sh`. New files must have zero conversion warnings (the script enforces zero for files not in the baseline).
- [ ] **Step 4: Write the test** for the script itself (`python3 -m unittest`): a fake compiler log with one more warning in a baselined file fails; one fewer passes and `--update` lowers the baseline; a new file with one warning fails.
- [ ] **Step 5:** Commit: `build: warnings as errors; conversion warning ratchet`.

---

### Task 6: clang-tidy with a ratchet

**Files:**
- Create: `.clang-tidy`, `tools/tidy.sh`, `tools/tidy-baseline.txt`
- Modify: `CMakeLists.txt` (`CMAKE_EXPORT_COMPILE_COMMANDS ON`), `tools/ci.sh`, `docs/TOOLING.md`

- [ ] **Step 1:** `.clang-tidy`:

```yaml
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  clang-analyzer-*,
  cppcoreguidelines-pro-bounds-constant-array-index,
  cppcoreguidelines-narrowing-conversions,
  cppcoreguidelines-init-variables,
  bugprone-dangling-handle,
  bugprone-use-after-move,
  performance-unnecessary-copy-initialization
WarningsAsErrors: ''
HeaderFilterRegex: 'src/.*'
```

  Reason for the selection: bounds (`pro-bounds-constant-array-index`), lifetime (`dangling-handle`, `use-after-move`, the clang static analyser's `cplusplus.NewDelete` and `core` checkers) and conversions are where C++ differs from Rust; style checks are left out on purpose. `pro-bounds-pointer-arithmetic` is not enabled: the raster spans use pointer arithmetic by design and are covered by ASan and fuzzing.
- [ ] **Step 2:** `tools/tidy.sh` runs `/opt/homebrew/opt/llvm/bin/run-clang-tidy -p build -quiet 'src/.*'` and counts findings per file and check. Same ratchet as Task 5 (`tools/tidy-baseline.txt`, zero for new files). Reuse `check_warnings.py` with a `--format tidy` switch rather than a second script; add a unit test case for that format.
- [ ] **Step 3: Triage the analyser findings first** (`clang-analyzer-*`, `bugprone-use-after-move`, `dangling-handle`): these are likely real bugs. Fix each with a test, one commit each. Leave the rest to the ratchet.
- [ ] **Step 4:** Wire into `tools/ci.sh` (skipped with a message if Homebrew LLVM is absent, so CI still runs on a machine without it). Commit: `build: clang-tidy bounds/lifetime checks with a ratchet`.

---

### Task 7: CI stages and documentation

**Files:**
- Modify: `tools/ci.sh`, `docs/TOOLING.md`, `docs/VERIFICATION.md`, `docs/plans/00-INDEX.md` (regression gate)

- [ ] **Step 1:** `tools/ci.sh` gains, in order, after the existing native tests: warnings ratchet (always), `tools/tidy.sh` (if LLVM present), `tools/fuzz.sh 30` (if LLVM present; 30 seconds per target, about 9 minutes; `RR_FUZZ_SECONDS` overrides), and `tools/tsan.sh` (if LLVM present and `RR_TSAN=1`, because it rebuilds everything).
- [ ] **Step 2:** Update the 00-INDEX regression gate to list the new stages and when each must be run (fuzz and TSan before closing any plan that touches a parser or a thread).
- [ ] **Step 3:** TOOLING.md: how to install Homebrew LLVM, run each stage alone, reproduce a fuzz crash, and update the baselines.
- [ ] **Step 4:** Run `tools/ci.sh` end to end. Commit: `ci: fuzzing, tidy, warnings and TSan stages`.

## Exit criteria

- The project builds as C++20 on macOS and Android, with identical goldens and sim hashes.
- Every parser has a fuzz target; each ran 5 minutes from synthetic and disc seeds with no finding left open; CI fuzzes every target on each run.
- TSan runs clean over the tests and the threaded scenarios.
- Warnings are errors for `-Wall -Wextra -Wpedantic`; conversion warnings and clang-tidy findings can only go down.
