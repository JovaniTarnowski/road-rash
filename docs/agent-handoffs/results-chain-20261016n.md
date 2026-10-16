# results-chain-20261016n — Plan 15/13 background: finish → 0xb4 tail → standings/prize/fines/repairs → level-up/finale

Task: READ-ONLY background on the results chain (finish decided → `0xb4` ticks →
standings/prize/fines/repairs/level-up/finale). Spot-check only; no builds/captures,
no commits. Baseline `d82c1b1` (verified via `git rev-parse HEAD` at turn start).

Worktree: `/Users/archit/Dev/rr/native-port` (shared tree; sibling uncommitted
render/road work present, none touched or read).

## Owned / changed files (new paths only, no tracked-file edits)

- `docs/agent-handoffs/results-chain-20261016n.md` (this file)
- `analysis/results-20261016n/` — `rea-442e10-tail.md/.log`,
  `rea-41f6d0-after.md/.log`, `chain.md`

## Evidence (2 REA calls — budget maxed, both `--format md`)

Pinned: `rea-agents@6.4.0`, `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`.
Reference EXE sha256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(REA-reported digest matches; locally re-verified via `shasum -a 256`).

| Addr | Role | evidence_id | Outcome |
| --- | --- | --- | --- |
| `0x00442e10` | race-loop update incl. `0xb4`-tick tail countdown | `ev_7afd57b4135ed74ce05c157c6d140aa30fca37571c7379afc5ae9a04b31352e6` | MATCH: REA pseudocode normalised-whitespace-identical to `analysis/decompiled/00442e10.c` (913/913 chars) |
| `0x0041f6d0` | after-results routing (level-up / finale) | (gap, no evidence_id; EXIT=1) | REA procedure-database gap reproduced: `No procedure exists at the requested address: 0x41f6d0` — same gap as the October-12 `rea-41f6d0-prize.md` finding (port from the project export) |

REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay/pixel
claim is made from it.

## Chain (original → native)

1. Setters → `0x00416010(outcome, ticks)` (`analysis/decompiled/00416010.c`):
   `0x7f` finish → 2/3 by 0-based place<3 single-player (`DAT_004c5b00`/`DAT_004c5af9`
   clear path); 4 busted (`0x00407f30`); 5 wrecked (`0x0040a600`/`0x0043a190`/quit
   `0x00423408`); first-write-wins on `DAT_004c5d7c` (`DAT_004753c4`/`DAT_004753b4`
   guards), param_2 seeds the tail counter (`0xb4` = 180), name copy when
   `DAT_004c5af9`. REA-pinned October 12 (`outcomestep-rea-20261012`).
2. `0x00442e10(steps)` per frame (`analysis/decompiled/00442e10.c`; REA MATCH this
   turn, see above): while `DAT_004753c4 > 0` and `0x0042fbd0` true, decrement by
   param_1 and set `DAT_004753b4 = 1` at expiry — the race keeps running 0xb4 ticks
   after the outcome is decided. Then: pump messages (`0x12` → done), poll controls
   `0x0044f800` → `DAT_004b7ed0`, network/cop anim `0x004403a0`, `0x00434980`;
   arrest check `0x00431870` → done unless tick `DAT_004b7ecc > 299` (`0x12c`);
   fade timer `DAT_004753c0` vs batch stepper `0x0044b2e0(steps)`. Returns done flag.
3. `0x00416120` maps outcome → screen: quit/abort → `0xb`, attract → 9, 2 → `0x1e`
   won, 3 → `0x1f` lost, 4 → `0x1d` busted (network replay `0x004210a0` p3=2),
   other → `0x1c` wrecked (replay p3=1); saves registry, writes standings place
   `DAT_004b8a14._2_1_`. REA-pinned October 12 (`resultschain-rea-20261012`).
4. `0x00421290` builds the standings screen, returns `0x22`: rows from `0x0046ab48`
   (stride `0x4c`), highlight clamp, Big-Game payout
   `DAT_004b8a18 += prize[place] * (level+1)`, `InvalidateRect`. REA-pinned
   October 12.
5. `0x004214e0` advances to `0x21` (`analysis/decompiled/004214e0.c`; REA gap #9,
   port from project export): Big Game busted (`param+8 == 0xc3`) → fine
   `(level*5+5)*0x50`; else repairs `bike_price/10`; `sprintf` via `0x0041c140`;
   pay if `penalty <= cash` (flag 1) else flag `0xc6` + `0x0041f630` restart. Thrash:
   clear qualified, message `0xc1`/`0xc4`. Mano: delegate `0x00421290`.
6. `0x0041f6d0` after-results (`analysis/decompiled/0041f6d0.c`; REA gap
   reproduced this turn): qualified `!= 0x1f` → mode routing (0 → 9, 1 → 10,
   2 → `0xf`/`0xe`, 3 → `0xb` via `0x00430140`); all-qualified → save-pending,
   clear, level<4 → `0x20` level-up else `0x23` finale.
7. Native mirror (inspected, untouched): `run_game` (`src/game_main.cpp:363-381`)
   → `run_race` → `RaceEnd` → `fe.race_finished(end.outcome, end.place, standings)`
   → `finish_race` (`src/career.cpp:104`, outcome→screen incl. attract→main_menu,
   qualify-bit, prize/fine branches) → `results_video` (`src/career.cpp:162`) →
   `queue_video` → `enter(screen)` (`src/frontend.cpp:917-924`); handlers route
   `0x004214e0 → 0x21`, `0x00421290 → 0x22`, `0x0041f6d0`/`0x004241e0` →
   `after_results` (`src/frontend.cpp:1055-1057`); standings rows formatted in
   `format_standing` (`src/frontend.cpp:904-915`); level-up/finale in
   `after_results` (`src/career.cpp:144-160`). Existing pins:
   `tests/career_test.cpp:32-80` (prize/fine/repair/level/finale/thrash/save),
   `tests/frontend_test.cpp:180-192,267,524-539` (standings rows, outcomes, video
   prefixes).
8. Native gaps noted (not changed): `Race::step_rider` coasts finished riders
   (`src/race.cpp:1820`) but no 180-tick `DAT_004753c4`-style countdown exists in
   `src/race.cpp`; the `RaceEnd` outcome/place/quit mapping lives behind the
   off-limits `src/race_screen.*` (not inspected this turn). Busted/wrecked
   setters exist (`police_arrest` in `src/police.cpp:70`, `CrashRecoveryAction::wrecked`
   in `src/crash.cpp:41,74`) but the end-to-end in-race → `0x00416010`-equivalent
   wiring was not traced this turn.

## Prize / fine formulas

- Prize table `0x0046ab48` (first dword of each `0x4c` standings row, 15 places):
  `1000, 750, 500, 400, 300, 250, 200, 160, 130, 100, 70, 50, 30, 20, 10`.
  Big Game pays every finisher (won or lost): `prize[place] × (level+1)`
  (`src/career.cpp:122`; mirrors `0x00421290` line 52-53).
- Busted fine (Big Game): `(level*5+5)*0x50` = `400 × (level+1)` → L0 400 …
  L4 2000 (`src/career.cpp:128`; mirrors `0x004214e0` line 15).
- Wrecked repairs (Big Game): `bike_price[bike] / 10` (`src/career.cpp:128`;
  mirrors `0x004214e0` line 21).
- Paid iff `penalty <= cash` (messages 197 busted / 194 wrecked); otherwise message
  198 ("outta money") + `0x0041f630` career restart (`src/career.cpp:129-136`).
- Thrash: no money; wrecked/busted clears qualifications (messages 193/196)
  (`src/career.cpp:137-140`).
- Corpus pin (existing test, not run this turn): `tests/career_test.cpp:110-114`
  asserts `prize[0]==1000, prize[1]==750, prize[14]==10` against the disc EXE.

## Verification plan (for owner/integrator; NOT run this turn per no-build scope)

- Normalised-whitespace diff of the new REA pseudocode vs
  `analysis/decompiled/00442e10.c` (913/913 MATCH claimed above; recompute with
  the `python3` one-liner in this handoff's scratch notes).
- `ctest -R 'career|frontend'` + full `cmake --build build --parallel` + `ctest`
  gate at integrate time.
- No replay/pixel claim from REA (spot-check per `docs/TOOLING.md`).

## NOT-changed list (inspected or deliberately untouched)

- Sibling eight (live work, neither read nor touched): `src/course_scene.cpp`,
  `src/race_caches.hpp`, `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`,
  `tests/course_scene_test.cpp`, `tests/race_cache_test.cpp`, `tests/road_test.cpp`
- `src/career.cpp`, `src/career.hpp`, `src/frontend.cpp`, `src/frontend.hpp`,
  `src/game_main.cpp`, `src/game_types.hpp`, `src/game_types.cpp`,
  `src/race.cpp`, `src/race.hpp`, `src/race_actions.cpp`, `src/race_helpers.cpp`,
  `src/police.cpp`, `src/crash.cpp`
- `tests/career_test.cpp`, `tests/frontend_test.cpp`, `tests/race_actions_test.cpp`
- `docs/REVERSE_ENGINEERING.md`, `docs/TOOLING.md`,
  `docs/plans/13-music-and-frontend-completion.md`,
  `docs/plans/15-release-and-acceptance.md`, `docs/CURRENT_STATUS.md`
- `analysis/decompiled/*` (read-only reference), `analysis/outcome-20261016j/`,
  `analysis/oracle/`, `build*/`, `oracle/`, parent `android/`/`prefix/`/`cdrom/`
- No tracked file created/modified/deleted; `git status` delta from this turn =
  only the 2 new paths above. No commits made.

## Remaining / handoff to integrator

- The `0xb4`-tick tail (`0x00442e10`) is now independently REA-pinned (first pin;
  prior work covered `0x00416010`/`0x00416120`/`0x00421290`/`0x00449150`/
  `0x004023e0`/`0x004210a0`/`0x00402f50`); suggest one `REVERSE_ENGINEERING.md`
  bullet referencing this handoff's `ev_7afd…1352e6` for the tail countdown.
- `0x0041f6d0` remains a REA procedure-database gap (reproduced twice now);
  the port rests on `analysis/decompiled/0041f6d0.c` as before.
- Open (unchanged): native 180-tick post-finish tail (no `DAT_004753c4`
  equivalent in `Race`); `RaceEnd` mapping behind off-limits `race_screen.*`;
  `0x004214e0` gap #9; DUEL mano-a-mano gating (plan 09); original save
  round-trip (plan 13 Task 6).

## Live processes / resources

No live REA/Ghidra/Wine processes (call 1 exited 0, call 2 exited 1 with the
documented gap; ephemeral backend only, project Ghidra untouched). No build
locks, no captures, no held resources.
