# crash-flash-nitro-20261025

Task: Port the remaining `0x00407a90` state-3 flash copy and the player-only
`0x0040b1a0` nitro call from disassembly (plan 11 Task 4 / plan 16 crash slice).

Baseline: `ec7f0c1`.

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build-agent-crash-flash` (owned, Debug, retained)

Owned files:

- `src/race.cpp` (`crash_bike_animation` flash for the every-tick loop only;
  `step_rider` nitro gated to the player, including on the crash branch)
- `tests/race_test.cpp` (AI nitro never ticks)
- `tests/crash_test.cpp` (pickup flash mirror/expiry/sample-mismatch;
  crashed-player nitro)
- `docs/PROGRESS.md`, `docs/VERIFICATION.md`, `docs/REVERSE_ENGINEERING.md`

## Progress / evidence

Disassembly (not REA: `0x00407a90` has no REA procedure entry, known gap #8):
`407a91-407aa7` player check (`DAT_004642d8`), `407abc-407b0b` +0x7c clear
and flash words (bike +0x1c>>8 vs half +0x1ec, unsigned bike +0x10 minus
half +0x1f0 < 12, `(gap>>2)+1`, bike +0x81 copy), `437e69-437e6f` (+0x10
stored before think), `40b1a0-40b1fa` (global `DAT_004c5f10` charge,
dt = bike +0x18).

Live parity preserved by construction: the scheduler think keeps its
accepted sample form; the helper flash runs only when `!live_schedule_`
(verified: exact bike-word think diverged field L1 at tick 1301
`rider.3.raw_1e8` 896 vs 128, restored before commit).

Checks (owned build):
- `cmake --build build-agent-crash-flash --parallel 8`: exit 0.
- `race-test`, `crash-test`: pass (new pins fail before / pass after by
  construction).
- `ctest --test-dir build-agent-crash-flash --parallel 8`: 105/105 pass.
- `make verify-sim BUILD_DIR=build-agent-crash-flash`: exit 0 (strict human
  1431 identical; field replays green).

## Remaining

- Live think exact-word form (route sample vs bike +0x1c>>8): kept the
  accepted form; needs a trace for why the sample form matches the golden
  while the bike word diverges at tick 1301.
- Uncrashed every-tick path has no flash copy (game uses the scheduler).
- No Android/sanitizer/oracle-capture run in this slice.

## Live processes / resources

No game/encoder/test processes remain. Owned build `build-agent-crash-flash`
retained. No Wine/Ghidra mutation (read-only disassembly + existing export).
