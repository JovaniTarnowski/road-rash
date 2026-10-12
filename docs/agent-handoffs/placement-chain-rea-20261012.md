# placement-chain-rea-20261012

Task: REA spot-check of the plan-10 Task-6 grid-placement chain (`0x00405990` → `0x004387a0` → `0x00438930` → `0x00439b10`), per user note to use `npx rea-agents`.

Baseline: `e2dce9a546cb057dcb748ee832ad3127db7601b4` (tree carried a sibling session's uncommitted edits throughout; see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (configure + full build only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, pair-construction section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/placement-chain-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-005990-placement.md` (+ `.log`)
- `analysis/oracle/rea-4387a0-subslice.md` (+ `.log`)
- `analysis/oracle/rea-439b10-slice.md` (+ `.log`)

## Progress / evidence

Three pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`: Node v24.13.0, `GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`, JDK 21), each exit 0 on the hash-verified reference EXE:

```sh
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00405990 --format md
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x004387a0 --format md
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00439b10 --format md
```

Findings:
- `0x00405990` / `0x004387a0`: REA pseudocode whitespace-identical to the project's `analysis/decompiled/` exports (verified by normalized comparison for `0x00439b10`: 6818 vs 6819 chars, normalized match TRUE; `0x004387a0` visually identical line-for-line).
- Placement chain: `0x00405990` lateral = `*(slot + 0xcc) + grid`; `0x00438930` sets bike `+0x1c = *(*(slot + 0x28) + 8) + grid`; `0x00439b10(slot, type, lateral)` resolves the sub-slice by slot `+0x3c` type switch (0 flag-gated, 1 ramps, 3 edge, 4 lateral blend, 5 gate).
- This closes the sibling (`native-port-cc`, plan 10 Task 5) session's open "`*(slot+0x14)+0xcc` placement field" item at trace level. Replay seeding of frozen laterals from the frame-0 dump remains the Task-6 stopgap.
- REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` pair-construction section citing the three local REA files. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` + `cmake --build build --parallel`: pass (docs-only change; build unaffected).
- `ctest --test-dir build`: 69/69 pass baseline observed earlier this session (before the doc edit; doc edit needs no rebuild).
- NOT done: proxy `g_race_started` menu re-entry fix (diagnosed, unmade — needs a live capture to validate and the oracle prefix/DLL-install is shared with the sibling); no captures run (sibling owns the prefix); sibling Task-6 replay seeding unchanged.
- Integrator: consider linking this note from plan 10 Task 6 / `00-INDEX.md` next index refresh; keep the three `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling session (`playtest-audio-ai-loading`, files `src/sound.cpp`, `src/race.cpp`, `src/game_main.cpp`, `src/race_music.cpp`, `src/mixer.cpp/.hpp`, `tests/sound_test.cpp`, `tests/race_test.cpp`, `tests/music_test.cpp` plus `.gitignore`) was active in this tree during this task — none of its files touched. `oracle/aweman_proxy.c`, `oracle/build/`, prefix/install untouched.
