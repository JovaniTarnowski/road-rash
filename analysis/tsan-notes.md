# ThreadSanitizer tree — build, runs and findings (plan 22 Task 4)

Date: 2026-10-12. Toolchain: Homebrew clang 22.1.4, Darwin ARM64
(TSan supported per Clang docs). Tree: `build-tsan`
(`-DRR_TSAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo`,
Homebrew `clang`/`clang++`, `-fsanitize=thread -fno-omit-frame-pointer`).

Usage:

- `sh tools/tsan.sh` — configure, build, run race-relevant suites, run scenarios.
- `RR_TSAN=1 tools/ci.sh` — CI gate entry (opt-in; rebuilds everything).
- `RR_TSAN_BUILD_DIR`, `RR_TSAN_TESTS`, `RR_TSAN_CTEST_ARGS`, `RR_JOBS` override defaults.

## Runs (this session)

- `ctest -R 'mixer|importer|wav|avi|cinepak|video_player|session|actor_traffic|traffic'`
  in `build-tsan` with `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1`:
- `rr-game --first-run-smoke` (import worker thread): exit 0, no TSan report.
- Headless hunt race `--race --course 3 --steer-into-traffic --ticks 1800
  --assets ../cdrom/ROADRASH` with dummy video+audio (sim+traffic tick path):
  exit 0, 1800 ticks, no TSan report. (Plain `--autopilot --ticks` without
  `--steer-into-traffic` falls through to the interactive loop and never
  returns — pre-existing CLI behaviour, not a TSan finding.)
+

## Findings

- None so far in owned suites/scenarios. No suppressions added
  (`tools/tsan.supp` not created — none needed; per research note,
  suppressions require demonstrated external false positives).
- No source files touched (game/render/sim sources out of scope for this slice).

## Coverage gaps / open questions

- The dummy audio driver exercises the mixer callback path but not real
  hardware timing; a device/paused-background cycle (`--background-at-tick`,
  pause/resume every 100 ticks) is specified in plan 22 but the
  `--quit-after-import-ms` / `--background-at-tick` debug flags do not exist
  in `src/game_main.cpp` — adding them touches a file owned by the live
  playtest-visual-fixes session, so deferred with this note instead.
- `hires-render` with 7 render threads (plan 19 Task 6) does not exist yet;
  TSan must re-run once the threaded rasteriser lands (plan 22 → before plan 19).
- Full `ctest` under TSan was not run — only the race-relevant subset above.
  A full-tree TSan run is recommended before closing plan 22.
