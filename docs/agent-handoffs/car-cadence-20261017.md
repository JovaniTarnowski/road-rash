# car-cadence-20261017 — car cadence and dt motion; field through tick 307

Task: plan 10 Task 7 continuation; unblock `napa-field-l1-20261017` at tick
227, retain green gates, document and push. Baseline `70a8c04`.
Shared tree; pre-existing untracked research/handoffs/fuzz seeds preserved.
Current owner PID 89267 (session resumed after 84765 exited). Build directories
`build-agent-car-cadence-20261017` and `build-agent-car-cadence-asan-20261017`.
Ownership token `26709a4a-9f95-4cd1-9e37-a9d7b5cf57e0` supersedes the original
car and integration claims; release on completion. No Wine/Ghidra/capture writes.

## Changes

- `src/traffic.{hpp,cpp}`: car last/next/interval/dt; template interval 10;
  exact type-0x10 LOD branch of `0x00437e60`; pre-driver ring window; timer
  spawns first move next tick. Think target/heading and dt acceleration from
  `0x0044e710`/`0x00449880`/`0x004498c0`, dt stop-line lookahead; shared
  `integrate_speed` and `advance_actor_motion` with template drag/sine data.
  Remove the invented proximity horn RNG call; fix signed-byte drift indexing.
- `src/race.cpp`: pass player forward velocity into traffic LOD.
- `tests/field_replay.hpp`: compare reliable captured car motion triples
  (position/lateral/speed) as a multiset, no hydration. Capture kind/phase
  offsets are malformed; no claim to compare those words or pool identity.
- `tests/traffic_test.cpp`: measured dt-11 speed/position fixture, no movement
  or RNG between due ticks, LOD boundary table, signed drift bytes; adapt
  synthetic intersections/stop line and corpus render check to scheduled cars.
  The corpus check renders a spawned car while active, rather than assuming
  one remains at an arbitrary tick 2000. Near-camera red fixture avoids
  assuming per-tick precision in the original far LOD; the mover can creep
  when speed clamps to zero but acceleration remains negative.
- `tests/car_contact_test.cpp`: synthetic PE extends through sine table.
- `tools/sim_known_divergences.json`: advance only the field diagnostic's
  pinned mismatch/output to measured tick 308; original trace unchanged.
- `docs/PROGRESS.md`, `REVERSE_ENGINEERING.md`, `VERIFICATION.md`,
  `SIM_SCENARIOS.md`, plan 10, this handoff.

## Evidence / remaining work

Reference EXE hash rechecked against inventory:
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.
Existing address-keyed exports and assembly used, including `0x0044ee02`
`MOVSX ECX, byte ptr [EAX + 0x475694]`. This supersedes the Oct-16
car-think handoff's word-stride drift interpretation: bytes `01 00 ff 00`.

Unchanged field trace SHA-256 `99e2034496086990d67d130defd3fbea5af6708498d791feb524f5cafab68fe5`:
countdown 5 and ticks 6..307 compare equal, RNG included. First car:
spawn 226, first run 227 dt 11; think speed 341, drag speed 231,
position 34828 (+12). At first remaining mismatch tick 308, pedestrian
13604 original / 13605 native; car 35240/32000/1431 still matches.
Diagnostic output is pinned with explicit `--driver=demo`; no car hydration,
reanchoring or exclusions. `oracle-sim-field` is not acceptance.

Task 7 remains open beyond this requested unblock: interleaved car/pedestrian
list walk; pedestrian tick 308; lane changes, split-road teardown, curved-route
heading transport, car walls/contact and remaining scenarios. Task 6's older
hydrated tick-625 diagnostic is not native traffic parity. No later native
wake/full-race claim.

## Checks

Final native gate: passed, full build + 103/103 CTests (0 skips), viewer smoke
and all-course sweep; `analysis/agent-gates/car-cadence-20261017-final-v4/receipt.json`.
Final ASan/UBSan: 4/4 passed, 0 skipped, including field diagnostic,
`traffic`, `car_contact`, `race_schedule`:
`analysis/agent-gates/car-cadence-20261017-sanitizers-v2/receipt.json`.
Strict human acceptance: exit 0, `1431 ticks identical`, `acceptance_met true`:
`analysis/agent-gates/car-cadence-20261017-final-v4/human-acceptance.json`.

No client LSP available. Owned compilation DB preflight healthy;
Homebrew clangd 22.1.4 AST check with `--check-lines=1-1` exits 0, 0 errors
(`/tmp/rr-car-clangd-final.log`). Full token-feature checks in Apple/Homebrew
clangd hit ExtractFunction tweak failures on loop break/continue, with no
compiler diagnostics. Limited feature-range check still parses the full AST.
Android/device/packaging gates not run in this simulation slice.

Earlier iteration failures are not green receipts: fixtures initially had
undersized PE data, per-tick crossing/stop expectations and arbitrary tick-2000
car lifetime assumptions; field diagnostic needed the CLI-driver output hash.
The first final native run was interrupted because source changed during it;
the first sanitizer run mixed old constructor/new drift assertion. Native v2 passed all runtime tests but failed a plan-relative handoff link;
that documentation link is fixed for native v3. The final native v4 and
sanitizer v2 receipts on frozen final source are authoritative.

## Live processes / resources

No live test/build processes or oracle/Ghidra resources. Source claim held
through commit/push, then released.

The v3 native run passed all simulation checks but the unchanged
`render-threads-alloc` test transiently counted 4 steady-state allocations.
Ten isolated repetitions then passed with its original zero-allocation
threshold. The final v4 native run uses `CTEST_PARALLEL_LEVEL=4`; no renderer
or allocation-test source was changed.
