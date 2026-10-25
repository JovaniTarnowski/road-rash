# frontend-menu-26

Task: Plan 13 Task 6 Step 2: original menu capture, first slice (main menu screen 9)

Baseline: 1eab47e192a4aaa472631dac07f418231ed1441c

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-frontend-menu-26`
Owner PID: 20296
Resource token: `eb05df62-c4fe-4f32-87ac-c078899f9ead`

Owned files (extended during the slice; no overlap with the crash worker's
`src/race.cpp`, `tests/race_test.cpp`, `tests/crash_test.cpp`, `src/traffic.cpp`,
`tests/field_replay.hpp`):

- `/Users/archit/Dev/rr/native-port/oracle/aweman_proxy.c`
- `/Users/archit/Dev/rr/native-port/oracle/build.sh` (one-word `-lgdi32` link fix)
- `/Users/archit/Dev/rr/native-port/tests/frontend_test.cpp`
- `/Users/archit/Dev/rr/native-port/src/rri.cpp`, `src/rri.hpp`, `src/frontend.cpp`

## Progress / evidence

`RR_ORACLE_MENU=9` watchdog captures the composed 640×480 game window once
`0x0046a660` holds screen 9 (`analysis/oracle/menu-09-20261026.bmp` + receipt;
cnc-ddraw gdi renderer at 640×480, ini restored afterward). Boot takes ~100 s
under Wine (intro video), so the watchdog allows 300 s.

RGB565 finding: the live `0x00485df8` table (built by `0x0041e090` from the
display masks; dumped post-settle to `menu-table.bin`) maps all 32768
555-grid inputs to RGB565 with no odd 6-bit greens. The native menu pipeline
used RGB555. Port fix in this slice: `quantise565`/`darken565` in `rri.*`
(masked `0x7bde` darken mirrors `0x0041e6e0`), `frontend.cpp` call sites,
`frontend_test` pins updated. Native re-render + measured comparison pending
on the owned Release build.

## Checks / remaining work

- Owned Release build (`build-agent-frontend-menu-26`, `rr-game` +
  `frontend-test`) running in background.
- Then: re-render screen 9, measure non-text diff, add golden comparison test
  (`--save-corpus` pattern, skip 77), docs, commit, push.
- Other agent's dirty files (`src/race.cpp`, `src/traffic.cpp`,
  `tests/field_replay.hpp`) untouched; `ddraw.ini` restored (hash-verified).

## Live processes / resources

- Owner PID 20296 (sleep holder). Wine runs held `prefix` + oracle install
  claims via `agent_resource` and were killed after each capture.
- `ddraw.ini` in the oracle install was temporarily set to 640×480 for the
  menu captures and restored byte-identical (hash-verified) each run.
- Owned Release gate (`build-agent-frontend-menu-26`, full build + full
  ctest): **106/106 passed**, zero failures. Gate log
  `/tmp/menu26-fullctest.log`.
- Remaining: record gate outcome; commit explicit paths
  (`oracle/aweman_proxy.c`, `oracle/build.sh`, `src/rri.*`, `src/frontend.cpp`,
  `tests/frontend_test.cpp`, docs, handoff); push. `src/career.cpp` untouched
  this slice. Other agent's dirty files (`src/race.cpp`, `src/traffic.cpp`,
  `tests/field_replay.hpp`) untouched.
