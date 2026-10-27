# steal-original-fixture-20261026

Task: Measure original steal resolver on synthetic component fixtures in private worktree and Wine prefix

Baseline: 80f3e729fb317bdaeba20690c42f10371ef0d224

Worktree: `/Users/archit/Dev/rr/.re-steal-worktree-20261026`
Build directory: `/Users/archit/Dev/rr/.re-steal-worktree-20261026/build-agent-steal-original-fixture-20261026`
Owner PID: 95735
Resource token: `0153a596-58c1-4a1d-bb84-5113f433c5f1`

Owned files:

- `/Users/archit/Dev/rr/.re-steal-worktree-20261026/oracle/steal_fixture.c`
- `/Users/archit/Dev/rr/.re-steal-worktree-20261026/tools/steal_fixture_capture.py`
- `/Users/archit/Dev/rr/.re-steal-worktree-20261026/tests/steal_fixture_test.py`
- `/Users/archit/Dev/rr/.re-steal-worktree-20261026/docs/agent-handoffs/steal-original-fixture-20261026.md`
- `/Users/archit/Dev/rr/.re-steal-worktree-20261026/docs/REVERSE_ENGINEERING.md`
- `/Users/archit/Dev/rr/.re-steal-worktree-20261026/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/.re-steal-worktree-20261026/docs/PROGRESS.md`

## Progress / evidence

Completed the original resolver gate/transfer component measurement.

- Dedicated worktree as requested; branch `codex/re-steal-fixture`. App-managed
  creation could not resolve the parent workspace as a Git repository, so
  `tools/agent_workspace.py --worktree` created the checkout from the actual
  `native-port` repository. All commands/edits used this explicit worktree.
- A standalone proxy (`oracle/steal_fixture.c`) calls original `0x004096f0`
  in a process that exits before normal game startup. It does not build,
  modify or install the actively owned `oracle/aweman_proxy.c`.
- 31 original cases: 13 transfers, 15 gate rejections before the steal RNG
  call, three rejected controlled rolls. Measures signed victim frame,
  strict unsigned victim deadline, attacker deadline write, active-weapon
  gates versus inventory copy, hit-event gates, reach edges and queues.
- Original `0x00441750` mode 3 supplies known rolls. A verified forwarding
  call hook counts visits to `0x40981c`; no random filler calls or modified
  eligibility code. Normal damage is suppressed by victim hit immunity;
  neither synthetic bike is the player, and networking flags are off.
- `tools/steal_fixture_capture.py` rejects a foreign EXE before modifying
  any prefix, builds with warnings as errors, runs under resource claims
  and retains a unique trace/receipt. `tests/steal_fixture_test.py` verifies
  trace/source/DLL hashes against that receipt before checking outcomes.
- Private APFS-cloned prefix: `/Users/archit/Dev/rr/.re-steal-prefix-20261026`.
  The main prefix was only read while cloning; no original capture, source,
  DLL, Ghidra database, accepted trace or frontend work was overwritten.

Local evidence directory:
`analysis/oracle/steal-fixture/46883e0ec8734a20b8c9496c15184dc7/`.
Trace SHA-256: `f9cd89ec99f22187ae55c87162a74c471e712090006afeb17f061924a7355a01`.
EXE SHA-256: `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.
GCC MinGW i686 16.2.0; Wine 11.18 Staging. The binary, original EXE and
trace remain local/ignored, never committed.

Changed files: the three new fixture/runner/test files above, this handoff,
`docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`.
No native simulation or build configuration changed.

## Checks / remaining work

Commands from the worktree:

```sh
python3 tools/steal_fixture_capture.py \
  --exe /Users/archit/Dev/rr/cdrom/ROADRASH/ROADRASH.EXE \
  --prefix /Users/archit/Dev/rr/.re-steal-prefix-20261026 \
  --clone-prefix /Users/archit/Dev/rr/prefix
python3 tests/steal_fixture_test.py \
  --trace analysis/oracle/steal-fixture/46883e0ec8734a20b8c9496c15184dc7/steal.tsv
python3 tests/steal_capture_audit_test.py
git diff --check
```

All exit 0. Original fixture compilation uses `-O2 -Wall -Wextra -Werror
-shared -static-libgcc`; all 31 original expectations pass; four synthetic
format/hash-guard checks and the earlier audit's five checks pass. Verification
log is alongside the receipt. Reuse the private prefix by omitting
`--clone-prefix`; every run gets a unique immutable output directory.

Scope: original synthetic component fixtures. Not native replay, live
human controls, original animation assets, normal damage, CRT random-stream
advancement or a full-race steal. Plan 10 Task 6 / plan 16 Task 3 runtime
steal parity is still open. No native/Android/sanitizer/device gate was run
because this slice changes only standalone research tooling/documentation.

Next distinct acceptance step: after ownership coordination, an original
field capture with complete opponent animation fields and a resolver-entry
probe, followed by single-anchor native replay of a recorded transfer.
Do not infer this coverage from the old zero-steal farm traces.

## Live processes / resources

No live Wine, game, capture, Ghidra or build processes. Only the dedicated
prefix remains on disk for reuse. File claims released at completion.
Private worktree retained as requested; no shared checkout edits.

## Final integration and committed-source repeat

- Initial fixture commit: `876a584`. The commit hook formatted the C source,
  so the original-function capture was rebuilt/repeated from committed source
  instead of reusing a stale source-hash receipt. Final receipt is the run
  `46883e0ec8734a20b8c9496c15184dc7` above; all 31 rows are byte-identical
  to the initial capture (same trace SHA-256), and source/DLL provenance checks
  pass against the committed C source.
- Concurrent multiplayer commits `796f251` and `7ecda13` merged without
  conflicts; their simulation/server/source changes were preserved.
  No unrelated source was edited or restaged.
