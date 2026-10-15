# Plan 10 frozen initialization replay support — October 14, 2026

Baseline 02d7e8d in isolated `/Users/archit/Dev/rr/.codex-sim-20261014/native-port`.
Owned changes: `tests/oracle_sim_test.cpp`, new `tests/frozen_state.hpp`,
new `tests/frozen_state_test.cpp`, this handoff. Integrator owns CMake and
oracle changes. No commits, captures, Wine, shared-prefix writes, or held
resources. Other modifications preserved.

The replay now applies declared frame0 `probe.frozen_words=1` state once
before countdown/green to frozen solo riders in both human and demo paths.
Non-frozen player and demo brain logic remain untouched. Raw previous
copies +0x30..+0x40 retain their measured values independently of current
lateral; bike +0x108/+0x10c/+0x110 velocities, +0xc0 byte flags, +0x2f4
crash state, current lateral, and linked type8 half lateral are hydrated.
Strict marker requires complete fields for every matched frozen type4 bike,
validates word/byte ranges, unique bike positions and node addresses, and
requires the bike's actual +0x2fc pointer to link a same-position type8 half.
All validation completes before any rider mutation. Replay prints errors
and exits1 on invalid declared state. Unmarked captures preserve old human
behavior and demo-only lateral/derived-copy fallback; no-node fallback
preserves native grid state.

Evidence/checks:

- Synthetic failing test first: compiled against no-op helper, exit1 with
  17 expected failures before implementation.
- Standalone focused test with `-std=c++20 -Wall -Wextra -Werror`: exit0,
  0 failures. Cases: human/demo, negative INT32_MIN, valid zero words,
  current!=previous, flags255, no player mutation, unrelated same-position
  half, atomic failure on later missing rider, missing word, duplicate
  positions/addresses, missing/wrong-type/wrong-position linked half,
  out-of-range byte/i32, unsupported marker, old human/demo/no-node behavior.
- Same focused test under ASan+UBSan: exit0, 0 failures.
- Replay `c++ -std=c++20 -Wall -Wextra ... -fsyntax-only`: exit0, existing
  unused `direct` warning at line78. A `-Werror` attempt fails on that same
  pre-existing warning, left outside scoped edits.
- Private compile database `/tmp/frozen-state-lsp-20261014/compile_commands.json`.
  Homebrew clangd22 `--check` focused test: exit0, 0 errors. Replay check
  exits3 on known ExtractFunction tweak errors; no C++ diagnostics.
  Logs `/tmp/frozen-state-test-clangd-20261014.log` and
  `/tmp/frozen-state-replay-clangd-20261014.log`.
- `git diff --check` scoped edits passed. Integrator notified as soon as
  focused test existed for CMake registration. Integration build/CTest and
  coordinated fresh-capture replay remain integrator gates, not claimed here.

This repairs diagnostic initial-state coverage. It does not prove native
grid setup or simulation parity, and does not change any divergence manifest.
