# Full native gate 20261016h (HEAD 12f45ec)

Baseline: 12f45ec "docs: siren/engine slots, save round-trip, intersection opens handoffs 20261016g".
`git log --oneline -6`: 12f45ec / 242f718 / 0141e7f / 1b79351 / 8851ef6 / aae00a1.
Since gate2 baseline 7992dfe, landed: 1b79351 "hud: resolution pen width, needle order, solo target gap normalization" and 0141e7f "course: original start/finish bounds lookup with 25-race coverage".
`git status --short` at start and end: 0 tracked modifications; only pre-existing untracked analysis/fuzz/tests files. HEAD unchanged.

Owned build dir only: `build-agent-fullgate-20261016h` (gitignored via `build*/`). No shared `build/`, `build-clangd/`, other `build-agent-*/` touched.
No edits, stages, commits, pushes. Verification-only.

## Configure / build
- `cmake -S . -B build-agent-fullgate-20261016h -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DRR_STRICT_WARNINGS=ON` → exit 0 (log /tmp/fullgate-20261016h-configure.log).
- `cmake --build build-agent-fullgate-20261016h --parallel` → exit 0 (log /tmp/fullgate-20261016h-build.log). No error lines.

## ctest
- Full `ctest --test-dir build-agent-fullgate-20261016h --output-on-failure -j4` (log /tmp/fullgate-20261016h-ctest.log) killed by 120 s tool timeout at 97/99 finished: 96 passed, 1 failed. Unfinished (started, no result): #10 hires-render, #95 race-soak.
- First failure: #13 race-cache → `***Failed 2.56 sec`, output `RGB canvas output changed from pre-optimization baseline` (tests/race_cache_test.cpp:64, `rgb_baseline` captured from fdcd4aa, no new golden blessed). Rows logged before throw: row=0..5 hashes 16908656509277595288 / 2800544501961077726 / 10442994832978502141 / 12480883420206857557 / 6403728731288650074 / 11345352425216510781. Row=1 rgb hash matches baseline entry 2800544501961077726; failure is on a later rgb row. No fix attempted (verification-only).
- Reruns (owned dir only):
  - `ctest -R '^hires-render$'` → exit 0, 1/1 passed (128.77 s, log /tmp/fullgate-20261016h-hires.log).
  - `ctest -R '^race-soak$'` → exit 0, 1/1 passed (145.33 s, log /tmp/fullgate-20261016h-racesoak.log).
- Total: 98/99 passed, 1 failed (race-cache). Excludes no tests; exit codes preserved, no `|| true`.
- `ctest -R 'hud|course'` confirmation → exit 0, 3/3 passed (log /tmp/fullgate-20261016h-hudcourse.log): #56 course 0.37 s, #57 course_scene 51.74 s, #58 hud 0.30 s. Matches full-sweep passes (#56 0.50 s, #58 0.47 s, #57 61.00 s).

## Replay (owned binary `build-agent-fullgate-20261016h/oracle-sim-test` only, `--driver=demo`)
- `analysis/oracle/sched600-20261015.tsv --driver=demo` → exit 0 (log /tmp/fullgate-20261016h-sched600.log): `600 ticks identical (270 golden-only diagnostic fields not in acceptance set; 76822 field-comparisons uncovered ...)`. No divergence line. Start-hold `59 frozen frames, green at tick 60`, `58 native ticks, brain equal`.
- `analysis/oracle/sched900-20261015.tsv --driver=demo` → exit 1 (log /tmp/fullgate-20261016h-sched900.log): `first divergence at frame 892 (tick 893): rng original 707629878 native 2897718112`. Countdown/start-hold identical (`59 frozen frames, green at tick 60`, `58 native ticks, brain equal`). Same as gate2 baseline.

## LSP (read-only)
- `python3 tools/agent_lsp.py --check src/course.hpp` → script exit 1. clangd `--check` itself exit 0, `All checks completed, 0 errors`. Script reports `healthy: false` only because `newer_config: [CMakeLists.txt]` (owned gate dir is not the `build-clangd` database source); `entries: 181`, `missing_sources: []`. No config touched (log /tmp/fullgate-20261016h-lsp.log).

## No-edit confirmation
- No tracked source/docs/CMake/proxy/test files edited (`git status` tracked-dirty 0). Only file written: this handoff `docs/agent-handoffs/fullgate-20261016h.md`. Owned `build-agent-fullgate-20261016h/` is gitignored. No commits, no pushes.
