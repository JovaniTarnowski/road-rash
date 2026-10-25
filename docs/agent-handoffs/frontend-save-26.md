# frontend-save-26

Task: Plan 13 Task 6: original save round trip and menu pixel comparison

Baseline: fe33fd9398fd0e63f280561f1ff464ce2b966ec6

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-frontend-save-26`
Owner PID: 17276
Resource token: `cc130d91-5630-4390-8d44-ea018261c60d`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/career.cpp`
- `/Users/archit/Dev/rr/native-port/tests/career_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/frontend_test.cpp`
- `/Users/archit/Dev/rr/native-port/oracle/aweman_proxy.c`

## Progress / evidence

Scope settled (read-only recon): the README line was partly stale. Per plan
checkboxes + code inspection (`src/frontend.cpp`):

- Jukebox (13 Task 4), attract demo (13 Task 5), sound mix (18 Task 6),
  Configure Input + gamepad/touch pages (18 Task 7), pause menu (18 Task 8):
  all ported, no "not ported" stubs remain.
- `case 0x00430140: return 0; // mano-a-mano: plan 09` is the one deliberate
  stub — stays disabled until the plan 09 multiplayer decision. Not this task.
- Genuinely open: save round trip + menu pixel comparison (13 Task 6) and
  low/inset race views + pixel goldens (18 Task 4).

This slice: Task 6 Step 1 (original save file). Save format itself is fully
traced and REA double-confirmed (`0x0042efc0`/`0x0042f000`/`0x0042f110`,
docs/REVERSE_ENGINEERING.md "Save file"); what is missing is a real
original-writer file to pin `career.cpp` against.

Approach: isolated in-process writer call (no menu driving — `RR_ORACLE_KEYS`
only posts race-frame keys, so full menu driving would be new capability).
New `RR_ORACLE_SAVE=1` proxy mode, mirroring the raster mode-2 attach-time
pattern: seed `0x004b8a10` + personality triples (`0x004c6b40`, rider index 2
in `0x00467a6c`), point `DAT_004b8900` at `C:\save-seed.RRS`, call
`0x0042f000`, hex-dump `DAT_00497090` (0xa0) to the TSV. Reader call
`0x0042f110` deliberately NOT made in-proxy: it runs `0x00444fb0` (UI
strings), unsafe before a window exists; the native reader is pinned by the
byte-identity test instead. Game continues booting after capture; the runner
kills it once the .RRS appears (deliverable is the file, not the TSV, so the
tick-capture validator does not apply — recorded here, not as a skip).

Owned-file check: other agent owns `src/race.cpp`, `tests/race_test.cpp`,
`tests/crash_test.cpp` (handoff `crash-flash-nitro-20261025`) plus dirty
`tests/field_replay.hpp` (their SCRATCH instrumentation, not for commit).
No overlap with this slice. Workspace claim registry empty at start.

## Checks / remaining work

- `oracle/build.sh`: exit 0 (one pre-existing palette `-Wformat-truncation`
  warning, untouched code). `oracle/install.sh`: proxy
  `ec1ac9e564e0b3195cd058e866ffc7ec798cca9d516ce377472a83289cf35a3d`.
- Direct Wine run under `agent_resource` claim (prefix + install), env
  `RR_ORACLE_SAVE=1`, killed only its own process group after
  `C:\save-seed.RRS` hit 0xa0 bytes: `target_ok=True`.
- Golden `analysis/oracle/save-seed-20261026.rrs` (sha256 `45c601fd…3619c388`)
  + `.receipt.json`; TSV hex dump identical to the file. Seeded values read
  back exactly (header, cash `0x12345678`, `SAVETEST`, 30 triples incl.
  negatives and `0x7fffffff`).
- `career-test` (owned Debug build): synthetic 0 failures;
  `--save-corpus` 0 failures (all field pins + `write_save` byte identity).
- Full owned-Debug gate (`build-agent-frontend-save-26`): **106/106 passed,
  zero failures** (105 existing + new `career-save-corpus`), 194 s real.
  `make verify-sim` / sanitizers / Android not run in this slice (no sim,
  sanitizer-sensitive, or Android-affecting change: proxy is a separate
  mingw build, CMake change is one additive test).
- Remaining: record gate outcome here; commit explicit paths
  (`oracle/aweman_proxy.c`, `tests/career_test.cpp`, `CMakeLists.txt`,
  `docs/VERIFICATION.md`, `docs/REVERSE_ENGINEERING.md`, `docs/PROGRESS.md`,
  `README.md`, `docs/USER_GUIDE.md`, handoff); push. `src/career.cpp` needed
  no change. Step 2 (menu pixels) and the driven-career save variant stay open.

## Live processes / resources

File claims above. Record capture/Ghidra processes here.
