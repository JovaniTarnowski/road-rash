# outcome-videos-20261016j — Plan 13 background: outcome/cutscene selection

Task: READ-ONLY background on outcome/cutscene selection — busted/wrecked movies, level-up/finale, BUSTED1-6, results flow `0x00416010`. Spot-check only; no builds/captures, no commits.

Baseline: `45ae3ed` (verified via `git rev-parse HEAD` at turn start).
Worktree: `/Users/archit/Dev/rr/native-port` (shared tree; sibling untracked work present, none touched).

## Owned / changed files (new paths only, no tracked-file edits)

- `docs/agent-handoffs/outcome-videos-20261016j.md` (this file)
- `analysis/outcome-20261016j/` — `rea-449150-selection.md/.log`, `rea-4023e0-variant.md/.log`, `selection-law.md`

## Evidence (2 REA calls — budget maxed)

Pinned: `rea-agents@6.4.0`, `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`, `GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`, `JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`, `--format md`. Reference EXE sha256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (REA-reported digest matches; locally re-verified via shasum).

| Addr | Role | evidence_id | Outcome |
| --- | --- | --- | --- |
| `0x00449150` | results screen → video-base selection | `ev_0c916afb2e8f3a1500dcfb9610a1fad449a6aac706fee2237ce6e7e0ea0a7bc4` | MATCH: pseudocode + disassembly agree with `analysis/decompiled/00449150.c` (cases `0x1c`–`0x1f`, EDX 6/6/6/0xA, FinalWin XOR EDX, flag push 4) |
| `0x004023e0` | variant + path dispatch | `ev_4ced5fccb87323eeb921d7da537ff2775f18c80b351a9ccdeb92d54c502974c2` | MATCH: `rand()%n+1` + `sprintf("%s%d")` + `Video/` path (REA lines ~270/2180) agree with `analysis/decompiled/004023e0.c` |

Name bytes verified from EXE via PE section map: `0x475568`=Wreck, `0x475570`=Busted, `0x475578`=Lose, `0x475580`=Win, `0x475584`=Level, `0x47558c`=FinalWin; format `"%s%d"` at `0x46344c`, `"Video/"` at `0x46343c`; `0x46b0fc`=Start.

## Selection law (`0x00449150` → `0x004023e0(base, n, 4)`)

- `0x1c` (wrecked) → `Wreck`, 6
- `0x1d` (busted) → `Busted`, 6
- `0x1e` (won) → qualified `!=0x1f` → `Win`, 6; qualified `==0x1f` and level `!=4` → `Level`, 6; level `==4` → `FinalWin`, 0 (exact). Course bit (`1,2,4,8,0x10`) OR-ed into qualified when mode `!=3`.
- `0x1f` (lost) → `Lose`, 10
- default/menu → `NoVideo` / `StudLogo`+`PapyLogo`+`Intro` chain
- Variant: `n==0` verbatim, else `base + (rand()%n+1)`.

## Selection table (original → native → disc)

| Outcome → screen | Original | Native (`src/career.cpp:162` + `src/frontend.cpp:917,246`) | Disc `VIDEO/` |
| --- | --- | --- | --- |
| wrecked → `0x1c` | `Wreck` + `%6+1` | `numbered("WRECK",6)` | WRECK1-6 ✓ |
| busted → `0x1d` | `Busted` + `%6+1` | `numbered("BUSTED",6)` | BUSTED1-6 ✓ |
| lost → `0x1f` | `Lose` + `%10+1` | `numbered("LOSE",10)` | LOSE1-10 ✓ |
| won, not all-qualified → `0x1e` | `Win` + `%6+1` | `numbered("WIN",6)` | WIN1-6 ✓ |
| won, qualified `0x1f`, level<4 → `0x1e` | `Level` + `%6+1` | `numbered("LEVEL",6)` | LEVEL1-6 ✓ |
| won, qualified `0x1f`, level 4 → `0x1e` | `FinalWin` exact | `"FINALWIN"` | FINALWIN ✓ |
| course chosen (`0x004213c0`) | `Start`, 6 (via `0x004023e0`, flag 2) | `queue_video("Start",6)` (`src/frontend.cpp:1000`) | START1-6 ✓ |

Native gating note (not a law difference): `queue_video` skips when `video_mode==0` and requires `assets_.exists(path)`; path case `VIDEO/` vs original `Video/` is asset-lookup level.

## Trigger points (results flow)

1. Setters → `0x00416010`: `0x7f` finish → 2/3 by 0-based place<3 (single player); 4 busted (`0x00407f30`); 5 wrecked (`0x0040a600`/`0x0043a190`/quit `0x00423408`); first-write-wins on `DAT_004c5d7c`, `0xb4`-tick tail (`0x00442e10`).
2. `0x00416120` maps outcome → screen: 2→`0x1e`, 3→`0x1f`, 4→`0x1d`, other→`0x1c` (attract→9, quit→0xb); network branch replays via `0x004210a0` (p3=1 wrecked / 2 busted).
3. Screen entry → `0x00449150(screen)` selects base+variants → `0x004023e0` queues `Video/<BASE><N>.AVI`.
4. Native mirror: `game_main.cpp:381` → `Frontend::race_finished` → `finish_race` (`career.cpp:104`, outcome→screen incl. attract→main_menu) → `results_video` (`career.cpp:162`) → `queue_video` (`frontend.cpp:246`) → `enter(screen)`; level-up/finale routing in `after_results` (`career.cpp:144`); result pictures in `background_for` (`frontend.cpp:321-326`).
5. Existing pins: `tests/frontend_test.cpp:511-539` (pure-rule exact names + end-to-end prefix asserts).

## Verification plan (for owner/integrator; NOT run this turn per no-build scope)

- Normalized-whitespace diff of the two REA pseudocodes vs `analysis/decompiled/00449150.c` / `004023e0.c` (char-count MATCH claim as in `outcomestep-rea-20261012`).
- `ctest -R frontend` (outcome-video block) + full `cmake --build build --parallel` + `ctest` gate at integrate time.
- No replay/pixel claim from REA (spot-check per `docs/TOOLING.md`).

## NOT-changed list (inspected, untouched)

- `src/career.cpp`, `src/frontend.cpp`, `src/career.hpp`, `src/game_main.cpp`, `tests/frontend_test.cpp`
- `docs/REVERSE_ENGINEERING.md`, `docs/TOOLING.md`, `docs/plans/13-music-and-frontend-completion.md`, `docs/CURRENT_STATUS.md`
- `analysis/decompiled/*`, `analysis/oracle/*`, `build/`, `oracle/`, parent `android/`/`prefix/`/`cdrom/`
- No tracked file created/modified/deleted; `git status` delta from this turn = only the 2 new paths above. No commits made.

## Remaining / handoff to integrator

- Selection law is now independently REA-pinned for `0x00449150` (first pin; prior work covered `0x00416010`/`0x00416120`/`0x004210a0`/`0x00421290`); suggest one REVERSE_ENGINEERING.md bullet referencing this handoff's evidence_ids.
- Open (unchanged): DUEL mano-a-mano gating (plan 09), input-exit mechanism of demo race, `0x004214e0`/`0x0041f6d0` Ghidra gaps (project-export ports).

## Live processes / resources

No live REA/Ghidra/Wine processes (both REA calls exited 0; ephemeral backend only, project Ghidra untouched). No build locks, no captures, no held resources.
