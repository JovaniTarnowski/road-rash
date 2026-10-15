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
