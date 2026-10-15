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
