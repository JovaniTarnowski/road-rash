# Safe Object Field Access Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) conventions and [REVIEW-ORCHESTRATION.md](REVIEW-ORCHESTRATION.md) first.

**Goal:** Remove the strict-aliasing undefined behaviour from `rr::Bike` field writes without changing a single simulated value. Then, optionally and with the owner's approval, make the 1,059 hex-offset accesses self-describing while keeping each original offset visible.

**Architecture:** `Bike::i32(int)` (non-const) returns a small proxy, `I32Field`, that reads and writes through `std::memcpy`. Assignment, `+=`, `-=` and implicit reads keep their current call syntax, so the change is mechanical and compiler-guided. A determinism test then runs the same race in Debug and Release trees and requires equal hashes. Tasks 2–3 add `bike_field::<name>_<hex>` constants (for example `bike_field::position_1c`) and fan the replacement out across disjoint file groups.

**Tech Stack:** C++20, CTest, CMake (an extra Release test tree).

**Spec:** [../agent-review/CODE-REVIEW-2026-10-13.md](../agent-review/CODE-REVIEW-2026-10-13.md), finding 6.

## Global Constraints

- **Zero behaviour change.** `race-soak` hashes, `oracle-sim` diagnostic JSON, and every golden test are byte-identical before and after every task in this plan.
- Offsets stay visible at each use site. Ported code cites original addresses (00-INDEX code rules), so names carry the hex (`position_1c`), never a bare word.
- Task 3 edits ported files that other plans also edit. Run it only when `git status`, the claims registry (`python3 tools/agent_doctor.py`) and the orchestration table show no other owner of those files.
- Tasks 2–3 are **decision-gated**: start them only after the owner approves the naming convention (the orchestrator asks once, showing the Task 2 header).

## Review Focus

1. **A call site binds `int32_t&` to `i32(...)`** (bike.cpp:252 `int32_t& gear = b.i32(0x23c);`) or passes it to an `int32_t&` parameter. These must become explicit read/modify/write with the same semantics, including aliasing between two names for one field.
2. **Unaligned or out-of-range offsets.** The proxy asserts `offset >= 0 && offset + 4 <= 0x340` in Debug; a hit means an existing out-of-bounds bug, which is reported, not masked.
3. **Compound operators on the proxy with mixed signedness** (`b.i32(x) += uint32_t(...)`) must keep 32-bit wraparound. Do the arithmetic in `uint32_t` and convert back.
4. **Performance of the tick path.** `memcpy` of 4 bytes compiles to one load or store at -O2; confirm with `race-soak` wall time (±5%).
5. **Release-only miscompiles hidden today.** The Debug-versus-Release hash test (Task 1) must run in CI, not just once.

---

### Task 1: `I32Field` proxy and the Debug/Release determinism test

**Wave:** W2 · **Owns:** `src/bike.hpp`, `src/bike.cpp`, the call-site fixes the compiler demands (expected: bike.cpp only), `tests/bike_field_test.cpp`, `tools/determinism_check.sh` · **Profile:** sim-sensitive

**Files:**
- Modify: `src/bike.hpp:33-40`, `src/bike.cpp:252`
- Create: `tests/bike_field_test.cpp`, `tools/determinism_check.sh`
- CMake hunk: add `bike_field` to the test list.

**Interfaces:**
- Produces:
```cpp
namespace rr {
class I32Field {   // memcpy-backed lvalue for a 32-bit field in byte storage
public:
    explicit I32Field(uint8_t* p) : p_(p) {}
    operator int32_t() const { int32_t v; std::memcpy(&v, p_, 4); return v; }
    I32Field& operator=(int32_t v) { std::memcpy(p_, &v, 4); return *this; }
    I32Field& operator=(const I32Field& o) { return *this = int32_t(o); }
    I32Field& operator+=(int32_t v) { return *this = int32_t(uint32_t(int32_t(*this)) + uint32_t(v)); }
    I32Field& operator-=(int32_t v) { return *this = int32_t(uint32_t(int32_t(*this)) - uint32_t(v)); }
private:
    uint8_t* p_;
};
}
```
  `Bike::i32(int offset)` (non-const) returns `I32Field`, with
  `assert(offset >= 0 && size_t(offset) + 4 <= m.size());` in both overloads.
  Note that `+=` was formerly signed `int32_t` addition, which is UB on
  overflow. The `uint32_t` form defines the wraparound the original x86 code
  had. This is not a behaviour change for any non-overflowing value.

- [x] **Step 1: Baselines.** Save `race-soak -V` output and `oracle-diagnostic.json` to `analysis/review-27-1/before/`. Build a Release tree `build-wt-27-1-rel` (`-DCMAKE_BUILD_TYPE=Release`) and save its `race-soak -V` output too. Note whether the Debug and Release hashes already agree. If they differ, **stop and report**: that is a live miscompile, and the orchestrator escalates it.

- [x] **Step 2: Write the failing test** `tests/bike_field_test.cpp`:

```cpp
#include "bike.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    static_assert(!std::is_reference_v<decltype(std::declval<rr::Bike&>().i32(0))>,
                  "non-const i32 must not hand out an int32_t& into byte storage");
    rr::Bike b;
    b.i32(0x1c) = 0x12345678;
    check(b.m[0x1c] == 0x78 && b.m[0x1f] == 0x12, "little-endian store");
    b.i32(0x1c) += 8;
    check(int32_t(b.i32(0x1c)) == 0x12345680, "+=");
    b.i32(0x20) = std::numeric_limits<int32_t>::max();
    b.i32(0x20) += 1;   // x86 wraparound, defined here
    check(int32_t(b.i32(0x20)) == std::numeric_limits<int32_t>::min(), "wraparound +=");
    b.i32(0x24) = b.i32(0x1c);   // proxy-to-proxy assignment copies the value
    check(int32_t(b.i32(0x24)) == 0x12345680, "copy between fields");
    const rr::Bike& cb = b;
    check(cb.i32(0x24) == 0x12345680, "const read");
    if (failures == 0) std::cout << "bike_field OK\n";
    return failures ? 1 : 0;
}
```

- [x] **Step 3: Run it and confirm it fails** (the `static_assert` fires).

- [x] **Step 4: Implement** the proxy in `bike.hpp` (add `#include <cassert>` and `<cstring>`), and switch the non-const `i32` to `I32Field i32(int offset) { assert(...); return I32Field(m.data() + offset); }`. Build everything: `cmake --build build-wt-27-1 --parallel 2>&1 | tee /tmp/27-1-build.log`. Fix each compile error **minimally**:
  - `int32_t& gear = b.i32(0x23c);` → `int32_t gear = b.i32(0x23c);`, plus an explicit `b.i32(0x23c) = gear;` at every point where the function previously wrote through `gear` (read the whole function; writes may be conditional).
  - Calls that pass `b.i32(x)` to an `int32_t&` parameter: introduce a local, call, write back.
  - Ambiguous operator errors (`std::min(b.i32(a), c)`): wrap the read as `int32_t(b.i32(a))`.
  List every fixed site in the handoff.

- [x] **Step 5: Determinism script** `tools/determinism_check.sh`:
```sh
#!/bin/sh
# Same race in Debug and Release must hash identically (race-soak prints hash=...).
set -eu
dbg=${1:?debug build dir}; rel=${2:?release build dir}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/rr-det.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
# Capture exit codes explicitly: a crashing soak must not look like "no hashes".
"$dbg/race_soak-test" > "$tmp/dbg.log" || { rc=$?; [ $rc = 77 ] && exit 77; echo "debug soak failed ($rc)"; exit 1; }
"$rel/race_soak-test" > "$tmp/rel.log" || { rc=$?; [ $rc = 77 ] && exit 77; echo "release soak failed ($rc)"; exit 1; }
grep -o 'hash=[0-9a-f]*' "$tmp/dbg.log" | sort > "$tmp/a"
grep -o 'hash=[0-9a-f]*' "$tmp/rel.log" | sort > "$tmp/b"
[ -s "$tmp/a" ] || { echo "no hashes printed"; exit 1; }
if ! cmp -s "$tmp/a" "$tmp/b"; then echo "Debug/Release simulation differs"; diff "$tmp/a" "$tmp/b"; exit 1; fi
echo "determinism OK ($(wc -l < "$tmp/a") hashes)"
```

- [x] **Step 6: Verify.** Run `bike_field-test`, the full `ctest` (Debug and ASan trees), and `sh tools/determinism_check.sh build-wt-27-1 build-wt-27-1-rel` (`determinism OK`). Compare against the Step 1 baselines; they must be identical. `race-soak` wall time must be within ±5% of baseline in the Release tree.

- [x] **Step 7: Commit.** `bike: memcpy-backed field proxy (no aliasing UB); Debug/Release determinism check`. The orchestrator adds `tools/determinism_check.sh` to `tools/ci.sh` behind `RR_DETERMINISM=1`, since it needs two trees.

---

### Task 2 (decision-gated): Named field constants

**Wave:** W3 · **Owns:** `src/bike_fields.hpp` (new) · **Profile:** research + mechanical

**Files:**
- Create: `src/bike_fields.hpp`

**Interfaces:**
- Produces: `namespace rr::bike_field { constexpr int position_1c = 0x1c; ... }`, one constant per field whose meaning is already documented in a comment in `race.hpp`, `bike.hpp`, `bike.cpp`, `race.cpp` or `docs/REVERSE_ENGINEERING.md`. Undocumented offsets get **no** constant and stay as hex literals.

- [ ] **Step 1: Inventory.** `rg -oN "\.(i32|u8)\(0x[0-9a-f]+\)" src | sed -E 's/.*\((0x[0-9a-f]+)\)/\1/' | sort | uniq -c | sort -rn > analysis/review-27-2/offsets.txt`. For each offset with at least 3 uses, find its documented meaning with `rg -n "\+0x1c\b|0x1c[^0-9a-f].*(position|lateral|speed)" src docs/REVERSE_ENGINEERING.md`, and record the evidence line next to the offset.
- [ ] **Step 2: Write the header.** Each constant gets a one-line comment that cites its evidence:
```cpp
#pragma once
// Bike/actor object field offsets (0x340-byte object). Name = meaning + hex offset;
// only fields with documented evidence are named. Evidence: file:line or doc anchor.
namespace rr::bike_field {
constexpr int dt_18        = 0x18;   // tick delta (race.cpp Race::step countdown)
constexpr int position_1c  = 0x1c;   // Q8 route position (race.hpp Race::position)
constexpr int lateral_28   = 0x28;   // Q8 lateral (race.hpp Race::lateral)
constexpr int speed_ec     = 0xec;   // speed (race.hpp Race::speed)
constexpr int gear_23c     = 0x23c;  // gear (race.hpp Race::gear)
constexpr int crashed_2f4  = 0x2f4;  // nonzero while crashed (race.hpp Race::crashed)
constexpr int condition_2f0= 0x2f0;  // bike condition; 0 = wrecked (race.hpp Race::wrecked)
// ... continue from the Step 1 inventory; one line per documented field
}
```
  The seven entries shown are confirmed by the accessors in `race.hpp`. Every further entry must cite equivalent evidence.
- [ ] **Step 3:** Include the header from `bike.hpp`. Build. Commit: `bike: named field offsets (evidence-cited, hex kept in names)`.

---

### Task 3 (decision-gated): Fan-out replacement by file group

**Wave:** W4 · **Profile:** mechanical, **parallel**: one implementer per group, all on disjoint files, all sim-sensitive.

| Group | Files (Owns) |
| --- | --- |
| A | `src/race.cpp` lines 1–1100 (worker A1) |
| B | `src/race.cpp` lines 1101–end (worker B1); rebases after A1 merges, so serialize A1 → B1 if the diff touches shared helpers |
| C | `src/bike.cpp`, `src/crash.cpp`, `src/police.cpp` |
| D | `src/traffic.cpp`, `src/race_pedestrians.cpp`, `src/pedestrians.cpp` |
| E | `src/game_main.cpp`, `src/sound.cpp`, `src/hud.cpp`, `src/view_state.cpp`, `src/actor_*.cpp` |
| F | `tests/*.cpp` (after A–E merge) |

Each worker:
- [ ] **Step 1:** In its files only, replace `.i32(0xNN)`/`.u8(0xNN)` with `.i32(bike_field::<name>_NN)` for every offset that has a constant, using a scripted, reviewable substitution:
```sh
python3 - "$@" <<'PY'
import re, sys, pathlib
consts = dict((int(v,16), n) for n, v in re.findall(r"constexpr int (\w+)\s*=\s*(0x[0-9a-f]+);",
              pathlib.Path("src/bike_fields.hpp").read_text()))
for f in sys.argv[1:]:
    p = pathlib.Path(f); s = p.read_text()
    s2 = re.sub(r"\.(i32|u8)\((0x[0-9a-f]+)\)",
                lambda m: f".{m[1]}(bike_field::{consts[int(m[2],16)]})" if int(m[2],16) in consts else m[0], s)
    p.write_text(s2)
PY
```
  This substitution also hits non-Bike objects that expose `i32(int)` on another layout (for example `TrafficCar` or crash motion) if they have such accessors. Before running it, check with `rg -n "int32_t i32\(int" src` that `Bike` is the only type with `i32(int)`. If another type exists, restrict the regex to receivers known to be `Bike`, and review each hunk.
- [ ] **Step 2:** Build; `ctest`; compare `race-soak` and `oracle-diagnostic.json` with the Task 1 baseline (identical).
- [ ] **Step 3:** Commit: `<area>: named bike fields (group X)`.
