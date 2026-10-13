# C++ formatting and static analysis

Use LLVM **22.x** (`clang-format` and `clang-tidy`; verified locally with
22.1.4). The wrapper rejects other major versions so formatting does not depend
on whichever LLVM a developer happens to have on PATH. Install LLVM 22 with your
package manager; on this Mac `brew install llvm` supplies both tools. The wrapper
also searches Homebrew's keg-only LLVM directory and versioned `*-22` binaries.
Set `CLANG_FORMAT` / `CLANG_TIDY` to explicit executable paths if needed.

Run from `native-port/`. Claim the files you will edit and format only that slice
while other sessions are active:

```sh
make format FILES='src/input.cpp src/input.hpp'
make format-check FILES='src/input.cpp src/input.hpp'

cmake -S . -B build-agent-MYSESSION -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
# Generate the build-info header before linting game_main.cpp.
cmake --build build-agent-MYSESSION --target rr-build-provenance
make lint BUILD_DIR=build-agent-MYSESSION FILES='src/input.cpp'
```

The formatter uses four spaces, attached braces and a 110-column limit. Include
order and evidence comments are preserved. Editors supporting `.clang-format`
can use the same configuration; configure their formatter executable to LLVM 22.
The wrapper limits both tools to first-party C++ under `src/` and `tests/`,
including untracked additions and excluding ignored files when selecting all.
It does not process vendor headers, oracle C/proxy code, generated files or
disc-derived analysis. Explicit filenames must be existing first-party C++ files.

Lint uses the configured build's compilation database rather than guessed flags.
Refresh that database after CMake/source changes; a missing selected translation
unit is an error. Use a native build for this workflow. Headers are analyzed
through selected `.cpp` translation units; a header-only lint selection is an
error. Select the consuming translation units when changing a header.

The initial lint policy enables Clang's static analyzer plus selected bugprone
checks for dangling handles, use-after-move, integer division, sizeof/memset,
macro parentheses and assertion side effects. Findings cause a nonzero exit.
No lint fixes are applied automatically. Review findings against original field
widths, integer wrapping and object layout before changing code. Broad
modernization, narrowing and naming policies are not enabled. For a justified
suppression, use `NOLINTNEXTLINE(check-name)` with the original address/evidence
and a short explanation; do not disable a check globally to hide one finding.

## Rollout and verification

Run formatting and lint checks on every owned C++ change. Without `FILES`,
`make format`, `make format-check` and `make lint` select all first-party C++.
Coordinate a full formatting migration with active source owners before running
the all-file rewrite. A full read-only `make format-check` currently fails on
202 files in the shared October 12 tree. These are existing style differences;
this tooling change does not rewrite those files. A full lint audit remains to
be done, followed by evidence-based fixes/suppressions. The gates are available
to CI via Make, but are not yet mandatory in `tools/ci.sh` while this backlog
remains. Do not interpret the existing native CI pass as a C++ quality pass.

Validation in this session:

- Configuration parsing: `clang-format --style=file --dump-config` and
  `clang-tidy --config-file=.clang-tidy --verify-config` passed with LLVM 22.1.4.
- Native compilation database configured in `build-agent-cpp-quality-20261012`.
- Scoped lint passed for `src/input.cpp`, `src/resource.cpp` and
  `tests/input_test.cpp`, including their first-party headers. Dependency
  diagnostics were suppressed by the header filter.
- An isolated synthetic fixture verified formatting rejection, rewrite, clean
  check and idempotence, missing-database rejection, and a nonzero lint exit for
  a null dereference. No repository C++ files were changed by this check.
- Full formatting check failed as expected; local logs:
  `analysis/cpp-quality-format-all.log`, `analysis/cpp-quality-lint.log` and
  `analysis/cpp-quality-lint-focused.log`.
- Native/Android game builds and runtime tests were not run: source/build
  behavior was unchanged. This validates the tooling, not gameplay fidelity.

References: [clang-format options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
and [clang-tidy workflow](https://clang.llvm.org/extra/clang-tidy/).
