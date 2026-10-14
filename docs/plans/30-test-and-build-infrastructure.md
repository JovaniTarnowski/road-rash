# Test and Build Infrastructure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) conventions and [REVIEW-ORCHESTRATION.md](REVIEW-ORCHESTRATION.md) first.

**Goal:** Fast, targeted test runs for iteration, one shared check helper, zero first-party warnings at a strict level, no duplicated byte/ISO parsing, and a safe way to reclaim the ~3.5 GB of stale build trees.

**Architecture:** Mostly CMake and tooling. Task 1 adds CTest labels and a parallel fast target. Task 2 adds `tests/check.hpp`, used by new tests and adopted in a mechanical fan-out. Task 3 fixes the 105 strict warnings in fan-out groups and hands a clean tree to [plan 22](22-toolchain-hardening.md) Task 5's ratchet. Task 4 replaces the six private `le32` helpers with the new `rr::ByteReader` and shares ISO record parsing. Task 5 extends `tools/agent_workspace.py` with a claim-aware build-tree pruner.

**Tech Stack:** CMake ≥ 3.22, CTest labels, Python 3 (stdlib only), C++20.

**Spec:** [../agent-review/CODE-REVIEW-2026-10-13.md](../agent-review/CODE-REVIEW-2026-10-13.md), findings 12–16.

## Global Constraints

- The default `ctest` (no labels) still runs everything; `tools/ci.sh` keeps running the full suite.
- No test is weakened, skipped or shortened to make it fast. Speed comes from labels, parallelism and faster setup, never from less coverage.
- Warning fixes in ported code keep each original width explicitly (`uint16_t(x)` where the original stores 16 bits; plan 22 Review Focus 5). Every file touched in Task 3 keeps its goldens byte-identical.
- The pruner never deletes a tree with a live claim, the current session's tree, or anything outside `build*` directories at the repo root, and defaults to dry-run.

## Review Focus

1. **A test whose name changes** must keep its label (labels are assigned by name list; Task 1's guard test fails if a registered test has no label).
2. **`ctest -j` on tests that share an output path** (`first-run-test-pref`, files under `/tmp`) must not collide. Task 1 audits them and gives each test a unique path.
3. **A `-Wshadow` fix that renames a variable in ported arithmetic** must not change which variable a later line reads (golden identity per group).
4. **A read over a short buffer:** `ByteReader` throws `std::runtime_error`, and callers keep their own earlier, more specific size checks (Task 4).
5. **The pruner and a build dir that is being written right now.** It refuses trees modified in the last 10 minutes unless `--include-recent` is given.

---

### Task 1: CTest labels, parallel fast runs and a label guard

**Wave:** W1 · **Owns:** `CMakeLists.txt` (test section only), `Makefile` (`test-fast`, `test-sim`), `tests/agent_tooling_test.py` (label guard) · **Profile:** mechanical

- [x] **Step 1: Measure.** Run `ctest --test-dir build-wt-30-1 -j1` and record per-test times (the review run: `hires-render` 177 s, `race-soak` 176 s, `course_scene` 54 s, `race_setup` 35 s, `raster-rgb` 18 s, `widescreen` 17 s, `oracle-scene` 16 s; total 566 s serial).
- [x] **Step 2: Assign labels** at the end of the test section:
```cmake
set(RR_SLOW_TESTS hires-render race-soak course_scene race_setup raster-rgb widescreen oracle-scene)
set(RR_SIM_TESTS oracle-sim race race-soak race_setup bike crash traffic actor_traffic pedestrian police sim_fields track_ring)
set(RR_DISC_TESTS oracle-road oracle-scene oracle-raster hires-render raster-rgb widescreen polygon text frontend
    native_screens sound music pe-strings-corpus career-corpus wav-corpus race-soak race_setup)
foreach(t ${RR_SLOW_TESTS})
    set_property(TEST ${t} APPEND PROPERTY LABELS slow)
endforeach()
foreach(t ${RR_SIM_TESTS})
    set_property(TEST ${t} APPEND PROPERTY LABELS sim)
endforeach()
foreach(t ${RR_DISC_TESTS})
    set_property(TEST ${t} APPEND PROPERTY LABELS disc)
endforeach()
```
  Verified: all names resolve in `ctest -N` (94 tests); `race-soak`/`race_setup` gained `SKIP_RETURN_CODE 77` (they return 77); `course_scene` labelled `disc` (reads the disc, returns 0 without corpus).
  Verify each list against `ctest -N` and the actual skip behaviour: a test that exits 77 without the disc belongs in `disc`. Add anything the review missed, such as `course_scene`, if it reads the disc.
- [x] **Step 3: Collision audit.** `rg -n "temp_directory_path|/tmp/|first-run-test-pref" tests CMakeLists.txt`. Make every fixed path per-test, for example by appending the test name or `getpid()`.
- [x] **Step 4: Make targets.**
```make
test-fast: build
	ctest --test-dir $(BUILD_DIR) -LE slow -j $(JOBS) --output-on-failure
test-sim: build
	ctest --test-dir $(BUILD_DIR) -L sim -j $(JOBS) --output-on-failure
```
  Add both to `.PHONY` and `help`.
- [x] **Step 5: Label guard.** Add a check to `tests/agent_tooling_test.py` that parses `ctest --test-dir <dir> --show-only=json-v1` when a build dir is given through `RR_BUILD_DIR`. Every test must have at least one label from `{slow, sim, disc, unit}`; unlabelled tests get `unit` through a final CMake loop over `get_property(... TESTS)`, so the guard catches a missing loop rather than a missing name.
- [x] **Step 6: Verify.** `make test-fast` time versus the full serial run (record both); `ctest -j $(sysctl -n hw.ncpu)` passes with the same results as `-j1`, run three times. Commit: `test: labels (slow/sim/disc/unit), parallel fast targets, label guard`.

---

### Task 2: Shared `tests/check.hpp`

**Wave:** W1 (header), then fan-out in W3 · **Owns (W1):** `tests/check.hpp`, `tests/check_test.cpp` · **Profile:** mechanical

**Interfaces:**
```cpp
#pragma once
#include <iostream>
#include <string_view>
namespace rr::test {
// Shared check for the no-framework tests: records, prints "FAIL <what>", never aborts.
struct Checks {
    int failures = 0;
    bool operator()(bool ok, std::string_view what) {
        if (!ok) { ++failures; std::cerr << "FAIL " << what << '\n'; }
        return ok;
    }
    template <class A, class B> bool eq(const A& a, const B& b, std::string_view what) {
        if (a == b) return true;
        ++failures; std::cerr << "FAIL " << what << ": got " << a << ", expected " << b << '\n';
        return false;
    }
    int finish(std::string_view name) const {
        if (failures == 0) std::cout << name << " OK\n";
        else std::cerr << name << ": " << failures << " failure(s)\n";
        return failures ? 1 : 0;
    }
};
}
```
- [ ] **Step 1:** Add the header and `tests/check_test.cpp`. The test exercises pass, fail, `eq` with integers and `finish`'s return values; for the failing case, construct a separate `Checks` object and assert its `failures == 1`. Commit: `test: shared Checks helper`.
- [ ] **Step 2 (W3 fan-out, optional, one worker per 10 files, disjoint lists from `rg -l "auto check = \[&\]" tests`):** replace the local lambda with `rr::test::Checks check;`, keep each call site's message text, and end `main` with `return check.finish("<name>");`. Exit codes and outputs other than the final summary line stay the same. Commit per group: `test: adopt Checks (group N)`.

---

### Task 3: Zero first-party warnings at the strict level

**Wave:** W2 (fan-out) · **Profile:** mechanical; ported files are sim-sensitive

- [ ] **Step 1 (orchestrator, CMake):** make vendor headers system headers: `target_include_directories(rr-resource SYSTEM PRIVATE third_party)` and the same for `${RR_VIEWER}`. Add `option(RR_STRICT_WARNINGS ...)`, which applies `-Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion` to first-party targets. Plan 22 Task 5 owns the option name if it lands first; reuse its name.
- [ ] **Step 2 (fan-out):** regenerate the list with `cmake --build build-strict 2>&1 | grep "warning:" | grep -v third_party | sort -u`, then split by file:

| Group | Files |
| --- | --- |
| G1 | `src/game_main.cpp` (after plan 28; delete the unused `quad`; designated-initializer order; missing `config` initializers) |
| G2 | `src/hud.cpp`, `src/road_draw.cpp`, `src/text.cpp`, `src/import_ui.cpp`, `src/post_chain.cpp`, `src/raster_rgb.cpp` (int→float for SDL coordinates: explicit `float(...)`) |
| G3 | `src/course.cpp`, `src/race_music.cpp`, `src/actor_descriptors.cpp` (ported widths: cast to the original width; make the bounds-check-only `.at()` explicit with `(void)` and a comment) |
| G4 | `src/import_job.cpp`, `src/native_screens.cpp`, `src/upscale_import.cpp` (shadowing) |
| G5 | `tests/*.cpp` (shadowing in tests, the pessimizing move in `crash_test.cpp:142`, the range-loop copy in `wav_test.cpp:56`, unused variables) |

  `race.cpp`'s unused locals are removed by plan 25 Task 6.
- [ ] **Step 3:** Each group builds `build-strict` with no warnings in its files, runs `ctest`, and for G3 checks that the corpus and golden tests are byte-identical. Commit per group: `<area>: fix strict warnings (group N)`.
- [ ] **Step 4 (orchestrator):** once all groups merge, `-Werror` under `RR_STRICT_WARNINGS` goes green, and plan 22 Task 5 records an empty baseline.

---

### Task 4: One byte reader and one ISO record parser

**Wave:** W2, after `src/byte_reader.hpp` is **committed**. Another session added `rr::ByteReader` during this review (untracked on October 13; likely plan 22 work). Do not create a second header. **Owns:** the `le32` definitions and their call sites in `src/career.cpp`, `src/bike.cpp`, `src/frontend.cpp`, `src/import_job.cpp`, `src/iso9660.cpp`; `src/iso9660.hpp`; `src/upscale_cache.cpp` (`read_u32` only) · **Profile:** careful-mechanical

**Interfaces:**
- Consumes (as committed; re-read the header first, since it may have changed):
```cpp
namespace rr {
class ByteReader {                       // src/byte_reader.hpp
public:
    explicit ByteReader(std::span<const uint8_t>);
    std::span<const uint8_t> bytes(size_t offset, size_t count) const;   // throws std::runtime_error
    template <class T> T le(size_t offset) const;                        // throws std::runtime_error
};
}
```
- Produces: `Iso9660::Entry parse_iso_record(std::span<const uint8_t> record)`, a free function in `iso9660.hpp` moved from `Iso9660::record`.

- [ ] **Step 1: Inventory.** `rg -n "le32\(|le16\(|read_u32\(" src` lists each private helper and its call sites. For each, note in the handoff whether the caller bounds-checks before the call.
- [ ] **Step 2: Replace each helper.** `le32(b, p)` → `rr::ByteReader(b).le<uint32_t>(p)`. Where a function reads many fields, construct one reader at the top. Pointer forms (`le32(&b[16])` in career.cpp) become `reader.le<uint32_t>(16)` over the whole buffer. Every converted read is now checked. Where the old code relied on an earlier size check, keep that check, because it produces the more specific error message that tests may assert on. Grep the tests for the message strings first.
- [ ] **Step 3: ISO.** Move `Iso9660::record` into the free function `parse_iso_record` and call it from `import_job.cpp` wherever that file re-decodes directory records (`rg -n "record|0x21|0x20|\[0\]" src/import_job.cpp`). The pull-based control flow stays; only record decoding is shared. Add a truncation test in `tests/iso9660_test.cpp`: every prefix length 0..33 of a valid synthetic record must throw `std::runtime_error`, never anything else.
- [ ] **Step 4: Verify.** `ctest -R "iso9660|import_job|importer|career|bike|frontend|upscale_cache|byte_reader"`; the corpus tests stay byte-identical. Commit: `io: one little-endian reader and one ISO record parser`.

---

### Task 5: Claim-aware build-tree pruner

**Wave:** W3 · **Owns:** `tools/agent_workspace.py` (new `builds` subcommand), `tests/agent_tooling_test.py` (cases) · **Profile:** mechanical

- [ ] **Step 1: Tests first** in `tests/agent_tooling_test.py`, using a temp repo layout with `build-a` (old, unclaimed), `build-b` (claimed by a live pid: use `os.getpid()`), `build-c` (modified now) and `src/` (must never be listed). The tests run `builds --dry-run` and `builds --prune --yes` and assert that only `build-a` is removed.
- [ ] **Step 2: Implement** `python3 tools/agent_workspace.py builds [--prune] [--yes] [--include-recent] [--older-than DAYS]`. It lists `build*` directories at the repo root with size (summing `os.scandir` without following symlinks), last mtime, and claim owner/liveness from the existing claims registry (the same source `agent_doctor.py` reads). It deletes only with `--prune --yes`, only unclaimed trees, only trees older than `--older-than` (default 3 days), and never `build` itself unless named explicitly.
- [ ] **Step 3:** Run it in the real repo with `--dry-run` only and paste the table in the handoff. Deleting real trees is the owner's call. Commit: `tools: claim-aware build-tree listing and pruning`.
