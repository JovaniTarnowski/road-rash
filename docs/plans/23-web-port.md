# Browser Port Implementation Plan (WebAssembly)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Independent of the gameplay plans: the browser build runs whatever state the shared game code is in. Task 3 (main-loop refactor) touches `src/game_main.cpp`, which other sessions edit often; coordinate before starting it (see Global Constraints).

**Goal:** Run the same native game in a desktop browser (Chrome, Firefox, Safari) as WebAssembly. The player opens a web page, picks their own Road Rash ISO (or the `ROADRASH` folder from the disc), the page imports it into the browser's private storage once, and the game plays with keyboard, gamepad or touch, with sound, video and saves. The page hosts only our code; no game data is ever uploaded or served.

**Investigation summary (October 12, 2026):**

| Question | Finding | Consequence |
| --- | --- | --- |
| Toolchain | Emscripten is not installed. SDL 3.4.8 is already fetched from source and pinned by hash (`cmake/sdl3_static.cmake`); SDL3 supports Emscripten as a platform. | Install a pinned emsdk under `.tools/`; build SDL from the same pinned archive. |
| Main loop | `game_main.cpp` has four blocking `while (running)` loops (video, race, front end, viewer) plus one in `import_ui.cpp`, with `SDL_Delay`. A browser page cannot block its main thread. | Restructure into SDL3 main callbacks (`SDL_AppInit`/`SDL_AppIterate`/`SDL_AppEvent`/`SDL_AppQuit`), one state machine, used on **every** platform so desktop tests keep covering it (Task 3). ASYNCIFY or JSPI could avoid the refactor but cost speed or browser support; not chosen. |
| Import | `import_game` reads the ISO synchronously through `Iso9660(ReadAt)` on a worker `std::thread`. Browser `File` reads are asynchronous on the main thread. ISO 9660 files are contiguous extents. | A pull-based import job that asks for byte ranges and is fed them, driven synchronously on desktop/Android and asynchronously in the browser (Task 4). |
| Data size | Imported data is 467 MB: VIDEO 296 MB, AUDIO 146 MB, everything else about 25 MB. The ISO is 518 MB. | Keep the ~25 MB core resident in memory; load each video and soundtrack file on demand from persistent browser storage before it plays (Task 5). |
| Storage | Assets are read with synchronous whole-file reads (`Assets::read`); saves and settings live under `SDL_GetPrefPath`. | Imported data in the Origin Private File System (OPFS) or IndexedDB; saves/settings in a small persistent filesystem synced after writes (Task 5). |
| Threads | Today: the import worker, SDL's audio callback, event watches. Plan 19 adds render threads. Wasm threads need `SharedArrayBuffer`, which needs cross-origin isolation headers (COOP/COEP) from the host. | The baseline web build is **single-threaded** (works on any static host). A threaded build is optional, for higher render scales (Task 7). |
| Audio, input | SDL3's Emscripten backend provides audio streams with callbacks, the Gamepad API, keyboard, touch and fullscreen. Browsers only start audio and fullscreen after a user gesture; Escape leaves fullscreen. | A "click to start" screen; pause also on P/Backspace/Start in the browser (Task 6). |
| Speed | The rasteriser is portable integer C++. Wasm typically runs CPU-bound integer code at roughly 1.3–2× native time. 640×480 at 60 Hz is a small load on desktop hardware. | Measure in Task 1; `-O3 -msimd128 -flto`; render scale above 1 only where measured. |
| Legal | The import is local; the APK rule (never contain game data) applies to the web bundle too. | The deploy check rejects any game file in the bundle (Task 9). |

**Architecture:**
- One code base, one more platform. The browser build is the existing game compiled with Emscripten and linked against SDL3 built for Emscripten, plus a small JavaScript shell (`web/`) for the file picker, storage and start screen.
- `src/app_main.cpp` replaces the body of `main()` with SDL main callbacks and an explicit `AppState` (Import → Title → Front end → Video → Race → Quit) used on macOS, Android and the web.
- `rr::ImportJob` (pull-based) replaces the blocking import loop; `rr::AssetStore` adds asynchronous prefetch in front of `Assets` for large files.
- Tests run under Node.js (Emscripten's `CMAKE_CROSSCOMPILING_EMULATOR`), including the golden suites with `NODERAWFS`, which proves the wasm build computes exactly what the native build computes.

**Tech Stack:** Emscripten (emsdk, pinned version), SDL 3.4.8 (pinned archive), CMake + `emcmake`, Node.js (bundled with emsdk), Playwright (pinned; browser smoke tests), C++17/20.

**Spec:** the owner's request of October 12, 2026: "make this playable in browser, perhaps using wasm; investigate and write a plan".

## Global Constraints

- **No game data on the web.** The deployed bundle contains only our HTML, JS, wasm and licences. The user's data is imported in their browser and stays there. Nothing is uploaded. The page says so.
- **One behaviour.** The web build runs the same simulation and renderer code as native; the golden and tick tests must pass under Node with identical results (Task 2).
- **No regression elsewhere.** After Task 3 the macOS and Android builds run through the same main-callback code; every headless screenshot and test result is unchanged.
- **Parallel sessions.** `src/game_main.cpp` is edited by other sessions. Before Task 3, run `git status`; if `game_main.cpp` has uncommitted changes you did not make, wait for them to be committed. Do Task 3 in one focused session and commit promptly.
- **Downloads need the owner's approval.** Ask before installing emsdk, Playwright browsers or anything else from the network, and record versions and hashes in TOOLING.md.
- **Publishing needs the owner's approval** (Task 9): choosing a host and deploying is outward-facing.

## Review Focus

1. **The tab is hidden or the browser throttles timers** (`requestAnimationFrame` stops): the race must pause like an Android background event, not run 600 catch-up ticks when the tab returns. Task 6 tests a hidden/visible cycle.
2. **Storage quota, eviction or private browsing**: an import that cannot fit, or data the browser evicted, must give a clear message and a way to re-import, never a broken game. Task 5 tests a quota failure and a missing file after eviction.
3. **Selecting the wrong file** (another ISO, a corrupted download, a folder without `ROADRASH.EXE`): the hash check rejects it with the same messages as native import. Task 4 runs the fixture with a wrong hash.
4. **Escape and browser shortcuts.** Escape leaves fullscreen before the game sees it; Ctrl/Cmd+W closes the tab; Backspace and arrow keys scroll the page. Game keys must not scroll the page, and pausing must work without Escape. Task 6 tests key handling with `preventDefault`.
5. **Large videos on low-memory devices** (a 30 MB AVI fully loaded into the wasm heap on an iPad): memory must be released after each video and the heap must not grow without bound across a career. Task 5 measures heap size before and after 20 videos.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `cmake/emscripten.cmake` | Web target options, SDL for Emscripten, link flags |
| `src/app_main.cpp`, `src/app_state.hpp/.cpp` | SDL main callbacks and the app state machine (all platforms) |
| `src/game_main.cpp` | Loops become `iterate`/`event` functions of the states |
| `src/import_job.hpp/.cpp` | Pull-based import (byte-range requests), sync driver |
| `src/importer.cpp`, `src/import_ui.cpp` | Use `ImportJob`; UI drives it per frame |
| `src/asset_store.hpp/.cpp` | Resident core files + async prefetch of large files |
| `src/platform_web.hpp/.cpp` | Emscripten glue (`EM_JS`/`EMSCRIPTEN_KEEPALIVE`): file picker results, storage calls, visibility |
| `web/index.html`, `web/shell.js`, `web/storage.js`, `web/style.css` | Page, start screen, picker, storage, progress |
| `tools/build_web.sh`, `tools/serve_web.py`, `tools/web_smoke.mjs` | Build, local server (with optional COOP/COEP headers), browser smoke test |
| `tests/import_job_test.cpp`, `tests/asset_store_test.cpp`, `tests/app_state_test.cpp` | Tests (native and Node) |
| `docs/WEB.md`, `docs/USER_GUIDE.md`, `docs/TOOLING.md` | Docs |

---

### Task 1: Toolchain spike and measurements (time box: one session)

**Files:**
- Create: `cmake/emscripten.cmake`, `docs/WEB.md`
- Modify: `CMakeLists.txt` (an `if(EMSCRIPTEN)` branch: build `rr-resource` and the synthetic tests only), `docs/TOOLING.md`

- [ ] **Step 1: Install (ask the owner first).** Clone emsdk into `.tools/emsdk`, install and activate one pinned release (the newest release at least two weeks old), record the version in TOOLING.md. Do not install globally.
- [ ] **Step 2: Build SDL3 and the core library.** In `cmake/emscripten.cmake`, reuse the pinned SDL archive from `cmake/sdl3_static.cmake` (same URL and SHA-256) with `SDL_STATIC ON`. Configure with `emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release`. Fix whatever does not compile (expected: nothing in the ported code; possibly `std::filesystem` uses in `importer.cpp`, which Emscripten supports on its virtual filesystem).
- [ ] **Step 3: Run synthetic tests under Node.** Set `CMAKE_CROSSCOMPILING_EMULATOR` to emsdk's `node` so `ctest --test-dir build-web` runs them. Record pass/fail per test.
- [ ] **Step 4: Measure.** Build a small `rr-bench-web` executable that renders the synthetic `world_renderer_test` scene 1,000 times and decodes a synthetic Cinepak stream; run it natively and under Node; record both times, and the wasm size (`-O3 -flto -msimd128`, gzip and brotli sizes). If plan 19 has landed, also measure render scale 2.
- [ ] **Step 5:** Write `docs/WEB.md` with the toolchain version, test results, measurements and any compile issues found. If the speed ratio is worse than 3× native, stop and report before continuing: the performance task (Task 7) then comes before the others. Commit: `web: Emscripten toolchain spike, SDL3 for the web, measurements`.

---

### Task 2: The whole test suite under Node, including goldens

**Files:**
- Modify: `cmake/emscripten.cmake` (all test executables; `-sNODERAWFS=1` for tests so they read the real disc and `analysis/oracle/` goldens through Node's filesystem), `CMakeLists.txt`, `tools/ci.sh` (`RR_WEB=1` stage), `docs/VERIFICATION.md`

- [x] **Step 1:** Build every CTest executable for Emscripten. Tests that need SDL video (screenshot tests) are skipped under Node with exit 77 and a message; list them.
- [x] **Step 2:** Run `ctest --test-dir build-web --output-on-failure` with the disc mounted and the goldens present. Expected: every test that passes natively passes under Node, with the same skip set (apart from Step 1's list). A difference is a real portability bug (integer width, `size_t` being 32-bit in wasm32, undefined behaviour the native compiler happened to tolerate): fix it in the shared code, with a native test that pins the fixed behaviour.
- [x] **Step 3:** Pay special attention to `size_t`/`long` being 32 bits in wasm32: grep for `long` used as a 64-bit type and for `size_t` arithmetic on file offsets above 2 GB (none should exist, but the ISO is 518 MB and offsets are computed as `uint64_t` in `Iso9660::ReadAt`; confirm).
- [x] **Step 4:** Add the stage to `tools/ci.sh` behind `RR_WEB=1`. Record results in VERIFICATION.md. Commit: `web: full test suite and goldens pass under Node`.

---

### Task 3: SDL main callbacks and one app state machine (all platforms)

**Files:**
- Create: `src/app_main.cpp`, `src/app_state.hpp`, `src/app_state.cpp`, `tests/app_state_test.cpp`
- Modify: `src/game_main.cpp` (each loop body becomes a state's `iterate`/`event`), `src/import_ui.cpp`, `CMakeLists.txt` (`rr-game` and Android `main` use `app_main.cpp` with `SDL_MAIN_USE_CALLBACKS`)

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  enum class AppResult { keep_going, success, failure };
  class AppScreen {                         // one per former loop: Import, FrontEnd, Video, Race
  public:
      virtual ~AppScreen() = default;
      virtual void event(const SDL_Event&) = 0;
      virtual AppResult iterate(uint64_t now_ns) = 0;   // one frame: input, ticks via TickClock, draw, present
      virtual std::unique_ptr<AppScreen> next() { return nullptr; }   // the screen to switch to after this one finishes
  };
  struct AppContext;                        // window, renderer, assets, settings, career, mixer: what main() owns today
  std::unique_ptr<AppScreen> first_screen(AppContext&, const CommandLine&);   // same decisions main() makes today
  }
  ```
  `SDL_AppIterate` calls the current screen's `iterate`, swaps to `next()` on completion, and returns `SDL_APP_SUCCESS` on quit. Every `SDL_Delay` in a loop is removed: pacing comes from vsync and `TickClock`. Background handling (`LifecycleWatch`) stays, now checked at the top of `iterate`. Headless options (`--ticks`, `--menu-frames`, `--screenshot`, `--first-run-smoke`) run through the same screens with a fixed simulated clock.

- [ ] **Step 1: Record the baseline** (before any change). Run and save the BMP outputs and exit codes of: `rr-game --menu-frames 120 --screenshot`, `rr-game --race --ticks 900 --autopilot --screenshot`, `rr-game --video VIDEO/INTRO.AVI --ticks 300 --screenshot`, `rr-game --first-run-smoke`, and every CTest. Store hashes in `analysis/app-callbacks-before/`.
- [ ] **Step 2: Write the failing test** `tests/app_state_test.cpp` (synthetic, no window): a fake screen that finishes after 3 iterations and names a second fake screen; the driver calls `iterate` exactly 3 times on the first, switches, and stops when the second returns `success`. An `SDL_EVENT_QUIT` delivered to `event` ends the run with `success` on the next iterate.
- [ ] **Step 3: Refactor.** Move each loop body into its screen class without changing what it does per frame. The race loop's tick/present code moves verbatim into `RaceScreen::iterate`. The front end's inner `fe.poll` loop and its calls to `play_video` and `run_race` become screen switches with the same follow-up logic (`race_finished`, standings).
- [ ] **Step 4: Compare.** Re-run Step 1 into `analysis/app-callbacks-after/`; every hash and exit code must match. Build Android debug and release (`tools/ci.sh`).
- [ ] **Step 5: Manual check.** Play from the title through one race and back to the menu on macOS. Commit: `app: SDL main callbacks and one screen state machine on every platform`.

---

### Task 4: Pull-based import job

**Files:**
- Create: `src/import_job.hpp`, `src/import_job.cpp`, `tests/import_job_test.cpp`
- Modify: `src/importer.cpp` (the existing `import_game` becomes a sync driver over `ImportJob`), `src/import_ui.cpp` (drives the job per frame instead of a thread on the web), `tests/importer_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct ByteRange { uint64_t offset = 0; uint32_t size = 0; };
  class ImportJob {
  public:
      // source_kind: an ISO image, or a folder whose files are read by path.
      ImportJob(std::vector<ReferenceHash> hashes, ImportSink& sink, bool iso);
      // What the job needs next: a byte range of the ISO, or a whole file by path (folder imports).
      std::optional<ByteRange> need_range() const;
      std::optional<std::string> need_file() const;
      void supply(const uint8_t* data, size_t size);   // answer to the current need
      void fail_source(const std::string& message);    // the host could not read
      bool done() const;
      const ImportResult& result() const;
      ImportProgress progress() const;
      void cancel();
  };
  // Where verified files go: native writes to the staging directory; web writes to browser storage.
  class ImportSink { public: virtual ~ImportSink() = default; virtual void begin_file(const std::string& path, uint64_t size) = 0;
                     virtual void write(const uint8_t*, size_t) = 0; virtual void end_file(bool ok) = 0;
                     virtual bool commit() = 0; virtual void abort() = 0; };
  }
  ```
  For an ISO the job first requests the volume descriptor (sector 16) and directory sectors, then each referenced file's extent in chunks of at most 1 MiB, hashing as it goes; nothing is committed unless every hash matches (same rule as today's staging directory).

- [x] **Step 1: Write the failing tests** `tests/import_job_test.cpp`, using `tests/iso_fixture.hpp` and a small test reference table:
  - a sync driver feeding exact ranges from the fixture produces the same files and `ImportResult` as today's `import_game` on the same fixture;
  - feeding ranges with random delays between `need` and `supply` (the async case, simulated by interleaving two jobs) still succeeds;
  - a fixture with one modified byte fails with that file in `mismatched` and the sink's `abort` called, `commit` never called (Review Focus 3);
  - `cancel()` mid-file aborts the sink;
  - `fail_source` ends the job with the message in `result().error`;
  - a directory record pointing past the end of the image fails cleanly (`std::runtime_error` caught into `error`, no crash).
- [x] **Step 2:** Run; expected FAIL.
- [x] **Step 3: Implement**, then reimplement `import_game` as a loop over the job with `SDL_IOStream` reads and a filesystem sink. The existing `importer_test` must pass unchanged.
- [x] **Step 4:** Run all tests natively and under Node. Commit: `import: pull-based import job shared by native and web`.

---

### Task 5: Browser storage and the asset store

**Files:**
- Create: `src/asset_store.hpp`, `src/asset_store.cpp`, `src/platform_web.hpp`, `src/platform_web.cpp`, `web/storage.js`, `tests/asset_store_test.cpp`
- Modify: `src/assets.hpp/.cpp` (backed by `AssetStore`), `src/video_player.cpp` and the soundtrack code in `src/sound.cpp` (request a prefetch, wait in a "loading" state), `src/importer.cpp` (`data_root` on the web), `src/settings.cpp`/`src/career.cpp` save paths

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  class AssetBackend {                      // native: the filesystem; web: JS storage via platform_web
  public:
      virtual ~AssetBackend() = default;
      virtual bool exists(const std::string& path) const = 0;
      // Native completes immediately; web completes on a later frame. The callback runs on the main thread.
      virtual void load(const std::string& path, std::function<void(std::optional<std::vector<uint8_t>>)> done) = 0;
  };
  class AssetStore {
  public:
      AssetStore(std::unique_ptr<AssetBackend>, std::vector<std::string> resident_prefixes);  // e.g. everything except VIDEO/ and AUDIO/
      void preload_resident(std::function<void(size_t done, size_t total)> progress, std::function<void(bool ok)> finished);
      bool ready(const std::string& path) const;            // resident or prefetched
      void prefetch(const std::string& path);               // starts a load if not ready
      std::vector<uint8_t> read(const std::string& path) const;   // throws std::runtime_error if not ready
      void release(const std::string& path);                // drop a prefetched large file
  };
  }
  ```
  Storage on the web: imported files are written by `web/storage.js` to the Origin Private File System (`navigator.storage.getDirectory()`), falling back to IndexedDB if OPFS is missing. The page calls `navigator.storage.persist()` after a successful import and shows the result. Saves and settings use an Emscripten IDBFS mount at the pref path, synced (`FS.syncfs`) after each save.

- [ ] **Step 1: Write the failing tests** `tests/asset_store_test.cpp` with a fake backend that completes loads only when the test calls `pump()`:
  - after `preload_resident`, every resident path is `ready` and `read` returns its bytes; a `VIDEO/` path is not ready and `read` throws `std::runtime_error`;
  - `prefetch` then `pump` makes it ready; `release` makes it not ready again and frees its memory (check the store's byte count);
  - a backend that reports a missing file (eviction, Review Focus 2) makes `preload_resident` finish with `ok == false` and the missing path listed;
  - 20 prefetch/read/release cycles of a 10 MB fake video leave the store's byte count at the resident size (Review Focus 5).
- [ ] **Step 2:** Run; expected FAIL. **Implement.** `VideoPlayer` and the soundtrack request a prefetch and show the existing loading state until ready; videos are released when they end.
- [ ] **Step 3: Web glue.** `platform_web.cpp` exposes `EMSCRIPTEN_KEEPALIVE` functions that JS calls with file bytes, and `EM_JS` functions that start JS loads. `web/storage.js` implements write (during import), read, list, delete-all, quota estimate. Quota failure (Review Focus 2) during import aborts the job with "Not enough browser storage: N MB needed, M MB available" and deletes the partial files.
- [ ] **Step 4:** Run the tests natively and under Node. Commit: `web: asset store with resident core and on-demand video and music`.

---

### Task 6: The web page and input

**Files:**
- Create: `web/index.html`, `web/shell.js`, `web/style.css`, `tools/serve_web.py`
- Modify: `cmake/emscripten.cmake` (`rr-game` web target with `--shell-file web/index.html`), `src/platform_web.cpp`, `src/game_main.cpp` screens (web-specific pause keys), `docs/USER_GUIDE.md`

- [ ] **Step 1: Page flow.**
  1. **Start screen:** what this is, "Bring your own disc: your files stay in this browser and are never uploaded", a **Start** button (the user gesture that unlocks audio and allows fullscreen), and a link to the licences.
  2. **First run:** "Choose your Road Rash disc image (.iso)" via `<input type=file accept=".iso">`, drag-and-drop on the page, or "Choose the ROADRASH folder" via a directory picker. The `File` is handed to the import job: `shell.js` answers each `need_range` with `file.slice(offset, offset + size).arrayBuffer()` and calls back into wasm. Progress uses the game's import screen.
  3. **Later runs:** go straight to the game; a "Manage data" link offers re-import and delete.
- [ ] **Step 2: Input.** Prevent default browser handling for every key the game binds and for Backspace/arrows/space while the canvas has focus (Review Focus 4). In the browser build, pause also maps to P, Backspace and gamepad Start, because Escape exits fullscreen first. A fullscreen button sits outside the canvas. Gamepads appear after the first button press (a browser rule); the start screen says "press any button on your controller".
- [ ] **Step 3: Visibility.** `visibilitychange` to hidden sends the same lifecycle event as Android backgrounding: race paused, audio paused, `TickClock` reset on return (Review Focus 1).
- [ ] **Step 4: Tests.** `tools/serve_web.py` serves `build-web/dist` (and, with `--isolated`, adds `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`). Manual check in the built-in browser pane: start screen, import of the real ISO, menu, one race, a video, save and reload. Write what was checked in VERIFICATION.md.
- [ ] **Step 5:** Commit: `web: page, disc import from the browser, input and visibility handling`.

---

### Task 7: Performance and the optional threaded build

**Files:**
- Modify: `cmake/emscripten.cmake` (`RR_WEB_THREADS` option: `-pthread`, `-sPTHREAD_POOL_SIZE`), `src/render_threads.cpp` (plan 19; single-threaded fallback), `tools/build_web.sh`, `docs/WEB.md`

- [ ] **Step 1:** Release flags: `-O3 -flto -msimd128`, `-sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2GB`, `-sINITIAL_MEMORY=64MB`, `-sENVIRONMENT=web`. Record wasm size (raw, gzip, brotli).
- [ ] **Step 2: Measure in browsers.** The game's `--bench` mode (plan 14 Task 2 or plan 19 Task 6) as a URL parameter `?bench=3600`, which prints p50/p95 frame times to the console and the page. Run in Chrome, Firefox and Safari on the owner's Mac at render scale 1 (and 2 if plan 19 landed). Target: p95 ≤ 16.7 ms at scale 1 in all three. Record the numbers.
- [ ] **Step 3: Threaded build (optional).** With `RR_WEB_THREADS=ON`, plan 19's band-parallel rasteriser runs on Web Workers. It needs cross-origin isolation, so it is served only by hosts that can set COOP/COEP (or with the `coi-serviceworker` approach, recorded as an option in WEB.md with its trade-offs). The page detects `crossOriginIsolated` and loads the threaded build only when available; otherwise the single-threaded build.
- [ ] **Step 4:** Commit: `web: release flags, browser frame-time measurements, optional threads`.

---

### Task 8: Automated browser smoke test

**Files:**
- Create: `tools/web_smoke.mjs`, `tools/web_smoke_test_fixture.md` (how the synthetic data set is built)
- Modify: `cmake/emscripten.cmake` (a `RR_WEB_TEST_HASHES` build flag that swaps the reference hash table for the synthetic fixture's, for tests only, never deployed), `tools/ci.sh`

- [ ] **Step 1 (ask the owner before installing Playwright and its browsers; pin the version).** `tools/web_smoke.mjs` starts `serve_web.py`, opens the page in headless Chromium and WebKit, clicks Start, imports the synthetic ISO fixture (built by a test helper from `tests/iso_fixture.hpp`) through the file input, and asserts: the import finishes with "verified", reloading the page skips the import, "Manage data → Delete" removes it. No original data is involved, so this runs in CI.
- [ ] **Step 2: Local full smoke** (`--disc ../Road\ Rash*.iso`, never in CI): import the real ISO, reach the main menu, start a quick race with `?race=napa&ticks=900&autopilot=1`, take a screenshot into `analysis/web/` (git-ignored), and compare it with the native `--race --ticks 900 --autopilot` screenshot: they must be pixel-identical (same code, same inputs).
- [ ] **Step 3:** Commit: `test: browser smoke tests (synthetic in CI, full with the disc locally)`.

---

### Task 9: Packaging and publishing (owner decision)

**Files:**
- Create: `tools/build_web.sh`
- Modify: `tools/ci.sh` (bundle content check), `docs/WEB.md`, `docs/USER_GUIDE.md`, `README.md`

- [ ] **Step 1:** `tools/build_web.sh` builds the single-threaded (and, if enabled, threaded) web target into `build-web/dist/` with content-hashed file names and an `index.html` that references them, plus `LICENSES.txt` (SDL zlib licence, stb, our licence).
- [ ] **Step 2: Bundle check.** `tools/ci.sh` fails if `build-web/dist` contains any file whose SHA-256 is in `reference_hashes.inc`, any `.AVI`/`.RRA`/`.RSC`/`.CRS`/`.CAR`/`.ISO`, or is larger than the expected wasm size plus 10 %.
- [ ] **Step 3: Choose a host with the owner** (outward-facing; do not deploy without an explicit yes). Options to present, with the trade-offs recorded in WEB.md:
  - GitHub Pages: free, static, no custom headers (single-threaded build only, or `coi-serviceworker`);
  - Cloudflare Pages or Netlify: free tiers, custom headers via a `_headers` file (threaded build possible);
  - itch.io: game-oriented, has a SharedArrayBuffer option for HTML games.
  Also confirm with the owner how the page should present the project (name, notices), since "Road Rash" is a third-party trademark; the page must not imply affiliation with Electronic Arts.
- [ ] **Step 4:** On approval, deploy, open the live URL in the built-in browser and repeat Task 6 Step 4's manual check there. Commit: `web: release bundle and deployment docs`.

## Exit criteria

- The full test suite, including tick and pixel goldens, passes under Node.
- macOS and Android run through the same main-callback state machine with unchanged outputs.
- In Chrome, Firefox and Safari on the owner's Mac: the user imports their own ISO in the page, plays a full race with sound, videos and saves, and frame times meet the target at render scale 1.
- The bundle contains no game data (checked in CI), and any public deployment happened only after the owner's approval.

## Out of scope (for now)

- Mobile browsers: they may work (touch input exists), but storage quotas and memory on iOS Safari are tight. Test after Task 8 and decide with the owner.
- Multiplayer over the web (plan 09 is decision-gated).
