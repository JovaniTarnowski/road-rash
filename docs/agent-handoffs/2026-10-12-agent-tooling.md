# Agent tooling implementation

Task: implement review P0/P1. Baseline: e3136a3; dirty simulation/oracle source
and shared progress/plans belong to other sessions and are preserved.

Owned files: new agent tooling scripts/tests/docs, root/native instructions,
Makefile additions, CMake test/build-info integration, CI/packaging/Ghidra script
wrappers. `game_main.cpp` receives only an isolated build-info option; existing
hunks remain intact. No edits to race code, oracle proxy or replay implementation.
Build directory: `build-agent-tooling`. No shared Wine/Ghidra captures scheduled.

Progress: installed canonical AGENTS.md and Claude imports; implementing resource
claims, gate receipts, strict simulation validation, capture receipts, doctor,
status/evidence and build provenance.

## Completed P0/P1 implementation

- Installed `native-port/AGENTS.md` from the review proposal; updated its diagnostic
  rule to match the implemented gate. Added native Claude import and root routing/imports.
- Added explicit file/directory claims, detached-worktree support, per-session
  handoffs and shared Wine/Ghidra/build resource guards with explicit PID-checked reaping.
- Replaced blanket CTest WILL_FAIL with exact fixture/output-hash diagnostics;
  strict acceptance rejects missing provenance, demo/unverified controls and missing replay capability.
- Added complete-trace validation, capture deadlines, owned-process cleanup,
  immutable capture runs, receipts and failure preservation. Oracle install/build
  and Ghidra wrappers use the same resource registry and preserve exit codes.
- Added read-only doctor, source/config/compiler build identity (`rr-game --build-info`),
  package identity checks, logged native/sanitizer/oracle/Android/package profiles,
  evidence/status manifest + renderer, address/decompile/evidence index, Makefile
  commands and CI profiles. Existing native asset smoke/course sweep retained.
- Documented commands and limitations in docs/AGENT_TOOLING.md; README routes to
  docs/CURRENT_STATUS.md instead of treating old snapshots as current acceptance.

## Verification

- 15 synthetic tooling regression tests pass. Python compilation, shell syntax,
  local documentation links, Claude pointer checks and `git diff --check` pass.
- Native final gate: 63/63 CTest checks, viewer asset smoke and five-course sweep pass.
- Package final gate: 63/63 Release tests and ARM64/system-library/license/asset/
  ad-hoc-signature/embedded-source-config checks pass.
- Final focused ASan/UBSan gate: 6/6 selected CTest checks pass, including the
  tooling suite, replay diagnostics, race/contact/crash/police. An earlier full
  sanitizer run passed all 63 tests, but its receipt was invalidated by concurrent
  source edits; it is retained as historical evidence, not a stable final gate.
- Android final debug and development-release assemble and APK asset inspections
  pass. Release uses explicit local debug signing. No hardware tests performed.
- Known diagnostic is currently the pinned tick-139 RNG mismatch (another session
  closed the older tick-120 physics gap and updated the baseline with its evidence).
  No golden was altered by this task. Diagnostic acceptance is false by design.
- Strict acceptance rejects the existing human-named fixture: missing capture
  receipt and no instrumented player-control proof. Physics/replay completion is
  outside this tooling task; the gate exposes that gap instead of hiding it.

Final receipts: `analysis/agent-gates/p0-p1-*-final/receipt.json` (local/ignored).
The evidence matrix records their exact commit/source identity and coverage.
After validation, another task added web-port files (`cmake/emscripten.cmake` and
`tests/golden.proto`); those changes were preserved and invalidate earlier build
identity against the new working tree. A current package needs rebuilding after
that task's source/config changes settle. The stale-app check was observed failing
correctly against this later tree.

## Integration and limitations

Another session committed shared work during this implementation (including an
initial tooling checkpoint). This task did not stage, commit, reset or rewrite
other sessions' changes. The later config-fingerprint/receipt improvements remain
as local changes; inspect explicit paths before any future commit.

Claims are cooperative; unmanaged agents/commands can bypass them. Actual
instruction loading inside all four clients was not invoked; files/imports were
checked and the current Codex session read the canonical instructions. No live
Wine capture/Ghidra import/device test was run; capture failure contracts were
validated with synthetic processes. P2 exporters and C++ toolchain hardening
remain separate work. No task-owned live processes or held resources remain
after the final gate completes.

Final integration note: after the passing gates, another session added
`car_contact` to the shared CMake test list before creating
`tests/car_contact_test.cpp`. The attempted strict profile stopped correctly at
CMake generation with that missing-source error; it did not reach replay.
The direct strict verifier's rejection of the old fixture is retained at
`analysis/agent-tools/strict-acceptance.json`. The incomplete concurrent test
registration was left intact for its owner. Passing final receipts describe the
earlier stable 63-test source snapshot, not this later in-flight tree.
