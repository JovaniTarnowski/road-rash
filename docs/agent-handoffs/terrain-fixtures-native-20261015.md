# Seeded terrain native fixture harness — 2026-10-15

Plan10 seeded terrain component diagnostic. Baseline `c452be2`.
Isolated checkout `/Users/archit/Dev/rr/.codex-terrain-20261015/native-port`.
Main source checkout was not edited.

Owned and changed:

- `src/terrain.hpp`
- `src/terrain.cpp`
- `tests/terrain_test.cpp`
- This handoff.

Added read-only value snapshot `TerrainIterator::diagnostic_state()` returning
`TerrainIteratorState`: signed32 position/last_sample, size_t hill record index,
and all8 `TerrainPointState` entries (signed32 start/end/x/height/dx/dh). This
exposes the two interpolation points absent from TerrainSlice. Interpolation,
record transitions, random order, and terrain output behavior are unchanged.

The terrain test accepts a separate optional seeded golden at default
`RR_ORACLE_DIR/terrain-seeded-20261015.tsv`, or explicit first CLI argument. A
missing default produces an explicit UNVERIFIED message and performs native
traversals only; a missing explicitly requested path fails. Existing
raster-fixtures.tsv fixed-random comparison remains intact.

Schema coordinated with terrain_review:

- Standard complete rr-oracle1 file with original EXE hash and exactly1 frame.
- `terrain.seeded.schema=1`.
- Prefix `terrain.seeded.<seed_index>.<path>.<tick>.` with fields rng (full
  unsigned32 state), position,last_sample,record,point.<0..7>.<start,end,x,height,dx,dh>.
- Seeds in order0,1,123,32767,0x12345678,0xdeadbeef. Each path reinitializes the
  RNG before iterator construction. Snapshots therefore include constructor draws.
- Paths0/1:48 pre-step snapshots, initial0/+256 or initial48*256/-256.
- Path2:64 pre-step snapshots, initial0; first32 advances+256 then32 advances-256.
- Required `terrain.seeded.<seed_index>.<path>.final_rng` after final step.
- Thus960 full states and18 terminal RNG values. Every expected field is required
  when a golden exists; its end marker and duplicate-field validation come from
  golden.hpp. No self-generated golden was made.

Validation:

- Owned configure/build target terrain-test succeeded; logs
  `/tmp/rr-terrain-native-configure-20261015.log` and
  `/tmp/rr-terrain-native-build-20261015.log`.
- `RR_ORACLE_DIR=/Users/archit/Dev/rr/native-port/analysis/oracle ctest --test-dir build-agent-terrain-native-20261015 -R '^terrain$' --output-on-failure`
  passed1/1. Existing864 original terrain slices matched. The genuine seeded
  golden is still missing, so seeded parity is UNVERIFIED. Test log
  `/tmp/rr-terrain-native-test-20261015.log`; full output in owned build
  `Testing/Temporary/LastTest.log`.
- Isolated checkout has no sibling original-disc corpus, so the optional course
  corpus traversal skipped. No commercial data copied.
- No client LSP tool exposed; fallback `tools/agent_lsp.py` with owned compilation
  database and `--check tests/terrain_test.cpp` exited0 with zero errors; log
  `/tmp/rr-terrain-native-lsp-20261015.json`.
- `git diff --check` passed. No shared compilation database edited.

Next: terrain_review owns isolated oracle proxy changes and genuine seeded
capture. Root integrates after its main detached-half gate. Run terrain-test
with the genuine seeded file to establish seeded original parity. No commit,
Wine/Ghidra process, or held shared resource by this worker. Native source edits
are finished and frozen for root integration.

## Optional live ring walker terrain diagnostics

Next ownership transfer from root: isolated `src/track_ring.hpp`,
`src/track_ring.cpp`, `tests/oracle_sim_test.cpp` plus this handoff only. Main
source remains untouched. Private baseline still c452be2; root must integrate
only this narrow delta to preserve its later detached-half changes.

Added read-only value diagnostics `TrackRing::diagnostic_state()` returning
`TrackRingState {front,back}` of `TrackWalkerState {position,optional terrain}`.
Positions are local walker section coordinates, not absolute ring cursors.
Terrain uses the existing TerrainIteratorState snapshot; no state advancement,
random draws, pointer exposure, or mutation.

Replay schema agreed with terrain_review:

- Initial frame `ring.terrain.schema=1` enables comparison; unsupported values
  fail. Absent declaration leaves older captures unchanged.
- Every compared frame requires `ring.walk.<front|back>.position` and
  `.terrain.available` (0/1). Native terrain present then requires
  `.terrain.position,last_sample,record` and all8
  `.terrain.point.<i>.<start,end,x,height,dx,dh>`.
- Compare initial state immediately after factory setup against frame0's
  post-tick1 state, before any countdown replay. Compare each native countdown
  tick and each normal tick against its matching original frame. Replay modes
  skipping countdown also check every still-held original frame.
- First missing field/value mismatch reports exact frame,tick,key and values
  and returns1, independent of RR_SIM_CONTINUE. No reanchoring or existing-field
  filtering added. Original-only optional `.active` does not represent native
  terrain interpolation state and remains diagnostic context.

Validation:

- Private build `--target oracle-sim-test track_ring-test --parallel 4` passed;
  log `/tmp/rr-ring-terrain-native-build-20261015.log`.
- Focused track_ring passed. Initial combined CTest oracle-sim failure was the
  isolated checkout's missing default golden (all-skip rejected by wrapper),
  recorded `/tmp/rr-ring-terrain-native-test-20261015.log`.
- Ran existing main legacy fixture/disc via absolute paths with verify_sim.py
  diagnostic mode. Exit0, exactly1 pinned known divergence at tick222 lateral;
  zero unexpected failures/skips. Thus absence of the new schema preserves the
  legacy diagnostic. Report `/tmp/rr-ring-terrain-native-legacy-report-20261015.json`,
  output `/tmp/rr-ring-terrain-native-legacy-20261015.log`.
- clangd fallback parsed/indexed oracle_sim_test.cpp without source diagnostics;
  Apple ExtractFunction feature checks exit3 with19 break/continue tweak errors,
  log `/tmp/rr-ring-terrain-native-lsp-20261015.json`.
- `git diff --check` passes for all3 new owned files.

Actual declared live ring trace comparison remains pending root's genuine
capture/integration; no new original trace or synthetic golden was generated.
The proxy worker separately completed its isolated original trace producer.
No commit, main checkout write, Wine/capture/Ghidra process, or resource held.
All native source edits are now finished and FROZEN for root integration.
