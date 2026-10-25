# Format enforcement + full-tree clang-format (2026-10-25)

## Task
Owner request: format all C++ source files (weird bracket indenting) and ensure
the formatter always runs on pre-commit.

## Baseline
- `git status --short` clean at start; HEAD 485428a.
- No active pre-commit hook (only `.git/hooks/*.sample`); no `.pre-commit-config.yaml`.
- Existing tooling: `make format` / `make format-check` via `tools/cpp_quality.py`,
  scoped to `src/` + `tests/` only; `tools/ci.sh` had no format gate.

## Changes (uncommitted at handoff time; build+ctest running in build-format/)
- `tools/cpp_quality.py`: scope extended to `src tests fuzz oracle`, `.c` added to
  extensions; `format` mode iterates `clang-format -i` to a fixpoint (bounded 4
  passes) because clang-format 22.1.4 is not idempotent on some trailing-comment
  alignment (first pass aligns, second collapses; seen in `src/settings.hpp:19`,
  `src/game_main.cpp:467`). Verified `format-check` exits 0 after.
- `tools/git-hooks/pre-commit` (new, committed): formats staged C/C++ files under
  `src tests fuzz oracle` with clang-format 22 (same fixpoint loop), re-stages.
  Fails the commit with instructions when clang-format 22 is missing or does not
  converge. Tested with a scratch mangled file: reformatted + re-staged, then
  removed.
- `tools/git-hooks/install.sh` (new): symlinks the hook into `git rev-parse
  --git-dir`/hooks (worktree-safe). Already installed in this checkout;
  re-run via `make hooks`.
- `Makefile`: `hooks` target + help text updated to the new scope.
- `tools/ci.sh`: `python3 tools/cpp_quality.py format-check` as the first gate
  (fail-fast; catches `--no-verify` commits).
- Full tree formatted: 290 files (`src`, `tests`, `fuzz`, `oracle/*.c/*.h`).
  `third_party/` untouched. Brace style is `Attach` per `.clang-format`
  (`} else {`, namespace `} // namespace` comments added).

## Verification
- `python3 tools/cpp_quality.py format-check` -> exit 0.
- Semantic check: comment/whitespace-stripped token stream identical for all but
  9 files; those 9 diffs are normalizer artifacts (spacing around `,`/`+`/`<<`,
  one string literal split into adjacent literals, one macro body wrapped with a
  `\` continuation) - all semantically null. Spot-checked
  `oracle/actor_record_hooks.h` diff.
- Native gate green in `build-format/`: configure + full build OK,
  `ctest`: 100% tests passed, 0 failed out of 106 (`/tmp/fmt-ctest.log`).

## Done
Committed and pushed to `main` (this handoff is part of that commit).
