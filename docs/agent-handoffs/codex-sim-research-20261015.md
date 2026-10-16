# Next simulation divergence research — October 15, 2026

Read-only Plan10 Task5/6 research. Baseline fdcd4aa, including selected-player
rider identity fix15ebc38. Owned only this handoff and ignored
`analysis/codex-sim-next-20261015/`. No source, Wine, proxy, Ghidra-project,
build-directory or capture mutation. Other agents' source edits preserved.

## Current actual replay findings

Use `build/oracle-sim-test`, not the stale
`build-agent-countdown-timers-20261015/oracle-sim-test` (which still fails584).
Commands run from native-port:

```
build/oracle-sim-test analysis/oracle/sched600-20261015.tsv --driver=demo
build/oracle-sim-test analysis/oracle/sched900-20261015.tsv --driver=demo
build/oracle-sim-test analysis/oracle/terrain-human200-20261015.tsv
```

First exits0:600 ticks identical, with the explicitly reported uncovered
field eras/270 golden-only diagnostics; this is no full scenario acceptance.
Second exits1 at735, brain+194 original5632/native22784. Third exits1 at
frame0/tick1 `ring.walk.front.terrain.point.0.end` original132/native133.
The new200tick human fixture has a complete receipt, control_branch human,
exit0, trace SHA25694bb443a156b061c7e73d9a0d7a33a5e46022e90b3f373fa46087c96e4ae79d1.
Thus hidden race terrain state differs at setup; old tick189 is the first
coarser RNG mismatch, not the earliest hidden-state mismatch. Seeded terrain
component parity does not settle race setup call interleaving.

## Concrete next demo fix/probe: preserve null slice membership

Continuation (diagnostic only, never acceptance):

```
RR_SIM_CONTINUE=1 RR_SIM_TRACE=rider.0. build/oracle-sim-test analysis/oracle/sched900-20261015.tsv --driver=demo
```

Saved `analysis/codex-sim-next-20261015/demo-continue.log`. Tick734 still
matches target_x5632. Tick735 native newly latches object position27648,
original latch remains NULL. Native target position28608 vs original26215;
state1, target/ahead25088, behind23808 and other brain counters still match.
This identifies the overtake gate, before speed/steering arithmetic.

Existing original frame0 metadata is discriminating:

- nodeaddr.5: type8,position27648,lateral8192,**w2c=0**.
- nodeaddr.6: type4,position27648,lateral-9984,**w2c=0**,half2c=0.
- At native735 think entry player position25255, speed7335; difference2393
  has just entered the strict0x960=2400 scan window.

`src/race.cpp` build_scan_list manufactures a route sample for every frozen
bike and half, while original00449ce0 skips every candidate with object+2c
NULL, and returns NULL immediately if self+2c NULL. Original assembly
449ce1..449cee proves self gate,449d1f..449d24 proves forward candidate gate,
449d30..449d38 proves compatible slice+36 road-half flag gate. Native
scan_next currently checks distance/type only and its comment assumes every
object has a valid single-road slice. This is contradicted by captured nodes.
Native overtake target22784 is consistent with half lateral8192 + side14592.

Recommended narrow implementation: retain separate captured bike/half slice
validity when hydrating frozen state (w2c and half2c metadata), carry validity
into ScanObject, and apply original self/candidate null-slice gates while
preserving list traversal and distance stopping. Do not globally exclude
frozen halves: valid frozen halves were needed to resolve tick576. Preserve
legacy fixtures without this metadata explicitly. Add a focused synthetic
regression: null-slice bike/half inside scan window cannot become an overtake
target; valid-slice equivalent remains eligible. Then rerun same900tick
single-anchor trace; do not alter golden or allowlist. Full graph road-half
flag parity is a separate uncovered issue beyond the proven NULL case.

This is evidence-backed next work, not a claimed implemented fix. Sorting
of equal-position objects may also need later scrutiny; no guessed order
change justified by this probe alone.

## Reverse engineering and checks

Pinned `npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE
0x00449ce0 --format md` completed exit0 with documented Node24/JDK21/Ghidra
libexec environment. Evidence `analysis/codex-sim-next-20261015/rea-449ce0.md`
and `.log`; original hash73d7f974…a60f18a2. Independent ephemeral output
confirms candidate/self NULL gates, not runtime equivalence.

Two native LLDB read-only runs stopped at ai_scan_overtake when position25255;
initial full Rider/vector snapshot in scan-lldb.log. A second smaller attempt
failed on an optimized-out std::vector operator[] expression and ended;
no debugger state writes or source edits. Scripts/logs retained; do not call
the failed expression successful evidence.

No client C++ LSP channel exposed. clangd fallback requested through
`python3 tools/agent_lsp.py --check src/race.cpp`; result is recorded in
`analysis/codex-sim-next-20261015/lsp-race.json`. No compilation database changed.
No build needed for documentation-only worker. Parent owns implementation,
full build gate, status integration, commit/push and any original captures.

Final checks: REA00449ce0 normalized pseudocode is exactly identical to
existing address-keyed export (783 characters excluding whitespace).
clangd fallback exits3 with38 ExtractFunction tweak failures; preflight exits1
because CMakeLists.txt is newer than the compile DB (no missing sources). This is not a
clean LSP gate; source parsing has no reported syntax diagnostic. All owned
REA/LLDB/clangd processes ended; no shared resources held.

## Isolated implementation of tick735 correction

Parent subsequently authorized private implementation. Detached worktree
`/Users/archit/Dev/rr/.codex-sim735-20261015/native-port`, based fdcd4aa.
Source ownership is isolated there; live owner53167 still claims overlapping
main-checkout Race/replay source, so **do not integrate until coordinated**.
Only this uniquely named handoff was updated in main. No main source edits.

Changed5 private files: src/race.hpp,src/race.cpp,tests/frozen_state.hpp,
tests/frozen_state_test.cpp,tests/race_test.cpp. Original +0x2c slice validity
is now retained independently for hydrated frozen bike and linked half;
optional captured w2c/half2c pointers are range-checked, mutually checked when
both aliases exist, validated atomically, and absent diagnostics preserve
legacy valid-slice defaults. ScanObject carries nullptr sample for NULL
slice nodes. scan_next preserves all traversal/window stopping behavior,
rejects a NULL-slice self and skips NULL-slice candidates in either direction.
No original fixture or diagnostic allowlist changed; no RNG draw added.

New synthetic regression covers independently valid/invalid bike and half,
continues through a NULL-slice bike to its valid farther half, retains valid
selection and rejects NULL-slice observer. Frozen hydration regressions cover
legacy absence, linked alias fallback, all validity combinations, malformed
pointer ranges, conflicting aliases and validation atomicity.

Private configure/full Debug build exit0:

```
cmake -S . -B build-agent-sim735 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-agent-sim735 --parallel 4
ctest --test-dir build-agent-sim735 -R '^(race|frozen_state)$' --output-on-failure
```

Focused CTest2/2 pass. Logs `/tmp/codex-sim735-{configure,build,tests}.log`.
Private replay needs explicit absolute original assets/fixture paths:

```
build-agent-sim735/oracle-sim-test /Users/archit/Dev/rr/native-port/analysis/oracle/sched900-20261015.tsv /Users/archit/Dev/rr/cdrom/ROADRASH --driver=demo
```

Single-anchor900 now matches through892; first divergence893 is RNG original
707629878/native2897718112 (exactly2 additional native CRT draws). Fresh600
still600ticks identical with its reported coverage gaps. Diagnostic logs
`/tmp/codex-sim735-demo900.log`, `-demo600.log`, `-continuation.log`.

Next893 is likely a separate candidate contact-flag omission: continuation
shows original speed8450/native6866, lateral18451/native13895, a steering
kick and unchanged brain state. Captured node4 bike pos30208,lat28672,
**bike_c0=4**,w2c=0 lies within contact distance when playerpos30093. Original
004368f0 accepts candidates only when byte+c0 &5==5; native collide_riders
comment describes this gate but its code currently filters only object type.
This strongly motivates the next narrow contact gate probe; no change to
that separate code path was made in this slice.

Private clangd fallback with owned build compilation database: preflight
healthy181 entries; exit3 from38 preexisting ExtractFunction tweak diagnostics,
no source parse diagnostic. Log `/tmp/codex-sim735-lsp.json`.
A symlink at owned worktree parent `.codex-sim735-20261015/cdrom` references
original workspace cdrom for normal CTest corpus lookup; no assets copied.
Full97test suite was launched with RR_ORACLE_DIR pointing at the original
ignored captures. Completion/commit details follow below.

Final private validation and integration artifact:

- Full native CTest completed223.65s:96/97pass, no skips; only agent-tooling
  failed because tracked RE documentation links to an untracked main-checkout
  countdown-driver-trace-20261016.md handoff absent from a clean detached tree.
  Exact original log preserved `/tmp/codex-sim735-full-ctest.log` (exit8).
- Copied that exact existing main handoff to private docs as an **untracked
  local test dependency**, without modifying/committing it. agent-tooling
  rerun passes1/1,1.42s (exit0),
  `/tmp/codex-sim735-tooling-rerun.log`. Thus all97 individual checks passed
  across the full run and targeted environment repair; no claim of one green
  full-suite invocation. Initial full-log tail-only progress statement missed
  the early tooling failure and was explicitly corrected to parent.
- Human sched-human-20261016 single-anchor retains first189 RNG mismatch;
  `/tmp/codex-sim735-human.log`, exit1. No new human parity claim.
- Full Debug build exit0 with no warnings/errors. Focused race/frozen_state2/2
  pass. Diff whitespace check clean. Golden/allowlist/capture content untouched.
- Detached commit **f6363195b911eece7a7924bf97e9b4dff96cada1** contains only
  the5 source/test paths listed above. No shared main history change/push.
- Exact integration mail patch `/tmp/codex-sim735-20261015.patch`, or parent
  may cherry-pick f636319 after coordinating the live main source ownership.
  No oracle_sim_test or CMake change needed. This unique main handoff should
  be staged separately by parent after integration/status merging.
- Private source edits now frozen. Worktree/build retained; only private
  untracked copied handoff remains. All own REA/LLDB/clangd/tests ended;
  no Wine/Ghidra project, capture, install or shared source resource held.
