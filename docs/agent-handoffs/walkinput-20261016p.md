# walkinput-20261016p — Plan 24 background (READ-ONLY)

Task: background on on-foot walk input/steer — `0x0040a600` auto-walk, walksteer
gate, walk-to-cam, walk-enter; run-back under player control, bounds wreck,
on-foot contact. The direct roots were all pinned by prior turns (see coverage
below), so the 2-call budget went to two UNCOVERED neighbours in the same
paths: walker init `0x00452d70` (dual-called by walkroot `0x00452f50`) and
slice-list rebuild `0x00450f90` (in the `0x00454120` advance chain with
walk-to-camera `0x00453ae0`).

- Baseline: HEAD `e309e0a` at session start; tree carries sibling sessions'
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
| 1 | `0x00452d70` (walker init, walkroot leaf) | `ev_f15ee8bf63f0cd59ae691a1115349807d31e70de02fd1c902d8e7e5e9bc67a5c` | exit 0; export 459 norm-chars contained in REA pseudocode (match) |
| 2 | `0x00450f90` (slice-list rebuild, advance chain) | `ev_4fe440a3ea4ef714dd87a52021f4d6619861a1d3b846bcbb751001179c0cda3a` | exit 0; export 313 norm-chars contained in REA pseudocode (match) |

Raw outputs + branch notes (scratch only, never commit disc-derived dumps):
`analysis/walkinput-20261016p/rea-452d70-walkinit.md(.log)`,
`analysis/walkinput-20261016p/rea-450f90-slicefree.md(.log)`,
`analysis/walkinput-20261016p/notes.md`.

Prior coverage (not re-spent — all TRUE vs `analysis/decompiled/` unless noted):
`0x0040a600`/`0x00403d60`/`0x0043a610` (crashseq), `0x00404560`/`0x004067d0`/
`0x0040a9d0` (remount), `0x004046b0` (onfoot), `0x00440410` (netpkt),
`0x0040ac70`/`0x0040aca0`/`0x0043ae90`/`0x0043ae10` (anim), `0x0044f5a0` (atan2),
`0x00449c20` (schedwalk), `0x00453ae0`/`0x00452e10` (walktocam),
`0x00452f50`/`0x00452cb0`/`0x00450fc0` (walkerenter), `0x00437c30` (camera),
`0x00416010` (outcome), `0x0044f800`/`0x0044fa80` (controls/keymap),
`0x00407410` tail (keycontact-20261016o, HEAD). Bounds-wreck, walksteer-gate,
timer/police-owner and remount-exit regions verified by disassembly (walksteer
handoff). `0x0040a9e0` remains the only export-bearing walk function with no
REA procedure entry (network-only; port from parent + export).

New semantics for the porter:
- `0x00452d70`: zeroes words 1–3, runs eight feature inits with 0
  (`0x4528c0/0x451bf0/0x4522d0/0x4519b0/0x452660/0x452ad0/0x4511f0/0x4520a0`),
  then conditional `0x00452cb0(walker, param_2, 1)`. Sole caller `0x00452f50`
  (refs `0x452fa2`/`0x452fb2` = front/back). No RNG in body. Native
  `TrackRing::enter` (`track_ring.cpp:112-122`) covers only the `0x00452cb0`
  tail — the 8-init prologue has no native equivalent yet.
- `0x00450f90`: walks `DAT_004b6e60[param_1]` list, `0x00450dc0` per node.
  Pure teardown in the `0x00454120` advance chain. No RNG. No native
  equivalent (native slices are value objects, no free-list) — correctly absent.

REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay/pixel
claim is made from it.

## Input map (original → native, read-only `rg`)

| Original bit | Key (via `0x00465fb9`/`0x00476c00`) | Native control | Walk effect (`0x0040a600` anim-38) |
| --- | --- | --- | --- |
| `0x800` | accelerate (codes `0x94`) | `control_accelerate` (`race.cpp:1180`) | `toward=+1` if bike ahead (`long>=1`), else 0 |
| `0x1000` | brake (`0x96`) | `control_brake` (`:1181`) | `toward=-1` if bike behind (`long<0`), else 0 |
| `0x4000` | left (`0x93`) | `control_left` (`:1182`) | `side=-256` |
| `0x2000` | right (`0x95`) | `control_right` (`:1183`) | `side=(0x2000>>5)=+256` |

Gate chain (verbatim order, `0040a600.c:78-79`): `DAT_004b7ed0 & 0x7800` nonzero
→ player-bike identity (`DAT_004642d8 == param_1`) → demo-hold clear
(`DAT_004753c8 == 0`) → racing (`DAT_004c5d7c == 1`). Any gate fails = auto-walk.
Passing zeroes the `+0x138[state]` elapsed word (timer reset). Network twin
`0x0040a9e0` substitutes `uVar5 = 0x00440410(param_1)` for `DAT_004b7ed0` with
no identity/demo/racing gates. Native `controls_from` (`input.cpp:36-46`) +
`step_crash` gate (`race.cpp:1179`, `if(player)`) reproduce the player side;
touch/gamepad map through the same four `control_*` bits (plan Task 3 Step 3).

Auto-walk (no bits): `toward=longitudinal`, `side=lateral−6000`,
`angle = 0x0044f5a0(toward, side/256)`, heading `+0x128`/`+0x5c`, speed
`+0xec = 1200` (`0x0040a9d0`). Proximity box (`0040a600.c:60-75`):
`|long|<=9 (+9/128 rounding)`, `|lat|<0x1771 (6001)` → stop, remount if
`+0x2f0 != 0` (lateral `+6000` snap, anim `)`/`0x02`, state 3) else stand (`%`).
Native: `recover_crash_actor` (`crash.cpp:44-65`) — match incl. `0x7800` mask.

## Recovery semantics (porter checklist)

- 1200-tick timer: `+0x138[state] > 0x4b0` → over-time path; live cop owner
  (`DAT_004642dc != 0` and its `+0x2fc` == walking half) skips via `40a6d5`.
  Native `crash.cpp:38` (`state_elapsed>1200 && !police_owner`) — match; the
  `CrashRecoveryInput.police_owner` feed is the porter's wiring point.
- Wreck: timer expired + `+0x2f0 == 0` + player bike → `0x00416010(5,0xb4)`
  (anim-`&` path and anim-`%`+loops path, `crash.cpp:41,74`). Native returns
  `wrecked` → `+0x2f0 = 0` (`race.cpp:1191`) — match.
- Bounds wreck: player + slice type 1 + bike/rider lateral `>>8` outside
  `[+0x14c,+0x158]` → `0x00416010(5,0xb4)`, `+0x2f0 = 0`, return
  (`0040a600.c:40-56`). NOT in native `recover_crash_actor` — open (Task 3 Step 2).
- On-foot contact `0x004046b0` (type-8 half): thud `0x00436830(4)`,
  `+0x17c·2` vertical, speed-scaled `+0xf4` shove (0x150 clamp), `+0xec`
  quarter-bleed, `0x00407410` re-anchor when crashed. NOT in native
  `collide_traffic` — open (Task 3 Step 2).
- Remount exit: state 3 + anim-`)` frame gate → `0x00404560` (`0040a600.c:127-129`).
  Native `race.cpp:1187-1190` (`remount` → `remount_crash_actor`, stamina restore)
  — match.
- Camera while on foot: `0x00437c30` follows player bike `+0x138` (confirmed) —
  confirm against capture (Task 3 Step 3).

## Capture spec (Plan 24 Task 3 Step 1, for the owner — NOT run this session)

Three original runs, Napa level 1: crash into the first oncoming car
(11a car-contact scenario controls), then (a) release every key, (b) hold
accelerate, (c) hold left. Dump per tick: rider half `+0x1c/+0x28/+0x128`/
`+0xec/+0x80` and bike `+0x2f4/+0x2f0`. Expected: (a) auto-walk → remount,
(b) straight-ahead walk only while bike ahead, no auto-remount while held,
(c) left walk. `DAT_004b7ed0` control word comes from the per-frame poll
(`0x00442ea8`), reused across multi-step frames — record at the step
(`0x0044b324` hook), not at the poll. Requires a valid header, contiguous
ticks and completion marker; coordinate prefix/DLL/output ownership first.

## NOT-changed list (explicit)

- Sibling eight render/road files (live-modified, excluded from all searches):
  `src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
  `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp` — not read, not edited.
- Other tracked sources/tests/plans/shared docs (`src/crash.*`, `src/race.cpp`,
  `src/input.cpp`, `src/track_ring.*`, `tests/crash_test.cpp`,
  `docs/REVERSE_ENGINEERING.md`, `docs/plans/24-road-hazards-police-crash-control.md`)
  — read-only (`read`/`grep` only, eight excluded), none modified.
- No builds, no test runs, no captures, no Ghidra project writes, no prefix/DLL changes.
- Prior handoffs read (not modified): `walkinput/walksteer/walktocam/
  walkerenter/onfoot/schedwalk-rea-20261012.md`, `ped-helpers-20261016n.md`,
  `keycontact-20261016o.md`.
- New/untracked only: this handoff + `analysis/walkinput-20261016p/` (2 REA mds + logs + `notes.md`).
- Remaining work (owner/integrator): Task 3 Steps 1–4 open (capture trilogy →
  port bounds-wreck + police-owner feed + `0x00404560`-on-state-3 already native +
  racing/demo gates + `0x004046b0` on-foot contact → `race_test` bounds ±1 pins);
  8-init prologue of `0x00452d70` vs `TrackRing::enter` is porter input.

## Live processes / resources

No live REA/Ghidra/Wine processes (both `npx` calls exited 0 and returned).
No build locks taken, no worktrees created, no LSP/database writes, no shared
resources held.
