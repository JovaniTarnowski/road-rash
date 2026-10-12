# Web port (plan 23) — Task 1 kickoff handoff

Date: 2026-10-12. Baseline: `b76ffc8`. Branch: `port/playable-napa` (per 00-INDEX; `git status` shows only the agent-tooling worker's uncommitted edits to `CMakeLists.txt`, `tests/agent_tooling_test.py`, `tools/agent_doctor.py`, `tools/agent_gate.py`, `tools/build_info.py`, `tools/build_macos_app.sh` — untouched).

## Static verification of the plan's investigation table (no network used)

- **Main loops (blocking):** confirmed 4 in `src/game_main.cpp` + 1 in `src/import_ui.cpp`, all with `SDL_Delay`:
  - `play_video` loop (`game_main.cpp:440`), `SDL_Delay(20)` at :473.
  - `run_race` loop (`game_main.cpp:517`), `SDL_Delay(20)` at :616.
  - `run_game` frontend loop (`game_main.cpp:717`), `SDL_Delay(20)` at :872.
  - `run_import_screen` loop (`import_ui.cpp:40`), `SDL_Delay(16)` at :127, worker `std::thread` at :70.
  - `viewer.cpp:174` also has `SDL_Delay(16)` (research viewer only, out of scope for web).
  - No `SDL_MAIN_USE_CALLBACKS` / `SDL_App*` symbols anywhere in `src/` (grep: zero hits). Task 3 refactor is as described.
- **Import path:** `importer.cpp:110-113` builds `Iso9660` over a synchronous `ReadAt` lambda (`Seek+Read`); `import_ui.cpp:70` runs `import_game` on a worker thread. Browser `File.slice().arrayBuffer()` is async, so the pull-based `ImportJob` (Task 4) is required. No `ImportJob`/`AssetStore`/`platform_web`/`app_main`/`app_state` files exist yet.
- **ISO contiguity:** `iso9660.cpp:39` rejects interleaved/multi-extent records; `record()` stores a single `(sector, size)` extent and `load()` reads it in one `read_at` call. Files are contiguous extents — chunked range requests (≤1 MiB) are sound.
- **Storage:** `assets.cpp:8-16` is synchronous whole-file `SDL_LoadFile`; saves/settings go through `SDL_GetPrefPath` (`importer.cpp:41`, `game_main.cpp:660`, settings/saves paths in `run_game`). Task 5 backend split confirmed needed.
- **Toolchain state:** `emcc` not found; system node is v25.8.1 (REA docs require 22/24 LTS — emsdk ships its own node, so use that for web tests). `.tools/` has `android-sdk` and a pinned `SDL3-3.4.8.tar.gz` but no `emsdk/`. `cmake/sdl3_static.cmake` pins SDL 3.4.8 `SHA256=e9fff746...cbfbba` via FetchContent — reusable for the Emscripten branch as the plan says.
- **Reference data:** `src/reference_hashes.inc` has 341 entries (matches plan's 341 files); `../cdrom` is 494 MB on disk (plan says ISO 518 MB / imported 467 MB — same order, close enough for planning).
- **`long`/`size_t` risk for Task 2:** `play_video(..., long ticks)` (`game_main.cpp:430`) and `long menu_frames`/`long frames` in `run_game` — `long` is 32-bit under wasm32 vs 64-bit on macOS ARM64. Fix by using `long long`/`int64_t` or explicit widths when Task 2/3 touches these signatures. `Iso9660::ReadAt` already uses `uint64_t` offsets (good).

## What was NOT done (needs owner approval per Global Constraints)

- No network access, no emsdk clone, no Playwright install, no `cmake/emscripten.cmake`, no `CMakeLists.txt` edits (owned by the tooling worker right now), no `docs/WEB.md` yet.
- Next action after approval: clone emsdk into `.tools/emsdk`, pin newest release ≥2 weeks old, record version+hash in `docs/TOOLING.md`, then `cmake/emscripten.cmake` + `rr-resource`-only web configure per Task 1 Step 2.

## File ownership claimed

- `cmake/emscripten.cmake` (new), `docs/WEB.md` (new), `build-web/` (new build dir). Will not touch `CMakeLists.txt` until the tooling worker's edits land; will coordinate on `src/game_main.cpp` before Task 3.

## Task 1 results (same session)

All five steps done; details in `docs/WEB.md`.

- **Step 1:** emsdk 6.0.10 installed in `.tools/emsdk` (owner-approved in-chat), recorded in `docs/TOOLING.md`.
- **Step 2:** `cmake/emscripten.cmake` created (pinned SDL 3.4.8 archive reused, `-O3 -flto -msimd128`, Node emulator); `CMakeLists.txt` gained an `if(EMSCRIPTEN)` branch (lib + tests only, game executables skipped until Task 3). Two real fixes: (1) `-sDISABLE_EXCEPTION_CATCHING=0` must be a *compile* option or no landing pads are emitted (every catch dead); (2) restored the `frontend` SKIP_RETURN_CODE line dropped by a transient concurrent edit.
- **Step 3:** `ctest --test-dir build-web`: 63 tests, 46 pass, 12 skip (no disc/goldens in sandbox), 5 fail — all 5 pass natively; causes diagnosed for Task 2 (`oracle-sim` needs a Node prefix in verify_sim.py + NODERAWFS; `race`/`sim_fields` are genuine wasm32-only aborts in synthetic sim code; `importer` needs pthreads for its FIFO test; `mixer-device` needs an audio-device skip).
- **Step 4:** `rr-bench-web` (`tests/bench_web.cpp`, behind `RR_BENCH_WEB`, ON for web): identical checksums native vs wasm; ratios ~1.2x render / ~0.6x cinepak — far under the 3x stop threshold. Sizes recorded in WEB.md.
- **Step 5:** `docs/WEB.md` written.

## Concurrent-work incidents (for the integrator)

- Mid-session, `CMakeLists.txt` lost my uncommitted web hunks to another session's transient rewrite (which briefly added a broken `car_contact` target + a half-written `tests/car_contact_test.cpp`, failing the web build). I re-applied only my hunks and rebuilt from a clean `build-web/`.
- The same incident deleted HEAD's `set_tests_properties(frontend ... SKIP_RETURN_CODE 77)` line; restored (the test still returns 77, so the deletion was accidental).
- The tree also carries the music worker's uncommitted `CMakeLists.txt` hunks (`src/race_music.cpp` source entry, `music` foreach entry + skip) plus untracked `src/race_music.cpp` / `tests/music_test.cpp`. My commit stages **only** the web hunks of `CMakeLists.txt` (hunk-selective); theirs stay in the working tree. Do not bless their hunks as part of this commit.
- `src/game_main.cpp`, `src/race.cpp/hpp` and several frontend/hud/settings files have other sessions' in-flight edits; the web build links the library from the current tree state, so Task 2's failure list should be re-checked after those land.
