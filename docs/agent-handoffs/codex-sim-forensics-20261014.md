# codex-sim-forensics-20261014

Task: Plan10 forensic SIM capture fault and frozen-grid evidence

Baseline: 02d7e8ddcfde8963a86291be6961961c64455b72

Worktree: `/Users/archit/Dev/rr/.codex-sim-20261014/native-port`
Build directory: `/Users/archit/Dev/rr/.codex-sim-20261014/native-port/build-agent-codex-sim-forensics-20261014`
Owner PID: 62801
Resource token: `766bd85d-2ac0-4c06-9ddb-8cb51c63e7fd`

Owned files:

- `/Users/archit/Dev/rr/.codex-sim-20261014/native-port/oracle/aweman_proxy.c`
- `/Users/archit/Dev/rr/.codex-sim-20261014/native-port/oracle/build.sh`
- `/Users/archit/Dev/rr/.codex-sim-20261014/native-port/docs/agent-handoffs/codex-sim-forensics-20261014.md`

## Progress / evidence

Restored billboard forwarding, added512 record bounds, and reset actor
buffers at every present. Native actual-hook tests catch missing forwarding,
cap and reset by mutation. SIM bounds count tick dumps. Frozen setup fields
with strict linked-half validation are added; replay helper leaves legacy
goldens unchanged. All production code was built from clean02d7e8d plus these
changes, excluding the backed-up OMP diagnostics.

Native gate97/97, zero skips, viewer/course sweep pass; MinGW proxy build
passes; actual C hooks and C++ frozen helper pass strict compilation and
ASan/UBSan.900-tick and300-tick captures exit0 and validate. First250-present
probe exited0 with70ticks and was correctly rejected before fixing the limit.

Fresh data resolves tick210 cause: original frozen dt4 moves candidate3840
per contact, native dt1 only960. Metadata +0c/+10/+14/+18 must be captured
and scheduled even with NULL callbacks. Read frozen-scheduler-trace-20261014
for exact control flow; no hardcoded dt4 or per-tick reanchoring. Current
replay remains divergent, expected manifest unchanged. True human-control
entry/count evidence and actor pixel acceptance remain open.

Original installed DLL and dirty oracle diff/logs backed up in local
analysis/sim-forensics. Final installed DLL will be restored from the clean
fixed production build after the temporary mask-probe capture. No processes
remain from completed Wine calls. Integration and claims release belong to
main. Stable private checkout retained as evidence.

## Checks / remaining work

Next command after integration: extend first-frame scheduler probe with
+0c/+10/+14/+18, capture once, implement exact frozen metadata cadence in
countdown and race ticks, and replay codex-sim-mask300 to push the first
divergence past210. Do not mark Plan10 complete.

## Live processes / resources

File claims above. Record capture/Ghidra processes here.
