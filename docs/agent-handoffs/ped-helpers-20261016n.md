# ped-helpers-20261016n — Plan 24 Task 4 background (READ-ONLY)

Task: background on ped helpers — spawn ring build `0x004520d0`, think
`0x00440dd0`, cross-check `0x00441500`, hesitate/recover/turn-back
`0x004414a0`. All four were already REA-pinned by prior turns (see below),
so the 2-call budget went to two UNCOVERED helpers in the same paths:
voice-word `0x00440730` and remote ped-state writer `0x004416a0`
(`0x00440760`/`0x004049c0`/`0x00441500` already covered — not re-fetched).

- Baseline: HEAD `d82c1b1` at session start; tree carries sibling sessions'
  uncommitted edits (8 modified render/road files + untracked scratch) — none touched.
- Worktree: `/Users/archit/Dev/rr/native-port`. No owned build dir; no builds, no captures.
- Mode: READ-ONLY. No tracked file edited. No commits.

## Evidence (pinned REA, 2/2 allowed calls, both exit 0)

Env per `docs/TOOLING.md`: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=…/openjdk@21`, `npx --yes rea-agents@6.4.0 function <exe> <addr> --format md`.
EXE sha256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (matches pin).

| # | Address | evidence_id | Result |
| --- | --- | --- | --- |
| 1 | `0x00440730` (voice word byte-swap) | `ev_aa17abca43924d26300d4a0cfa5a501adecc656d390cc92298f2f9db94468d6a` | exit 0; export 382 norm-chars contained in REA pseudocode (match) |
| 2 | `0x004416a0` (remote ped-state writer) | `ev_5efa6d4f72919b55ef708daa17f9c1e60032ed1f24e120052fcdddbbe13292dc` | exit 0; export 510 norm-chars contained in REA pseudocode (match) |

Raw outputs + branch notes (scratch only, never commit disc-derived dumps):
`analysis/ped-helpers-20261016n/rea-440730-word.md(.log)`,
`analysis/ped-helpers-20261016n/rea-4416a0-pedstate.md(.log)`,
`analysis/ped-helpers-20261016n/notes.md`.

Prior coverage (not re-spent — all TRUE vs `analysis/decompiled/`):
`0x004520d0`/`0x004409e0` (dzhrspawn), `0x00440dd0`/`0x00440990`/`0x00440680`
(pedchain), `0x00440760`/`0x00440b80`/`0x00441500` (pedcontact) + 20261016f
re-pin of `0x00440760`/`0x004049c0`, `0x004415b0`/`0x004414a0`/`0x0044a020`/
`0x004498c0` (pedhelpers).

REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay/pixel
claim is made from it.

## Helper table (original → native, read-only `rg`)

| Helper | Original | Native | Verdict |
| --- | --- | --- | --- |
| Spawn ring build `0x004520d0` | one `rand()` per actor record; `\|= 0x3f00` consume; lateral `(slice+0xcc+sign12)·0x100 − 2s + rand%(4s+1)` | `build_pedestrian_slices` (`race_pedestrians.cpp:32-54`) + `spawn_pedestrian` (`:75-128`); caller draws `random()` first (`:47`) | match (prior turns) |
| Cop branch `0x004409e0` ≥ 0xb | two `rand()%0x81` + `0x00406b80` | draws kept (`:91`), construction returns (`:92`) | stream held for plan 24 Task 2 |
| Think `0x00440dd0` states | 0 wait / 1 cross / 3 hesitate / 4-5 recover / 7 perform; type-10 anim 6 | `think_pedestrian` (`:220-326`) incl. `LAB_004413ea` rest, signal gate, `(rand&3)/(rand&7)` draws | match (prior turns) |
| Cross-check `0x00441500` | `0x0044a070` nearest + `&~0xf0` formula, factors 0xff/0x80/1, recheck `(f>>2)+0x10` | `pedestrian_road_clear` (`:161-201`) | match (20261016f) |
| Arrive/turn-back `0x004414a0` | shoulder-midpoint ×0x100 vs lateral; re-arm trigger player+0xa0, side flip | `pedestrian_arrived` (`:203-210`) + re-arm (`:271-274`) | match (prior turns) |
| Voice word `0x00440730` **(new pin)** | pure byte-rotate, 1 block, no RNG; callers hit + think, +3 more sites (`0x44081b/0x4408fc/0x441147`) | folded to load time (`course_scene.cpp:169` byte-swap; `:212-218` passes int through) | match |
| Remote ped-state `0x004416a0` **(new pin)** | packet → `+0x1c/+0x28/+0x1e4/+0x1e8/+0x1f8/+0x1ec/+0xec` after `0x00441660` scan; sole caller `0x0043e9c0` net dispatch | no equivalent (network-only path) | correctly absent |
| Pool lifecycle `0x00440640/60/30` (export-read) | `0x00438b70(0x468,0xf)` alloc / `0x00438bd0` free / sound-side init | `vector<Pedestrian>(15)` + `reset()` (`pedestrians.cpp:14-18`) | structural match |
| Hit `0x00440760` / contact `0x004049c0` | knock-down `\|v\|>1000`, scream gates, nudge/ride-over | `hit_pedestrian` (`:343-375`), `contact_pedestrian` (`:377-421`) | match + 2 labelled APPROX (20261016f: walking-hitter carve-out, lateral-only car-vs-ped) |

New observations for the porter: `0x00440ddb` is the same think body as
`0x00440dd0` under a second entry convention (diff = entry guard + naming
only) — one native body covers both. `0x00440730`'s three extra call sites
(`0x44081b/0x4408fc/0x441147`) are outside ped hit/think and untraced
(likely AI/rider speech; not ped Task-4 scope).

## NOT-changed list (explicit)

- Sibling eight render/road files (live-modified, excluded from all searches):
  `src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
  `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp` — not read, not edited.
- Other tracked sources/tests/plans/shared docs (`src/race_pedestrians.cpp`,
  `src/pedestrians.*`, `tests/pedestrian_test.cpp`, `docs/PROGRESS.md`,
  `docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`,
  `docs/plans/24-road-hazards-police-crash-control.md`) — read-only (`read`/`grep`
  only, eight excluded), none modified.
- No builds, no test runs, no captures, no Ghidra project writes, no prefix/DLL changes.
- Prior handoffs read (not modified): `dzhrspawn/pedchain/pedcontact/pedhelpers-rea-20261012.md`,
  `ped-contact-20261016f.md`, `spawn-chain-20261016k.md`, `police-spawn-20261016d.md`.
- New/untracked only: this handoff + `analysis/ped-helpers-20261016n/` (2 REA mds + logs + `notes.md`).
- Remaining work (owner/integrator): Task-4 Step-5 acceptance still open
  (`city-pedestrians`/`city-pedestrian-hit` goldens + pixel frame); two
  labelled APPROX in `race_pedestrians.cpp` (`:349` walking-hitter, `:448-456`
  car-vs-ped separation) await the porter.

## Live processes / resources

No live REA/Ghidra/Wine processes (both `npx` calls exited 0 and returned).
No build locks taken, no worktrees created, no LSP/database writes, no shared
resources held.
