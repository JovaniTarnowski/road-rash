# Road Rash native port — agent instructions

## Scope and entry points

This Git repository is `native-port/`; its parent `rr/` is a workspace containing
the owner's disc, a legacy Wine install and a separate Wine-on-Android setup.
Run Git and native build commands from this repository.

The deliverable is native C++/SDL3 on macOS ARM64 and Android ARM64. Wine is
used only to inspect/capture the original for this port. The opening Napa race
is the current fidelity milestone; consult `docs/plans/00-INDEX.md` for current
dependencies before implementation or planning. Match plans to the actual
interfaces before using their snippets. Plans 02–07 describe older architecture;
plan 12 is superseded by 17.

Use `README.md` for commands, `docs/TOOLING.md` for analysis/environment pitfalls,
`docs/REVERSE_ENGINEERING.md` for binary evidence, `docs/VERIFICATION.md` for
coverage, and the relevant plan for acceptance. Read what the task needs.

## Preserve concurrent work

- Check `git status --short` and recent commits before editing. Existing changes
  may belong to another agent. Preserve them; do not reset, restore or delete
  unfamiliar changes to make your task easier.
- Before parallel implementation, establish file ownership. Give each worker a
  separate worktree when practical. Serialize edits to shared simulation headers,
  `game_main.cpp`, the oracle proxy and shared documentation. One integrator
  updates shared status and accepts worker changes.
- Use a separate build directory per worker/worktree. A build lock protects the
  build directory, not source files, Wine installs or Ghidra databases. Coordinate
  those shared resources separately. Never remove another worker's lock merely
  because it is old; verify its owner is gone.
- When committing is in scope, stage explicit paths and inspect the staged diff.
  Avoid `git add -A` and commits of another worker's in-flight changes. Do not
  rewrite shared history to untangle interleaved work.

## Reverse engineering and fidelity

- Use the unmodified disc binary at `../cdrom/ROADRASH/ROADRASH.EXE` as reference;
  check its hash against `analysis/inventory.json`. The parent's `android/`
  executable is patched and is not a reference.
- Trace behavior to addresses and field widths. Decompiler pointer indexing is
  element-scaled; verify byte offsets, signedness and wraparound against assembly.
  Suspect or empty pseudocode needs a disassembly check.
- Use existing address-keyed exports first. REA is a spot-check, not runtime proof;
  use the tested version and environment recorded in `docs/TOOLING.md`. Keep
  speculative mappings explicitly labelled until validated.
- Keep the original 60 Hz simulation, integer semantics and shared MSVC RNG
  behavior intact. Rendering and optional visual enhancements must not consume
  simulation randomness or change race results; original rendering remains the
  fidelity baseline.
- Acceptance replays anchor RNG once and then free-run. `--reanchor` is diagnostic.
  A scenario called "human" must prove it reaches the player-control branch;
  attract-demo replay does not establish player-control fidelity.
- Distinguish component fixtures, original-list replay, native scene construction,
  simulation replay, live gameplay and device validation. State exactly what a
  comparison covers. Never change a golden just to make a test pass.
- Keep commercial assets, disc-derived dumps and upscaled artwork local and out
  of source/APK/app packages. Use synthetic committed parser fixtures.

## Verification and tools

- Normal native gate: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`,
  `cmake --build build --parallel`, then
  `ctest --test-dir build --output-on-failure`. Substitute your owned build directory.
  `make help` lists existing convenience targets; `make run` runs `rr-game`,
  while `make viewer` runs the research viewer.
- Run focused tests during iteration; run the integration gate after integrating
  code changes. `tools/ci.sh` includes Android debug assembly and asset checks;
  Android release assembly, codecs and standalone packaging have separate gates.
  Use sanitizer/oracle checks appropriate to the change. No build is needed for
  a documentation-only edit.
- Report skipped tests and expected failures separately. `oracle-sim` currently
  uses CTest `WILL_FAIL`; a green suite does not prove simulation parity.
  Do not accept a different failure as a known divergence.
- Preserve command exit codes. Log long commands to files and inspect the logs;
  a `tail`/`grep` pipeline or `|| true` must not turn a failed build into success.
- Oracle captures mutate shared state. Coordinate the prefix, DLL install, output
  name and process ownership before running them; stop only your own processes.
  Require a valid header, contiguous ticks and completion marker before using a
  capture. Do not overwrite a previous accepted capture with a partial run.
- `tools/decompile.sh` overwrites the analysis import. Preserve manual annotations
  before using it; serialize writes to the Ghidra project.
- APK assembly is not hardware validation. Check the selected `adb` serial and
  record what was actually observed for import, rendering, controls and lifecycle.

## Progress and handoffs

Document all progress in `docs/`. For implementation, update `docs/PROGRESS.md`
with the change and remaining gaps; update `docs/VERIFICATION.md` with commands,
outcomes, skips and evidence paths; put recovered facts in
`docs/REVERSE_ENGINEERING.md`. Update the applicable plan status when it changes.

During concurrent work, write a uniquely named `docs/agent-handoffs/<session>.md`
instead of competing to rewrite shared summaries. Include task/plan, baseline
commit, owned and changed files, evidence, checks, remaining work, live process
IDs and held resources. The integrator merges the handoff into shared docs.
Research notes under `analysis/` do not replace this documentation requirement.

Plans must describe portable actions and acceptance, with optional client-specific
skills. If a named skill is unavailable, carry out the equivalent workflow and
record the limitation. Continue authorized work through validation and documentation;
when interrupted, leave a concrete next action and distinguish partial work from
completion.
