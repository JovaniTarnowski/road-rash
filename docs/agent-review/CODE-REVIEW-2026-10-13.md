# Native-port code review — October 13, 2026

Baseline: `57e6b3b` plus another session's uncommitted C++20 switch
(`CMakeLists.txt`, `tests/view_test.cpp`; plan 22 Task 1, not touched here).
Scope: `src/` (153 files, 21.4k lines), `tests/` (83 files, 9.8k lines), build,
CI, tooling and docs. Fidelity to the original was not re-measured; this review
covers code health, correctness of the native shell and the engineering
workflow. Findings are labelled **confirmed** (read in code and/or reproduced)
or **plausible** (mechanism confirmed, symptom not reproduced).

## Evidence collected

- Private Debug tree with `-Wall -Wextra -Wshadow -Wconversion -Wsign-compare
  -Wno-sign-conversion` (scratch build, not committed): builds; **105 unique
  first-party warnings** (49 in `src/`, the rest in `tests/`), no errors.
  Vendor headers (`stb_*`) add 150 more because `third_party` is not a
  `SYSTEM` include.
- CTest in that tree: see "Test run" at the end.
- Read in full or in the relevant parts: `race.hpp`, `race.cpp` (step, AI
  overtake, crash stand-ins), `game_main.cpp` (`Game`, `run_race`),
  `race_setup.*`, `oracle_sim_test.cpp` (bundle), `render_threads.*`,
  `raster_rgb.cpp`, `mixer.hpp`, `session.hpp`, `settings.cpp`,
  `frontend.cpp` (save path), `importer.cpp`, `import_job.cpp`, `iso9660.cpp`,
  `bike.hpp`, `CMakeLists.txt`, `Makefile`, `tools/ci.sh`, docs/plans index.

## Findings

### P0: the game does not race with the code the oracle verifies (confirmed)

There are three independent race-construction paths:

| Hook on `rr::Race` | `Game::restart` (game_main.cpp:243) | `make_race_setup` (race_setup.cpp) | `make_race_bundle` + harness (oracle_sim_test.cpp) |
| --- | --- | --- | --- |
| personalities + EXE image (ctor) | **missing** | yes | yes |
| `set_finish_q8` | **missing** | yes | (golden-driven) |
| `set_ride_library` (attack timing) | **missing** | **missing** | yes |
| `set_nitro` | **missing** | **missing** | yes |
| `set_track_ring` (ring/DZHR) | **missing** | **missing** | yes |
| terrain, traffic, pedestrians, crash library | yes | yes | yes |

Consequences in the playable game:
- AI brains get no personality data (`brain_from` is skipped), so rating
  tables, easing steps and per-level speed tables are zero.
- `ai_can_overtake` (0x004092e0) reads the 0x00465a10 eagerness table only
  when `personalities_exe_` is non-empty, so in the game `table == 0` and
  overtakes never latch.
- Attacks use the legacy tick-7 stand-in instead of descriptor timing; nitro
  is unset; pedestrians use the precomputed walk instead of the ring; the race
  ends at the route end instead of the original finish.

`race_soak` exercises `make_race_setup`, and `oracle-sim` exercises its own
bundle, so **neither test covers what players run**. → Plan 26.

### P1: correctness

1. **Restart after the finish is dead code (confirmed regression).** `aa96b53`
   ("in-race pause menu") removed `if (restart) g.restart();`. `run_race` still
   sets `restart` from Enter, gamepad South and a tap (game_main.cpp:1047, 1082,
   1101, 1107) but never reads it (`-Wunused-but-set-variable`). README.md:45
   and USER_GUIDE.md:73 still document the feature. → Plan 25 Task 1.
2. **An exception in a render worker terminates the process (confirmed).**
   `RenderThreads::Impl::loop` calls `work()` with no handler. Band jobs throw
   `std::runtime_error` (e.g. an unspecialized `0x2000` rider texture,
   render_threads.cpp:112). With one thread the error propagates to the caller;
   with two or more `std::terminate` runs. → Plan 25 Task 2.
3. **The full-colour mip cache is keyed by raw pointer and is never invalidated
   (confirmed code; stale-texture symptom plausible).**
   `rgba_mips` (raster_rgb.cpp:207) keys on `(const CelTexture*, generation)`,
   but `generation` is the constant `0` (line 388). `ActorNodes`, `CourseScene`
   and their textures are rebuilt by every `load_course`, so a new texture can
   reuse a freed address and receive the old texture's RGBA mips. Nothing
   evicts entries, so memory grows with every course load. → Plan 25 Task 3.
4. **A clock reset consumed during event polling skips the post-effect reset
   (confirmed).** `run_race` calls `take_clock_reset()` twice. The event-loop
   call (around line 1113) resets the clock but not `g.post_teleported`, and
   the second call (line 1167) then finds the flag already cleared. After a
   resume, motion blur blends the pre-pause history once. → Plan 25 Task 1.
5. **Career saves are written in place (confirmed).** `Frontend::save_current`
   truncates the `.RRS` and writes it directly. If the process is killed
   mid-write (common on Android), the save is lost. `save_settings` already
   uses temp-file plus rename; `upscale_cache` has a third copy of that
   pattern. → Plan 25 Task 5.
6. **`Bike::i32` writes through `reinterpret_cast<int32_t*>` into
   `std::array<uint8_t,0x340>` (confirmed; no observed miscompile).** This is
   strict-aliasing undefined behaviour and is unchecked against `0x340`. The
   const overload correctly uses `memcpy`. There are 1,059 `.i32(0x…)` sites in
   `src/`. Simulation determinism across Apple clang, NDK clang and Emscripten
   depends on the optimiser not exploiting the UB. → Plan 27.

### P1: architecture

7. **`game_main.cpp` (1,801 lines) mixes app shell, race setup, drawing and
   input.** Inside `run_race` (~290 lines), the pause, quit-confirm and restart
   logic is written three times (keyboard, gamepad, touch); finding 1 hid in
   that duplication. SDL textures and the gamepad are raw handles released at
   the end of the function, and three `throw`s inside the loop leak them. Plan
   23 Task 3 (SDL callbacks, screen state machine) will move this code; it is
   much safer after an input-action layer and RAII handles exist. → Plan 28.
8. **`rr::Race` is a god object** (~100 members covering AI, pedestrians,
   hazards, nitro, crash, camera and scan list). It carries legacy aliases
   (`Rider::invulnerable_until` aliases `hit_until`; `attack_hit_tick` aliases
   `attack_legacy_hit_tick`), a duplicated 7-line comment on `set_demo_hold`,
   dead locals (`race.cpp:1498 next`, `void* u`, `1718 b`), and a full copy of
   the EXE image (`personalities_exe_`) only to read one 5-word table.
   Splitting the class is deferred until plan 26 makes one construction path;
   plan 25 Task 6 removes the dead parts.

### P2: performance (enhanced paths and tests)

9. **The full-colour raster ignores the thread pool**
   (`render_nodes_rgb_impl`: `(void)pool;`), so the full-colour, bilinear and
   fog path runs on one thread.
10. **Per-node, per-frame allocations in the full-colour path.** An upscaled
    texture is converted to RGBA for every node in every frame
    (`rgba_mips_for_test` on the hot path, raster_rgb.cpp:466). Every flat quad
    builds a `std::vector<RgbaMip>{flat}`.
11. **Per-band, per-frame allocations in the indexed threaded path.**
    `RasterScratch64` is sized to the frame height for every band in every
    frame, and each band copies a `std::function`.
    → Plan 29 (9–11), measured with the existing `rr-bench-web`/bench harness.
12. **`hires-render` takes 177 s in Debug** (`raster-rgb` 18 s,
    `oracle-scene` 16 s) and runs in every `ctest`. No CTest labels exist, so
    there is no fast subset for iteration. → Plan 30.

### P2: build, test and tooling

13. **About 80 test executables**, one per file, each statically linking
    `rr-resource`. Fourteen `build*` trees in the checkout total about 3.5 GB,
    with no tool to list or prune unowned trees (the claims registry already
    knows which trees are live). → Plan 30.
14. **Fifty test files define their own `check` lambda**, with
    inconsistent output (`FAIL what` versus bare `what`). A 20-line shared
    `tests/check.hpp` keeps the no-framework policy. → Plan 30.
15. **Warnings:** only 105 first-party warnings at a strict level, so moving
    to zero plus `-Werror` (plan 22 Task 5) is cheap. Two warnings are real
    defects (finding 1 and the unused `quad` helper). `third_party` should be
    `SYSTEM`. → Plan 30 Task 3 feeds plan 22 Task 5.
16. **Duplicate low-level code:** six private `le32` helpers
    (career, bike, frontend, import_job, iso9660, upscale_cache), and ISO 9660
    parsing implemented twice (`iso9660.cpp` and `import_job.cpp`, whose
    comment says its rules "mirror" the other). → Plan 30 Task 4.

### P3: documentation drift

17. The `PROGRESS.md` header and status table describe an older state: C++17,
    "no police", "placeholder renderer" for the other courses, "plans 10–15".
    The "Where things stand" section of `00-INDEX.md` is dated October 11.
    There are 105 handoffs in `docs/agent-handoffs/` with no merged/archived
    marker. `CURRENT_STATUS.md` is generated, but the narrative docs are not
    tied to it. → Plan 31.

## What is in good shape

- Ported code is consistently annotated with original addresses, and
  integer semantics are explicit. Strict conversion warnings are nearly absent
  in `src/`.
- Parsers validate bounds and throw `std::runtime_error`. ISO path components
  reject `.`, `..` and separators. The importer stages and renames atomically.
- The mixer is lock-free with a bounded command queue; lifecycle state is
  atomic.
- The evidence and gate system (receipts, claims, `verify_sim.py` diagnostic
  versus acceptance) is unusually rigorous.

## Plans produced

| Plan | Scope | Findings |
| --- | --- | --- |
| [25](../plans/25-review-correctness-fixes.md) | Small correctness fixes | 1–5, 8 (dead parts) |
| [26](../plans/26-one-race-construction-path.md) | One race factory used by game, soak and oracle | P0 |
| [27](../plans/27-safe-object-field-access.md) | Defined-behaviour field access, then named offsets | 6 |
| [28](../plans/28-race-shell-input-and-raii.md) | Input actions, RAII SDL handles, extracting the race screen | 7 |
| [29](../plans/29-enhanced-path-performance.md) | Threaded, allocation-free full-colour path | 9–11 |
| [30](../plans/30-test-and-build-infrastructure.md) | Labels, shared checks, warnings, dedupe, build pruning | 12–16 |
| [31](../plans/31-docs-and-status-hygiene.md) | Status docs and handoff archive | 17 |

Execution order and the subagent protocol are in
[../plans/REVIEW-ORCHESTRATION.md](../plans/REVIEW-ORCHESTRATION.md).

## Test run

Private Debug tree, strict warnings, C++20 working tree, disc present:
**81/81 passed, 0 skipped**, 566 s serial. Slowest: `hires-render` 177 s,
`race-soak` 176 s, `course_scene` 54 s, `race_setup` 35 s, `raster-rgb` 18 s,
`widescreen` 17 s, `oracle-scene` 16 s. The ASan, TSan, Release and Android
trees were not run in this review; their latest receipts are in
`docs/CURRENT_STATUS.md`. A green suite does not prove simulation parity
(`verify-sim` acceptance is a separate, currently failing gate).

## Addendum (October 13, 2026): sanitizer findings from the C++20 session

Running the sanitizer gate (`build-san`, `-fsanitize=address,undefined`) on the
C++20 tree surfaced three heap-buffer-overflow reports; all three were
re-confirmed on the pristine C++17 tree (`57e6b3b` + `build-san17`), so they
are pre-existing defects, not C++20 fallout. Tracked as plan 25 Task 7.

1. `texture_edge_wide` (`src/texture.cpp:107-120`): ASan `__asan_memcpy` WRITE
   of size 24 past the end of a 46,080-byte `RasterScratch64` region, reached
   from `draw_textured_quad_wide` (`texture.cpp:154`) via `render_nodes_impl`.
   Aborts `hires-render` and `race-soak` under ASan.
2. Same site via `race-soak`: same root cause.
3. `tests/widescreen_test.cpp:80`: READ of size 4 past a `Rider` vector region —
   the test reads `riders()[1]` but constructs `Race` with no opponents (test-only OOB).
