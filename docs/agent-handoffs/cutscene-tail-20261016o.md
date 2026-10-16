# cutscene-tail-20261016o — Plan 13 background: cutscene tail / MCI presentation `0x00402820`

Task: READ-ONLY background on the cutscene tail / MCI presentation —
`0x00402820` destination geometry (normal native-size vs doubled placement),
`0x00402650` open/put/play chain, `0x004023e0` frontend mode/clip selection.
Spot-check only; no builds/captures, no commits. Sibling render/road edits
left untouched and unread.

Baseline: `5ffafa9` (verified via `git rev-parse HEAD` at turn start).
Worktree: `/Users/archit/Dev/rr/native-port` (shared tree; sibling
modifications present, none touched).

## Owned / changed files (new paths only, no tracked-file edits)

- `docs/agent-handoffs/cutscene-tail-20261016o.md` (this file)
- `analysis/cutscene-20261016o/` — `rea-402820-put.md/.log`,
  `rea-402650-open.md/.log`, `geometry-note.md`

## Evidence (2 REA calls — budget maxed)

Pinned: `rea-agents@6.4.0`,
`PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`--format md`. Reference EXE sha256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(REA-reported digest matches on both calls).

| Addr | Role | evidence_id | Outcome |
| --- | --- | --- | --- |
| `0x00402820` | MCI destination setup (window/status/where/put) | `ev_d642108599bc8c3603a791b62664dcc5910987fd8a105fec7dfdd8cf8c6554d7` | MATCH: REA pseudocode + disassembly agree with `analysis/decompiled/00402820.c` (`0x14` default y-base, `0xf0 − h` when flag bit 0 clear, `0xa0` x + `h>>1` small branch, doubled branch otherwise, mode `!= 2` gate) |
| `0x00402650` | open/load/put/cue/play chain around `0x00402820` | `ev_51c5372288db52b18909bd24ebe3fa2724c5ea461a3270ebbd3537c3a4414924` | MATCH: agrees with `analysis/decompiled/00402650.c`; REA `callers` lists exactly one caller, `0x004023e0` |

Prior coverage reused (not re-run): `0x00449150` + `0x004023e0`
(`outcome-videos-20261016j`), `0x004210a0` + `0x00402f50`
(`cutscenetail-rea-20261012`), outcome/results chain (`0x00416010`,
`0x00416120`, `0x00421290`). Known REA gaps unchanged: `0x0041f6d0`,
`0x004214e0`.

## Geometry table (`0x00402820`, 640x480 window; w/h = clip size)

From disassembly `4028b0-402924` (REA-confirmed); detail in
`analysis/cutscene-20261016o/geometry-note.md`.

| flag bit 0 | VideoMode (`DAT_0047538c`) | x | y | w | h |
| --- | --- | --- | --- | --- | --- |
| clear | != 2 | 160 | 240 − h/2 | w | h |
| set | any | 0 | 20 | 2w | 2h |
| clear | == 2 | 0 | 240 − h | 2w | 2h |

Call-site flags (from `analysis/original-disassembly.txt`): showroom = 1
(doubled at (0, 20)); course-Start = 2; outcome/jukebox/menu = 4; logo
chain = `0x20` (skips soundtrack handling); attract JESSIE/DUEL flag slot
OPEN (unresolved this turn). Native `queue_video` gates only on
`video_mode == 0` and has no flag/mode distinction; `play_video`
(`src/game_main.cpp:146`) is owner-directed fullscreen aspect-fit;
`Matte` blit remains a documented no-native-equivalent difference.

## Verification plan (for owner/integrator; NOT run this turn per no-build scope)

- Normalized-whitespace diff of the two REA pseudocodes vs
  `analysis/decompiled/00402820.c` / `00402650.c`.
- Resolve the `0x423d71` attract-call flag slot from the `0x00423d20`-region
  stack setup.
- `ctest -R frontend` + full `cmake --build build --parallel` + `ctest` gate
  at integrate time.
- No replay/pixel claim from REA (spot-check per `docs/TOOLING.md`).

## NOT-changed list (inspected or deliberately avoided, untouched)

- Sibling in-flight edits (neither read nor touched):
  `src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
  `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp`
- Inspected, untouched: `src/video_player.cpp/.hpp`, `src/frontend.cpp`,
  `src/game_main.cpp`, `analysis/decompiled/004023e0.c`,
  `analysis/decompiled/00402650.c`, `analysis/decompiled/00402820.c`,
  `analysis/original-disassembly.txt`, `docs/REVERSE_ENGINEERING.md`,
  `docs/TOOLING.md`, prior handoffs listed above
- `build/`, `oracle/`, parent `android/`/`prefix/`/`cdrom/` untouched.
- No tracked file created/modified/deleted; the turn delta is only the new
  paths above. No commits made.

## Remaining / handoff to integrator

- `0x00402820` is now independently REA-pinned (first pin); suggest one
  `REVERSE_ENGINEERING.md` bullet referencing this handoff's evidence_ids.
- Open (unchanged): frontend mode/clip selection (which VideoMode value each
  screen path uses), attract-call flag slot, `Matte`-blit architecture gap,
  `0x004214e0`/`0x0041f6d0` Ghidra gaps, DUEL mano-a-mano gating (plan 09).

## Live processes / resources

No live REA/Ghidra/Wine processes (both REA calls exited 0; ephemeral
backend only, project Ghidra untouched). No build locks, no captures, no
held resources.
