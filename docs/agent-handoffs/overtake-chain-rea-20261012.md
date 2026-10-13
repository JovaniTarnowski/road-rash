# overtake-chain-rea-20261012

Task: REA spot-check of the plan-10 Task-6 AI overtake chain (`0x00408e40` + `0x00449e90`), per user note to use `npx rea-agents`.

Baseline: `25daed6` (own prior commit; tree carries sibling sessions' uncommitted edits throughout, see below).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added, AI-driving section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/overtake-chain-rea-20261012.md` (this file)

Also created (git-ignored `analysis/oracle/`, disc-derived, never commit):
- `analysis/oracle/rea-408e40-overtake.md` (+ `.log`)
- `analysis/oracle/rea-449e90-scan.md` (+ `.log`)

## Progress / evidence

Two pinned REA calls (`rea-agents@6.4.0`, Ghidra 12.1.4 backend, env per `docs/TOOLING.md`), each exit 0 on the hash-verified reference EXE:

```sh
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00408e40 --format md
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00449e90 --format md
```

Findings (normalized whitespace comparison against `analysis/decompiled/`):
- `0x00408e40`: 2245 vs 2246 chars, NORMALIZED MATCH TRUE.
- `0x00449e90`: 1379 vs 1380 chars, NORMALIZED MATCH TRUE.
- Semantics for the Task-6 porter: `0x00449e90` walks scheduler objects ahead in position order (mask `0x3f`, window `0x960`, lateral gate, `999` = wall); `0x00408e40` latches brain `+0x180`, writes `+0x198`/`+0x194` targets and `+0x19c` speed from `0x00409020` by range. Native `ai_overtake` is still a stub that never writes targets.
- REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

Shared-doc edit: one bullet in `docs/REVERSE_ENGINEERING.md` AI-driving section citing the two local REA files. No source, test, plan, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: the actual `0x00449e90`/`0x00408e40` port (sibling `native-port-cc` scope, needs per-rider half lateral + `0x00408ad0` step-2 fix + state handlers); proxy capture fix (sibling-owned prefix); no captures run.
- Integrator: link from plan 10 Task 6 next index refresh; keep the two `analysis/oracle/rea-*` files local (git-ignored).

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Sibling sessions active in this tree during this task (`playtest-audio-ai-loading` owns `src/sound.cpp`, `src/race.cpp`, `src/game_main.cpp`, `src/race_music.cpp`, `src/mixer.cpp/.hpp`, `tests/sound_test.cpp`, `tests/race_test.cpp`, `tests/music_test.cpp`; `playtest-launch-build` owns `build`; web session owns `docs/WEB.md`, `vercel.json`, `tools/build_web.sh`) — none of their files touched. `oracle/`, prefix/install untouched.
