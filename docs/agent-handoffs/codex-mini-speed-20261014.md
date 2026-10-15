# codex-mini-speed-20261014

Task: Plan11 Task7 / Plan18 Task4 mini dashboard numeric speed correction

Baseline: 4c14085f67b15fdf63fdadd7df97708ab78695b7

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-codex-mini-speed`
Owner PID: 48801
Resource token: `e200cb23-93aa-4aee-b1b6-962d94d0ba39`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/hud.cpp`
- `/Users/archit/Dev/rr/native-port/src/hud.hpp`
- `/Users/archit/Dev/rr/native-port/tests/hud_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/codex-mini-speed-20261014.md`

## Progress / evidence

Implemented mini dashboard numeric speed, replacing travelled distance in the
mini readout. `HudState::speed` is appended after existing fields to preserve
aggregate initializer positions; `Hud::state` computes it from bike `+0xec`
and the existing metric selection. No game-main integration changes required.

Evidence: `analysis/decompiled/004468e0.c:20-33`, checked against
`analysis/original-disassembly.txt:96893-96925` (`0x0044693a..0x004469a4`).
Metric masks before signed shifts: `0xfffff0ff >> 8`, `0xffffff87 >> 3`,
`0xfffffe1f >> 5`, sum divided by 10 with truncation toward zero; imperial
uses signed `>>12 + >>7 + >>9`. Negative result clamps to zero. The largest
signed input produces sums within int32 range. `00446c30.c:177-188` draws
that numeric value at the mini speed rectangle. REA spot-check belongs to
the main integrator; no REA, Wine or capture run by this worker.

Tests add fixed expected conversions for zero, 127/128, 4095/4096, 8192,
12345, reverse, and int32 limits before the disc-availability check. Disc
integration verifies both unit settings read bike velocity and clamp reverse;
composition verifies mini changes with speed, ignores distance, and full
dashboard ignores the new numeric field across all three artwork families.

## Checks / remaining work

- Initial build failed as expected for missing helper/state field:
  `/tmp/rr-codex-mini-speed-red.log` (exit 2).
- After helper/state scaffolding and before changing mini composition,
  focused CTest failed with `Mini speed readout changes with distance`
  (exit 8): `/tmp/rr-codex-mini-speed-behavior-red-test.log`.
- `cmake -S . -B build-agent-codex-mini-speed -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` passed.
- `cmake --build build-agent-codex-mini-speed --target hud-test --parallel 4`
  passed: `/tmp/rr-codex-mini-speed-green-build.log`.
- `ctest --test-dir build-agent-codex-mini-speed -R '^hud$'
  --output-on-failure` passed, 1/1, 0.47 s with disc available:
  `/tmp/rr-codex-mini-speed-green-test.log`.
- Private compilation database clangd checks for both `src/hud.cpp` and
  `tests/hud_test.cpp` passed with 0 errors; JSON logs at
  `/tmp/rr-codex-mini-speed-lsp-{hud,test}.json`.
- `git diff --check` passed.

Full integration gate, REA check, shared progress/verification docs and commit
remain with main integrator. This does not close Plan16 Task6 pen/list pixel
acceptance. Existing mini layout/text rasterization and debug `%d:%d` branch
are outside this narrow correction.

## Live processes / resources

No Wine/Ghidra/capture resources. Claim owner was PID 48801; file claims released
at handoff. No commit created. Other agents' CMake/race/oracle edits preserved.

## Integration recovery

The original 2b08e6f commit was accidentally reset by another client, as
recorded in incident-interleaved-main-20261014.md. Source/test bytes remain
identical to that validated commit. Re-recorded the same correction after
reviewing the incident and the later shared-tree 95/95 gate receipt; no
reset, rebase or overwrite of concurrent source was used.
