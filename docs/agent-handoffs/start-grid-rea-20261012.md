# start-grid-rea-20261012

Task: Reverse engineer original start-grid draw order using pinned REA and saved original scene captures

Baseline: a758e0ff6503136eba8a0c5a57dd8bc58a3eaf3b

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-start-grid-rea-20261012`
Owner PID: 28849
Resource token: `fe8e0b8f-185b-46db-b5e4-850b7d2fb6f3`

Owned files:

- `/Users/archit/Dev/rr/native-port/docs/research/2026-10-12-start-grid-order.md`
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md`
- `/Users/archit/Dev/rr/native-port/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/native-port/docs/PROGRESS.md`

## Progress / evidence

Completed the requested original draw-order research with actual pinned REA calls.
All three spot-checks (`0x00443af0`, `0x00438610`, `0x00443cd0`) succeeded
through Ghidra 12.1.4 on the hash-verified disc executable and matched existing
exports exactly. Checked assembly, visible-array/snapshot fields and two saved
original start-frame tails. Evidence is local under `analysis/oracle/rea-start-grid-*`
and `analysis/oracle/start-grid-order-audit.json`.

Changed the four claimed documentation files and this handoff only. Existing
dirty native source, tooling and other research edits were preserved. No source
implementation, build, oracle proxy change, capture or commit was performed.

Finding: the whole nearer world strip precedes the farther object pass, for both
actor halves. The unsigned low-byte `<0x80` split inserts scenery between actors.
The current native front-half-only delay before nearer terrain is still approximate.

## Checks / remaining work

- Reference SHA-256 agrees with inventory: PASS.
- Three REA commands: exit 0; complete function ranges and exact export matches.
- Python scene/capture integrity and near-road -> shadow -> body audit: PASS.
- Documentation `git diff --check`, whitespace, links, local evidence paths and
  summary references: PASS.
- No native tests required for this documentation-only research change.
- Plan 11a Task 3 remains open. Next implementation: separate world/object passes
  using the documented W/O sequence, preserve scenery masks and original tie order,
  capture full actor state, then compare constructed nodes/full frames.

## Live processes / resources

No live REA/Ghidra/Wine processes owned by this task. File claims released after
documentation verification; owner PID belongs to the parent Codex session.
