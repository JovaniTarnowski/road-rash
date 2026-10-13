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

## Tests under Node (Tasks 1–2)

Task 1 (October 12, 2026) ran 63 tests: 46 pass, 12 skip, 5 fail (table
below, kept for the record). Task 2 closed all five and un-skipped the
twelve; the suite is now **68/68** (67 pass + `mixer-device` skipped 77).

| Test | Task 1 symptom under Node | Task 2 fix |
| --- | --- | --- |
| `oracle-sim` | `verify_sim.py` executed the `.js` directly (`Permission denied`) | Script runs `.js` through Node (`run_binary`/`node_prefix`: `RR_WEB_NODE`, emsdk bundle, `PATH`); binary links `-sNODERAWFS=1`. Diagnostic records the pinned `known-divergence`, same as native. |
| `race` | Stack-cookie overwrite before any output; native passes | Call chains need more than the 64 KiB default stack. `-sSTACK_SIZE=1MB` for all web links. No smash: ASan natively clean, and 1 MiB passes. |
| `sim_fields` | `RuntimeError: divide by zero`; native prints "sim field tables agree" | Genuine UB: `apply_spec` (`src/bike.cpp:104-115`) divides by zero on zeroed specs. UBSan natively reports 6 hits. Every divisor guarded to zero (= ARM64 result); real specs unaffected. Pinned by a zero-spec block in `tests/bike_test.cpp`. |
| `importer` | `FAIL create nonseekable source` | No FIFOs/pthreads in the single-threaded build. FIFO block gated out under `__EMSCRIPTEN__`; web nonseekable input arrives via the Task 4 `ImportJob`. |
| `mixer-device` | "No audio context available" | Binary exits 77 under Emscripten (no audio clock under Node) with a message; ctest maps 77 to skip. Native dummy-driver coverage unchanged. |

Former skips (all exit 77 for invisible disc/goldens) now pass via
`-sNODERAWFS=1` on every test executable (`rr_web_hostfs` in
`cmake/emscripten.cmake`, called from `CMakeLists.txt`; per-target so the
future browser game keeps the sandboxed filesystem):
`oracle-patch/road/scene/raster`, `polygon`, `text`, `frontend`, `sound`,
`music`, `pe-strings-corpus`, `career-corpus`, `wav-corpus`. Native-only by
design: `first-run-smoke` (excluded for `EMSCRIPTEN` in `CMakeLists.txt`).

Step 3 audit: `Iso9660::ReadAt` offsets are `uint64_t`; no `size_t`
file-offset arithmetic above 2 GB. The only `long`-as-counter uses are the
`game_main.cpp` tick/frame locals, owned by the Task 3 refactor. `long()`
casts elsewhere convert bounded in-memory sizes.

`tools/ci.sh` runs the web tree behind `RR_WEB=1` (`RR_WEB_DIR` overrides
the build directory).

## Import job (Task 4, October 12, 2026)

`src/import_job.hpp/.cpp`: `ImportJob` (table validation, ISO descriptor →
directory → ≤1 MiB file-chunk walk, per-file SHA-256 before delivery,
commit-only-when-clean) plus `ImportSink`. `src/sha256.cpp` grew a
`Sha256Stream` (the one-shot `sha256()` is now a thin wrapper; same
vectors). `src/importer.cpp` reimplements `import_game` as a sync driver
over the job (`FilesystemSink` keeps staging/marker/rename/cancel
semantics; cancellation still surfaces the progress callback's message).
`tests/import_job_test.cpp` covers the plan's six cases; the existing
`importer_test.cpp` passes unchanged. Full gates: native 70/70, web 69/69
(new `import_job` test in both), ASan/UBSan clean for
`importer/import_job/sha256/iso9660`. Task 3 stays blocked on another
worker's uncommitted `game_main.cpp` changes.

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

## Deployment (Vercel scaffold, October 12, 2026)

Early Task 9 Steps 1–2 scaffold; the playable game target does not exist
yet (Tasks 3–6 open), so there is nothing playable to deploy.

- `vercel.json` (repo root): `buildCommand: tools/build_web.sh`,
  `outputDirectory: build-web/dist`, static (`framework: null`), caching
  headers for future hashed `.js`/`.wasm`, no-cache `index.html`. No
  COOP/COEP headers on purpose: the baseline single-threaded build works
  on any static host without them; Task 7 adds them with the threaded build.
- `tools/build_web.sh`: writes a status placeholder `index.html` (with the
  non-affiliation notice the plan requires) plus `LICENSES.txt`, then runs
  the no-game-data bundle check (game extensions + SHA-256 against
  `src/reference_hashes.inc`). `RR_WEB_WASM=1` is reserved for the Task 3+
  game target. Verified locally: bundle builds, `vercel.json` parses,
  placeholder serves 200 over a local static server.
- `.vercelignore`: excludes local-only trees from uploads (`.tools/`
  ~50k files, `build*/`, `analysis/`, Android build outputs); without it
  `vercel` fails with "files should NOT have more than 15000 items".
  `installCommand` is a no-op (no npm dependencies).
- Not done: preview-deploy check, custom domain, `--prod`. Per plan 23,
  deploying publicly needs the owner's explicit yes (Task 9 Steps 3–4),
  including how the page names the project.

## Notes for later tasks

- Every test binary exits through Emscripten's async `run()` glue; a
  nonzero `main` return (including skip-77) still propagates correctly
  under both Node 24 and 25, but `--output-on-failure` logs are huge:
  Node prints the entire single-line `.js` as the error's source
  context. Grep past it.
- `build-web/` is the web build directory (git-ignored, like the other
  `build*/` trees). `RR_BENCH_WEB` stays OFF everywhere except the web
  toolchain file and explicit bench configures.
