# Offline field counts — October 18, 2026

Baseline: `6044526f20244f3f5990d0c3876e397b81cb436f` (`origin/main`).
Worktree: `/Users/archit/Dev/rr/.field-count-20261018`.
Build: `build-agent-field-count-20261018` (Debug).
Shared main checkout has unrelated dirty edits and two divergent local commits;
none were changed, staged or rebased. Current OMP session inventory was read
from `native-port/analysis/field-count-sessions.json`; file/resource claims and
latest police handoff were inspected before edits. No delegated agent work.

## Implemented / evidence

- Original 0x00405600 assembly: offline capacity 16, AI loop rows 1..14;
  mode 2 skips selected character row (0x00405783..0x004057a4). Thus Thrash
  has fourteen opponents and Big Game thirteen. Evidence addresses, verified
  disc hash and static/runtime limits are in REVERSE_ENGINEERING.
- rr-game and RaceConfig default to fourteen rows; mode reaches build_race,
  which omits the selected Big Game row without shifting other grid slots.
  Career::spare_levels remains wired and now affects the six live spare rows.
- Shared race_rider_names uses personality +0x165, course slots and the
  original name_roll easter-egg tables. Both game entry paths use it. Finish
  standings use the same indexed names and exclude cops.
- Changed: src/game_main.cpp, src/race_setup.cpp/.hpp,
  tests/race_setup_test.cpp, tests/race_screen_test.cpp,
  tests/frontend_test.cpp, README.md, PROGRESS, VERIFICATION,
  REVERSE_ENGINEERING, and this handoff. No changes to shared simulation
  headers, oracle proxy, CMake registration or golden fixtures.

## Validation

See VERIFICATION for exact commands and results. Original mode counts are
static constructor evidence; existing field replays corroborate fourteen-row
construction. No new live mode-selection capture or broad parity claim.
Local logs: `analysis/field-count-20261018/` in this worktree.
Full Debug build, focused setup/screen/frontend tests and CLI default/14/7/0
field reports passed; final full-suite result recorded in VERIFICATION.
Clangd source checks passed with tweak applicability probes disabled.

## Remaining / resources

All 104 CTests passed with zero skips (286.31 seconds); the native implementation
and documentation are complete. Commit/push to main follows this handoff update.
File claim token `ce6af417-f750-4fe8-a96b-180ca18dc4a6`, owner PID 57073;
release at completion. No owned Wine or Ghidra processes. Oracle/decompiled
symlinks under analysis read existing shared fixtures only. Preserve this
checkout for inspection; shared main intentionally stays untouched.
