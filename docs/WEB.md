# Web port (plan 23): toolchain, build and measurements

Status of the browser (WebAssembly) build. The plan lives in
[plans/23-web-port.md](plans/23-web-port.md); this file records what Task 1
established. The deployed page, storage and main-loop rework are Tasks 3–9
and do not exist yet.

## Toolchain (Task 1, Step 1)

- **emsdk 6.0.10**, installed locally at `.tools/emsdk` (clone, `--depth 1
  --branch 6.0.10`), installed and activated there; nothing installed
  globally. Owner-approved (plan's Global Constraints) on October 12, 2026.
- Pinned by the plan's rule (newest release at least two weeks old on
  October 12, 2026): 6.0.10 was released 2026-09-21; 6.0.11 (2026-10-06)
  and 6.0.12 (2026-10-08) were too new.
- `emcc 6.0.10 (d6c521a7f05449857c76bd99e396895583cf2083)`.
- **Node v24.19.0** bundled with the emsdk
  (`.tools/emsdk/node/24.19.0_64bit/bin/node`); the system Node (v25.8.1)
  is not used. `CMAKE_CROSSCOMPILING_EMULATOR` points at the bundled Node.
- **SDL 3.4.8**, the same pinned release archive and SHA-256 as the macOS
  bundle build (`cmake/sdl3_static.cmake`, reused verbatim). Emscripten
  backend drivers detected at configure time: video `dummy emscripten
  offscreen`, render `ogl_es2`, audio `disk dummy emscripten`, joystick
  `emscripten virtual`.
- Requires Python ≥ 3.10 to run emsdk itself (the Xcode `/usr/bin/python3`
  is 3.9); Homebrew Python works, and emsdk then uses its own Python 3.13.

## Build (Task 1, Step 2)

- `cmake/emscripten.cmake` holds the web settings; `CMakeLists.txt` has an
  `if(EMSCRIPTEN)` branch. Configure with:
  `emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release`
  (source `.tools/emsdk/emsdk_env.sh` first).
- Task 1 scope: `rr-resource` and the tests only. The game executables
  (`rr-game`, `rr-viewer`, `rr-import`, `rr-course`) keep blocking main
  loops and are skipped for `EMSCRIPTEN` until the SDL main-callback
  rework (Task 3).
- Release flags: `-O3 -flto -msimd128`, `-sALLOW_MEMORY_GROWTH=1`,
  `-sENVIRONMENT=node`, `-sDISABLE_EXCEPTION_CATCHING=0`.
- Compile issues found and fixed:
  1. **Exception catching must be a compile option, not just a link
     option.** Emscripten 6.0 emits no landing pads unless
     `-sDISABLE_EXCEPTION_CATCHING=0` is on the `em++` command line at
     compile time (verified: objects lack `__cxa_begin_catch` /
     `__cxa_find_matching_catch` references without it). With the flag at
     link time only, every `catch` in the ported code is dead and any
     thrown `std::runtime_error` escapes to JS as an unhandled promise
     rejection. Fixed in `cmake/emscripten.cmake` (`add_compile_options`
     and `add_link_options`).
  2. No ported-code changes were needed otherwise: `std::filesystem`
     (`importer.cpp`), `SDL_IOStream`, fixed-width integers and the ISO
     parser all compile for wasm32 as-is.
- Known 32-bit risk for Task 2/3 (not yet hit): `long` is 32 bits under
  wasm32 vs 64 on macOS ARM64. `play_video(..., long ticks)` and
  `run_game(..., long menu_frames)` in `src/game_main.cpp` should move to
  explicit widths when Task 3 touches those signatures. `Iso9660::ReadAt`
  already uses `uint64_t` offsets.

## Tests under Node (Task 1, Step 3)

`ctest --test-dir build-web` (emsdk Node 24.19.0), October 12, 2026:
**63 tests: 46 pass, 12 skip, 5 fail.**

Skips (all exit 77, host disc/oracle goldens invisible to the wasm
sandbox without `-sNODERAWFS`; for Task 2): `oracle-patch`,
`oracle-road`, `oracle-scene`, `oracle-raster`, `polygon`, `text`,
`frontend`, `sound`, `music`, `pe-strings-corpus`, `career-corpus`,
`wav-corpus`.

Failures (all pass in the native Release build in `build-bench/`; all
Task 2 work):

| Test | Symptom under Node | Cause / Task 2 fix |
| --- | --- | --- |
| `oracle-sim` | `verify_sim.py` fails: `[Errno 13] Permission denied: 'build-web/oracle-sim-test.js'` | The script executes the `.js` directly; it must run it through Node (emulator prefix) with `-sNODERAWFS` goldens (Task 2 Steps 1–2). |
| `race` | Uncaught C++ exception before any output; native passes | Genuine wasm32 portability bug in the pure-synthetic sim path (no files involved). Fix in shared code with a native pinning test (Task 2 Step 2). |
| `importer` | Uncaught exception; native passes | `importer_test` spawns `std::thread` (FIFO test); the single-threaded build has no pthreads. Build tests with `-pthread` for Node, or gate the thread path (Task 2 Step 1). |
| `sim_fields` | Uncaught exception, no output; native prints "sim field tables agree" | Same shape as `race` (touches `race.hpp`); possibly the same root cause (Task 2 Step 2). |
| `mixer-device` | `mixer-test --device` prints "No audio context available" and fails | SDL audio under Node; skip without a device or drive the dummy driver (Task 2 Step 1). |

## Measurements (Task 1, Step 4)

`rr-bench-web` (`tests/bench_web.cpp`, synthetic only): 20,000 renders of
the `world_renderer_test` scene plus 20,000 decodes of a synthetic 4×4
Cinepak frame. Built natively (`-DRR_BENCH_WEB=ON`, Release, Apple Clang)
and for wasm (`-O3 -flto -msimd128`).

| Case | Native (Release) | Wasm (Node 24.19.0) | Ratio |
| --- | --- | --- | --- |
| render 20,000× | 19 ms | 22 ms | ~1.2× |
| cinepak 20,000× | 254 ms | 162 ms | ~0.6× (wasm faster) |

Checksums identical (render 9593448, cinepak 24000000): the wasm build
computes exactly what the native build computes. The ratio is far below
the plan's 3× stop threshold, so no performance task reordering is
needed; real frame-time measurement stays in Task 7.

Sizes (`build-web/rr-bench-web.*`, code only — the bench links the whole
`rr-resource` but only runs two subsystems):

| File | Raw | gzip -9 | brotli -11 |
| --- | --- | --- | --- |
| `rr-bench-web.js` | 14,125 B | — | — |
| `rr-bench-web.wasm` | 54,538 B | 23,151 B | 19,879 B |

## Notes for later tasks

- Every test binary exits through Emscripten's async `run()` glue; a
  nonzero `main` return (including skip-77) still propagates correctly
  under both Node 24 and 25, but `--output-on-failure` logs are huge:
  Node prints the entire single-line `.js` as the error's source
  context. Grep past it.
- `build-web/` is the web build directory (git-ignored, like the other
  `build*/` trees). `RR_BENCH_WEB` stays OFF everywhere except the web
  toolchain file and explicit bench configures.
