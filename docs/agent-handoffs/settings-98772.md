# Settings display (settings-98772) — plan 18 verification + garbage-file hardening

Task: Settings display (plan 18, simulation-independent slice).
Baseline: `97a4281` (sim: steer-kick tail port). Head at work time moved
concurrently (OMP sim/fuzz work); final commit `be6dc20` on top.

Worktree: `/Users/archit/Dev/rr/native-port` (shared tree, no worktree).
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-settings-98772`
(owned, Debug; kept for reuse).
Owner PID: 1. Resource token: `88ef44a8-6b5f-45d9-a493-ea67772343eb`.

Owned files (claim `settings-98772`):
- `src/settings.cpp`, `src/settings.hpp`, `tests/settings_test.cpp`

Changed + committed: `tests/settings_test.cpp` only (`be6dc20`,
"settings: garbage-file truncated-line test"). `src/settings.*` untouched
(no implementation gap found). Never touched `oracle/*`, `src/race.cpp`,
or any simulation file.

## Finding: plan 18 Tasks 2, 3, 5 are already landed

The plan file's open checkboxes (Task 2 Step 5, Task 5 Step 5) are stale —
their commits exist in history:

- Task 2 (settings model): `2416ee9` "settings: native display, scaling,
  vsync and touch fields". All enums/members/parse/format/clamp from the
  plan present in `src/settings.hpp/cpp`, plus later extensions (render
  scale, widescreen, enhancements, gamepad/touch pages).
- Task 3 (display module): `62abbe5` "display: window, fullscreen, scaling,
  filtering and vsync from settings". `display_config`/`apply_display`/
  `apply_texture_scale`, `--fullscreen`/`--window-scale`/`--toggle-fullscreen-at`
  CLI, Alt+Enter/Cmd+F hotkeys all present in `src/display.*`/`game_main.cpp`.
- Task 5 (native Display screen): `235b1b6` "frontend: native Display options
  screen in the original menu style". Counts since extended by plans 19–21
  (desktop 11 / mobile 10 rows); `tests/native_screens_test.cpp` asserts the
  current counts, geometry copy, nav chain, Back→0x13, and 0x13→Display link.
- Task 4 Step 1 (trace) is also done and documented in
  `docs/REVERSE_ENGINEERING.md` ("Race view modes and low-detail bikers",
  October 12, 2026).

Suggested integrator action: tick Task 2 Step 5 and Task 5 Step 5 in
`docs/plans/18-settings-and-options.md` (shared-doc edit left to integrator).

## Task 5 Review Focus 5 (gamepad-only) assessment

`tests/frontend_test.cpp` drives the Display flow (Restroom → Graphics
Options → Display → Scaling → Integer → Back, settings + file assertions)
through a `key()` helper that builds abstract `MenuInput` — the same struct
the SDL gamepad path produces, so the flow is device-independent by
construction. The pause screen (Task 8) has an additional explicit
d-pad+A/B-only pass (`pad()` helper). No gap found; no test change made
(frontend files outside this claim).

## Independent Task 4 Step 1 spot-check (read-only, no REA needed)

Re-derived from `analysis/original-disassembly.txt` + existing exports
(matches the committed RE trace; no contradictions):

- `0x00448870`: `bVar4 = 1 - HiRes`; render `w = 0x280 >> bVar4`
  (640/320), `h = 0x1e0 >> bVar4` (480/240). Inset (Inset=1, HiRes=0):
  display 640×480 with offsets `0x475394/98 = 160/120`; otherwise offsets 0.
- `0x004457f0`: choices 1/2/3 → (HiRes,Inset) = (0,0)/(0,1)/(1,0). Confirmed.
- `0x00416310`: HiRes off → `Images/Horizons/L` + `LilMeter` +
  `kLoDash.BOB`/`mLoDash.BOB` (metric-dependent); on → `Images/Horizons/` +
  `BigMeter` + `kHiDash.BOB`/`mHiDash.BOB`. Separate low artwork, not scaled.
- `0x004436a0` region: HiResBikers (`0x00475384`) nonzero → strings
  `0x46f080` (Hi) else `0x46f070` (Lo) + `0x46f090` (`.DAT`), via `0x443380`
  with fallback to Lo. Separate low sprites, not halved.
- `0x0041e710`: race framebuffer allocates render-size (`0x47539c/a0` =
  320×240) in low-fullscreen, full 640×480 when HiRes or Inset set.
- One unverified carry-over (not checked this session): the RE note "disasm
  `0x448c8a` forces HiResBikers = 1 after the bikers popup". The popup-apply
  write I saw is at `0x445905` (`movb %cl, 0x475384`, stores the choice, not
  a force-to-1). Leave as-is; needs a targeted look before porting Task 4.

## Change made (this session)

`tests/settings_test.cpp` (+6): Review Focus 2 garbage-file hardening —
unterminated last line (`haptics=1`, no trailing newline) parses;
truncated last line (`vsync`, no `=`) is ignored while good keys apply.
`parse_settings` (`getline`-based) already handled both; the test pins it.

## Checks (exit codes preserved, logs kept)

- Configure + full build, owned dir: exit 0
  (`/tmp/cfg-settings.log`, `/tmp/build-settings.log`).
- Focused `ctest -R 'settings|frontend|display|native'`: 4/4 PASS
  (`/tmp/focused-settings.log`; frontend 20 s, disc-backed).
- `settings-test` after edit: PASS (`/tmp/rebuild-settings.log`).
- Full gate `python3 tools/agent_gate.py native --build-dir
  build-agent-settings-98772 --session settings-98772` (`/tmp/gate-settings.log`,
  receipt `analysis/agent-gates/native-82d36e2fc87d45d581ea288e9745b051/`):
  all 5 native steps exit 0; ctest 94 passed / 0 failed / 0 skipped.
  Overall receipt status "failed" SOLELY from `simulation_diagnostic`
  `acceptance_met: false` — the known plan-10 tick-208 divergence
  (`rider.0.position` 13746 vs 13778), documented at baseline
  (`17857d7`, `f9b597f`, 00-INDEX). Pre-existing, out of scope,
  untouched by this change (settings-only diff).

## Remaining work (for integrator / next settings worker)

1. Plan 18 Task 4 Steps 2–5 (low/inset views, low dash, LOBOB select):
   blocked on oracle captures (October 12 outage note in REVERSE_ENGINEERING.md;
   Oct-14 sim-capture fault handoffs suggest the oracle is still unstable).
   Do NOT run captures without coordinating the prefix with the oracle owner;
   Step 3 tests need the `napa-drive-low`/`napa-drive-inset` goldens first.
   Port files (`game_main.cpp`, `hud.*`, rider sprites) are shared — claim
   before editing.
2. Stale checkboxes Task 2 Step 5 / Task 5 Step 5 (see above).
3. Concurrent-work note: during this session the shared tree gained
   in-flight edits outside this claim (`.gitignore`, `CMakeLists.txt`,
   `oracle/aweman_proxy.c`, `oracle/build.sh`, `src/race.cpp`,
   `tests/race_test.cpp`, untracked `fuzz/`, `cmake/llvm.cmake`,
   `docs/agent-handoffs/fuzz-infra.md`, `sim-fault-2.md`,
   `sim-fault-opencode.md`, `tests/dash_bench.cpp`). Preserved untouched.
   The gate ran against that tree; native steps still green.

## Live processes / resources

None (no servers, captures, or Ghidra sessions started). Owned build dir
`build-agent-settings-98772` retained. Claim `settings-98772` may be
released by the integrator after merging this handoff.
