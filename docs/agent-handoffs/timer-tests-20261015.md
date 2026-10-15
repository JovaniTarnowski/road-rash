# Countdown timer synthetic regressions — 2026-10-15

Plan: 10 Task 7, countdown traffic timer banking. Baseline: `5524bf6`.
Parent transferred ownership of `tests/traffic_test.cpp` to this worker.

Changed files:

- `tests/traffic_test.cpp`
- `docs/agent-handoffs/timer-tests-20261015.md`

Added three always-on synthetic tests using the existing synthetic vehicle and
PE helpers:

- A native `Race` holds for 59 ticks and banks oncoming `3515-531`, behind
  `531`, ahead `-3515+531`. RNG, player position, lateral, speed and view camera
  remain unchanged, with no active cars.
- Initial ring fill applies density40 before timers: both reloads become23437,
  while the existing behind/ahead counters remain9 and `-3515+9`. The ring's
  zone event occurs once across 59 holds and the first racing tick.
- Disabled spawns expire at tick391 after zero draws through tick390, consume
  exactly two lane RNG draws, reload timers, and leave every car inactive with
  serial zero through tick400.

Checks:

- Owned configure: `cmake -S . -B build-agent-timer-tests-20261015 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` passed.
- `git diff --check -- tests/traffic_test.cpp` passed.
- No client C++ LSP tool was exposed. Fallback `python3 tools/agent_lsp.py
  --database build-agent-timer-tests-20261015/compile_commands.json
  --check tests/traffic_test.cpp` parsed/indexed the file without source
  diagnostics, but Apple clangd feature testing exited3 on four pre-existing
  ExtractFunction tweak failures involving break/continue. Log:
  `/tmp/rr-timer-tests-lsp-20261015.json`.
- Parent requested it own the compile/test gate after integration. Pending parent
  `traffic-test` execution and full native gate; no commit made by this worker.

No capture, Wine, Ghidra, or shared resource process held. No production source
or shared status documentation edited.

## Stable rider-node identity follow-up

Parent transferred ownership of `tests/frozen_state.hpp` and
`tests/frozen_state_test.cpp` for the target-pointer normalization fix.
Added `rr::golden::rider_node_identities(header,riders)`: capture once after
initial hydration, mapping complete numeric `nodeaddr.N` triples by initial
bike +0x1c for types4/8. Complete unrelated/unmatched nodes map-1; absent and
incomplete addresses remain absent. Duplicate captured addresses and ambiguous
matched native rider positions throw. The helper returns a value-owned map so
later movement and reuse of the initial position retain the original identity.

Focused tests cover linked bike/half identity, unrelated/unmatched nodes,
unknown/incomplete/nonnumeric entries, movement/position reuse, duplicate native
positions, and duplicate addresses. The parent owns replay integration.

Validation:

- `cmake --build build-agent-timer-tests-20261015 --target frozen_state-test --parallel 4` passed (log `/tmp/rr-frozen-state-build-20261015.log`).
- `ctest --test-dir build-agent-timer-tests-20261015 -R '^frozen_state$' --output-on-failure` passed,1/1.
- clangd fallback `tools/agent_lsp.py` against the owned compilation database
  checked `tests/frozen_state_test.cpp` with exit0/zero errors (log
  `/tmp/rr-frozen-state-lsp-20261015.json`).
- Initial test run exposed the legacy hydration parser's acceptance of a
  nonnumeric node suffix; moved that synthetic malformed entry after hydration
  so this helper's parsing coverage stays independent of legacy hydration.
- No replay source, CMake, shared status documentation, or production source
  changed. No commit made. No live resource process held.

## Detached frozen half scan state

Parent transferred ownership of `src/race.hpp`, `src/race.cpp`,
`tests/frozen_state.hpp`, `tests/frozen_state_test.cpp`, and
`tests/race_test.cpp`. Baseline `c452be2`.

Evidence supplied by parent: demo576 target brain180 points to type8 node
20988784, captured position21248. Bike contact575 moves the linked bike to21265;
null frozen half think/move callbacks leave its position/speed independent.
The linked half velocity is the BIKE node's `.half108`; the half node's own
`.half108` describes its absent linked half and is a sentinel.

Changes:

- Rider stores detached half position/speed in addition to its lateral.
- `build_scan_list` overrides all three fields for detached type8 nodes and
  chooses the route sample from the half's clamped position. Attached live halves
  keep the existing bike-derived position/speed/sample.
- Frozen hydration validates/stages the pointer-linked half position and optional
  bike `.half108` before changing riders. Old `probe.frozen_words=1` dumps predate
  `.half108`, so absence remains compatible with speed0. Legacy no-marker dumps
  use the matched initial position and available bike `.half108`, else speed0.
  Missing old speed evidence is a limitation, not a recovered measurement.
- Fixtures exercise signed half speed, ignored half-node sentinel, atomic invalid
  present speed rejection, and compatibility without the speed field.
- Native AI regressions place a frozen half at sample83 and displaced bike at90.
  The overtake scan selects the retained half when closing and its bike when the
  independent half speed prevents closing. Different route offsets establish that
  the passing point comes from the half's own slice.
- Existing `half_x` follow behavior remains unchanged: target bike position plus
  half lateral has no contradictory evidence in this slice. No keyed existing
  00409ca0 export was found with fd/rg in analysis, and no new RE run was needed
  to implement the parent-supplied confirmed scan evidence.

Validation:

- Owned `cmake --build build-agent-timer-tests-20261015 --target frozen_state-test race-test --parallel 4` passed.
- `ctest --test-dir build-agent-timer-tests-20261015 -R '^(frozen_state|race)$' --output-on-failure` passed2/2.
- clangd fallback parsed/indexed `src/race.cpp` without source diagnostics. Apple
  clangd feature tests again exit3 on ExtractFunction break/continue errors (38),
  logged `/tmp/rr-detached-half-lsp-20261015.json`. Compile gate passes.
- `git diff --check` passed for all five owned files.
- Parent owns actual replay and full integration gate. No commit, shared-doc
  change, replay-source edit, or live resource process held by this worker.
