# Race frame performance + benchmark tooling (2026-10-24)

- Baseline `289def9`. Shared-tree work; the other sessions' uncommitted sim
  edits (`race*.cpp`, `traffic*`, `police.cpp`, `sound*`, `CMakeLists.txt`,
  oracle proxy and tests) were left alone and not staged.
- Owned and changed: `run-macos.sh`, `src/frame_profile.hpp` (new),
  `src/game_main.cpp` (`--bench*` flags and report), `src/game_types.hpp`
  (`RaceOptions::bench`), `src/race_screen.cpp` (phase scopes; render_scale 0
  follows the settings), `src/race_caches.hpp`, `src/post_chain.{hpp,cpp}`,
  `src/raster_rgb.cpp`, `tests/post_chain_test.cpp`, `tools/bench_game.sh`,
  `tools/bench_report.py`, `docs/PERFORMANCE.md`, `README.md`, and this
  session's PROGRESS/VERIFICATION entries.
- Evidence: see the VERIFICATION entry "Race frame performance". Build dirs:
  `build-perf-cc`, `build-bench-debug`, `build-bench-release`, and the
  worktree `../native-port-perf` (removable).
- No live processes or held resources.
- Next: SIMD or integer trilinear and grade for scale 3+ and Android. Pass
  `draw_slices` to the race fog.
