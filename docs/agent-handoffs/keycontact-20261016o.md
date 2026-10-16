# Plan 10 Task 6 background — key contact helpers REA spot-check (read-only, 20261016o)

- Task: Plan 10 Task 6 background on the key contact helpers. Separation
  `0x00436a40` already covered; contact `0x004049c0` partially covered;
  keycontact/proximity helpers uncovered — this session spot-checks the two
  uncovered helpers below and tables the cluster against native code.
- Baseline: HEAD `5ffafa9` (`git rev-parse HEAD`). Read-only: no tracked file
  edited, no builds, no Wine captures, no Ghidra writes, no commits.
- EXE: `../cdrom/ROADRASH/ROADRASH.EXE`, sha256
  `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
  (verified via `shasum -a 256` before both REA calls; matches
  `analysis/inventory.json`).
- Sibling eight off-limits (tracked modifications owned by other workers, all
  excluded from `rg`/`fd` and untouched): `src/course_scene.cpp`,
  `src/race_caches.hpp`, `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`,
  `tests/course_scene_test.cpp`, `tests/race_cache_test.cpp`,
  `tests/road_test.cpp`.

## 1. Covered vs uncovered survey (prior handoffs read, not modified)

Covered (pinned REA dossier or disassembly verification — NOT re-fetched):
`0x0044fa80` keymap, `0x004368f0` dispatch, `0x004049c0` whole-file + short
branch, `0x00436a40` separation (+ `+0x144` guard semantics), `0x004373f0`
cull, `0x00437e60` scheduler, `0x004050c0`/`0x004051e0`/`0x004052c0` damage
chain, `0x00409400`/`0x004095a0`/`0x00409510`/`0x00409ad0`/`0x00409bd0`/
`0x00408d50` combat leaves, `0x004096f0` hit resolver, `0x00409c10` reach
(disassembly + orphan dossier), `0x0040a4a0`/`0x00408df0`/`0x00409000`/
`0x004092e0`/`0x00449a80` state helpers, `0x00436830` sound, `0x00439270`
mover, `0x00441750` rand, `0x00440760` ped hit, `0x00441500` lane-clear,
`0x00403d60` crash entry, `0x0040ac70` anim set, remount proximity box
(disassembly `40a785-40a7c9`).

Uncovered (no prior dossier) — the two REA targets of this session:
`0x00407410` (contact-tail finish-distance helper) and `0x00409560`
(steal-anim column predicate).

## 2. REA spot-check (pinned 6.4.0, exit 0 both, ≤2-call budget spent)

Env per `docs/TOOLING.md`: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE <addr> --format md`.

| # | Address | evidence_id | Result |
|---|---|---|---|
| 1 | `0x00407410` (contact-tail `+0x1f8` writer) | `ev_1cfcd4792f0b9817854ace91268f9ef0fd68e5348188ca2797fc15f31943c4dc` | exit 0; body `0x407410-0x40753c` (301 B); REA 1348 chars vs export 1348, TRUE |
| 2 | `0x00409560` (steal-anim column) | `ev_5d47d80d7b9865187e1832b63b6f78505b5902e7f48987806bc1d9f8e52d10eb` | exit 0; body `0x409560-0x409598` (57 B); REA 308 chars vs export 308, TRUE |

Raw outputs (scratch only, disc-derived, never commit):
`analysis/keycontact-20261016o/rea-407410-tail.md(.log)`,
`analysis/keycontact-20261016o/rea-409560-atkpred.md(.log)`,
`analysis/keycontact-20261016o/notes.md`.

Semantics confirmed (static only):
- `0x00409560(bike)`: weapon `*(half+0x1d4)` selects row; returns 1 iff anim
  code `*(half+0x80)` is in the right-side tables `0x004644f8`/`0x00464508`
  (contrast `0x00409510`, which checks all four side tables). Sole caller is
  the `0x004096f0` steal-success path at `0x40989c` (disassembly-verified):
  the return is the column index into the `0x00464538` attacker-anim table,
  then `0x0040ac70(code)` + `0x0040aca0(_,2)`; victim anim 2, queue 2.
  No RNG, no callees.
- `0x00407410(half)`: writes rider-half `+0x1f8` (distance-to-finish walk:
  `DAT_004b6e58 - +0x1c` fast path, else route-node walk accumulating
  `+0x2c`-scaled words; `0x7fffffff` on malformed graph), then fires the
  `0x0043ca90` HUD hook under the owner gates (`+0x324 == 0`, or flag-1 with
  matching `+0x328` char). Callees `0x00432af0`, `0x0043ca90`. Export-grep
  callers: contact tail `0x004049c0:268` (guarded by `+0x2f4 == 0`), remount
  `0x00404560:38`, `0x00404480:30`, `0x004046b0:109`, bike build
  `0x00405c80:36`, crash entry `0x00403d60:135`, `0x004040d0:127`,
  `0x004060b0:41,190`. (REA caller table lists only `0x00405c80` —
  thunk-mediated calls missed, partial; exports/disassembly authoritative.)

REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or
pixel claim is made from it.

## 3. Helper table (native vs original, all read via rg/read on HEAD)

| Original helper | Native counterpart | Verdict |
|---|---|---|
| `0x004368f0` dispatch (outer `+0x10==tick` + `c0&6==6`, candidate `c0&5==5`, then geometry) | `Race::collide_riders` (`race.cpp:2235`), `collide` (`:513`), `collide_traffic` (`:526`), `collide_hazards` (`:664`), `collide_pedestrians`/`contact_rider_pedestrian` (`race_pedestrians.cpp:423,440`); `ScanObject` (`race.hpp:366-371`) carries no flags byte | GAP (known): no `+0xc0` role gates by construction; geometry/c0 asymmetry selects the 893 pair (see contact893 handoff) |
| `0x00436a40` separation | `Race::separate` (`race.cpp:370-511`) | ported line-for-line; GAP (known): no `+0x144` early-return guard, no `+0x1a0` partner links |
| `0x004049c0` bike contact, sounds/class/elasticity/nudge/kick | `Race::contact` (`race.cpp:947-1029`, bike-bike), `contact_car` (`:828`), `contact_pedestrian` (`race_pedestrians.cpp:377`), `contact_hazard` (`race.cpp:688`) | ported; GAP (this session, static): bike-bike tail (`:1027-1028`) lacks the export tail `004049c0.c:248-266` — `+0xf4` clamp `0x600`, speed clamps `[−1000,30000]`, `±10000` clamps — and the `+0x2f4==0 → 0x00407410` call (`:267-269`). (Car path `:903-906` has its own clamps.) |
| `0x00407410` (`+0x1f8` writer + `0x0043ca90` hook) | none in sim: no native writer of bike `+0x1f8` exists (only `hud.cpp:83` approximates `finish − +0x1c` for nearest-opponent) | GAP (this session, static): unported helper; HUD-only approximation |
| `0x004052c0`/`0x004050c0`/`0x004051e0` knock-off chain | `Race::knock_off` (`race.cpp:754-819`) incl. reason scaling, owner bypass, crash vectors, reason-4 anim 9, reason-5 steer path | match (comment-verified against assembly widths) |
| `0x00409c10` reach | `Race::fight_reach` (`race.cpp:2000-2006`) | match (side strictness, `0x40`/`reach*0x1000`/`0x3000` bounds) |
| `0x004096f0` hit resolver incl. steal | `Race::resolve_hit` (`race.cpp:2008-2072`) | match except GAP (this session, static): steal path (`:2039-2048`) performs the atomic `+0x1c8`/`+0x1d4` transfer + `tick+300` immunity but sets NO `0x00464538` attacker-anim / victim-anim codes — the `0x00409560` column lookup and `0x0040ac70/0x0040aca0` calls are absent |
| `0x00409510` attack predicate (4 tables) | `Race::ai_attack_anim` (`race.cpp:1442-1444`, timer-only) | APPROX (known): no table check; `0x00409560` (2 right-side tables) likewise unported |

## 4. 893-relevance note

- The 893 pair (player vs frozen node4) runs the bike-bike branch, not
  combat: `0x00409560` cannot fire there (sole caller is the `0x004096f0`
  steal path). Its relevance is to the `napa-fight` scenario (Task 6 fix
  order step 6), where every successful steal currently skips both anim-code
  writes — a static gap, not a divergence claim.
- `0x00407410` IS on the 893 path: if the original takes the candidate
  branch for player pos30093 vs node4 pos30208 without a crash (`+0x2f4==0`),
  the contact tail recomputes the rider-half `+0x1f8` and may fire the
  `0x0043ca90` HUD hook. `+0x1f8` is not a compared sim field today and the
  hook is HUD-side, so this cannot explain the tick-893 RNG divergence (2
  extra native sound draws); it matters for HUD/finish-distance fidelity
  after contact, and for any future field that consumes `+0x1f8`.
- The missing bike-bike tail clamps (§3, `004049c0.c:248-266`) are also on
  the 893 path: if the 893 contact survives without crash, native speed/
  `+0xf4` keep unclamped values where the original clamps. Static observation
  only — the 893 branch-taken question itself still needs the per-tick
  `+0xc0`/`+0x10` capture specified in `contact-c0-20261016b.md:§8`.
- No new divergence mechanism is claimed: current HEAD still stands at the
  tick-735 `brain +0x194` gap shadowing 893 (see `contact893-rea-20261016.md`
  §"Replay diagnostics").

## 5. NOT-changed list (explicit)

- No tracked file edited — in particular untouched: the sibling eight
  (`src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
  `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp`) plus `src/race.cpp`,
  `src/race.hpp`, `src/bike.*`, `src/traffic.*`, `src/terrain.*`,
  `src/race_setup.*`, `src/hud.*`, `src/pedestrians.*`,
  `src/race_pedestrians.cpp`, `oracle/aweman_proxy.c`, `CMakeLists.txt`,
  `tests/oracle_sim_test.cpp`, all shared docs and plans.
- No builds, no test runs, no Wine captures, no prefix/DLL changes, no Ghidra
  project writes, no golden/allowlist edits, no reanchor, no commits.
- New files only: this handoff + untracked scratch
  `analysis/keycontact-20261016o/` (`rea-407410-tail.md/log`,
  `rea-409560-atkpred.md/log`, `notes.md`).

## 6. Remaining work (owner / integrator)

1. Porter decides the §3 gaps (`004049c0.c:248-269` tail in bike-bike
   `contact`, steal anim codes via `0x00409560` column): both are additive
   and testable with synthetic fixtures (clamp bounds; steal conservation +
   anim codes), no capture needed for the port itself.
2. 893 branch-taken question still needs the per-tick `+0xc0`/`+0x10`/
   `+0x144`/`+0x1a0` capture (owner: proxy/test); re-clear tick 735 first.
3. Integrator: link from the Plan 10 Task 6 index on next refresh; keep
   `analysis/keycontact-20261016o/` local (disc-derived REA dumps, never
   commit).

## Live processes / resources

No live processes (both `npx` calls exited 0 and returned). No build locks,
no worktrees, no LSP/database writes, no shared resources held.
