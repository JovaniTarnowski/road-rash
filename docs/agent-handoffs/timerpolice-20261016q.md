# Timer+police verify — 20261016q (READ-ONLY background)

Task: verify countdown traffic timers, arrest delays 180/0, police spawn-due periodic
branch at HEAD `940f4d7`. **No tracked files changed; no commits; no builds/captures.**
Scratch: `analysis/timerpolice-20261016q/` (`summary.md`, two `rea-*.md`/`.log` pairs).

## Evidence (spot-check only, max 2 REA calls)

- Pinned REA `rea-agents@6.4.0` / Ghidra 12.1.4, env per `docs/TOOLING.md`
  (`PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
  `GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
  `JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`), EXE sha256
  `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
  (matches `analysis/inventory.json`, verified with `shasum -a 256`).
- 2 calls, `--format md`, both exit 0, both whitespace-identical to
  `analysis/decompiled/` (newline-only deltas, same pattern as prior handoffs).
  Both addrs are UNCOVERED (not REA'd by the handoffs read below):
  - `0x00416010` outcome/delay setter — extracted 1326 vs export 1329 chars, TRUE,
    `evidence_id ev_a1cfcf897c7a9b44a49c93b9cd0ae8cfce0e405b4218c72cb87ba046676f0525`.
  - `0x0040a600` crash/police-owner machine — extracted 4464 vs export 4467 chars, TRUE,
    `evidence_id ev_92615e29ad835b7b8c1287640cd9314db301dd34b146c38ff0c7519428cd6179`.
- Prior evidence cited, not re-run: `0x0044dd90` (traffic-timer-trace-20261016,
  structural agreement + TOOLING.md pseudocode-identity), `0x0044b3b0`
  (timer-independent-20261015, whitespace-identical), `0x004409e0` + `0x00407860`
  (+ `0x00406b80`/`0x00407f30` triple-confirm) in police-spawn-20261016d.
- Native read (rg/fd, eight sibling-modified files excluded from search scope):
  `src/police.cpp:70-77`, `src/police.hpp:37-45`, `src/traffic.cpp:87-102,508-577`,
  `tests/police_test.cpp:13-24,52-61`, `tests/traffic_test.cpp:141-207`,
  `src/crash.cpp:38` (1200-tick exemption note), `src/career.cpp:104-139,166`,
  `src/frontend.cpp:917`. Exports read fully: `00407f30.c` (175 lines),
  `00416010.c` (53), `0040a600.c` (132), `00442e10.c` (52).

## Timer table (countdown traffic timers, `0x0044dd90` via `src/traffic.cpp:530-577`)

Solo init via `0x0044db80 → 0x0044dd00` (`traffic_configure`, `traffic.cpp:96-102`):
`cruise = 25`, `rate = 2500`, `oncoming_reload = other_reload = div256(2500·6·0x3c) = 3515`,
`oncoming = 3515`, `behind = 0`, `ahead = -3515`, `density = 6`.
`div256` is the signed round-toward-zero `>>8` (`traffic.cpp:14`,
`traffic_lane_count :104-106` char-truncates lane counts, gate `> 0`).
Countdown hold calls `countdown_step → step_timers(..., distance 0)` once per tick
(`traffic.cpp:508-514`); `0x0044b3b0` calls `0x0044dd90` unconditionally every tick
(timer-independent-20261015). 59 holds bank `9 = div256(2500)` per tick
(`tests/traffic_test.cpp:151-154`).

| Timer (word) | Per-tick update (distance `d`) | Fire test | Tap / lane word | Draw on fire (gate>0) | Reset / clamp |
| --- | --- | --- | --- | --- | --- |
| Oncoming `0x004b7e90` (`timers_.oncoming`) | `-= div256(rate) + d` (`:539`) | `< 0` (`:540`) | front tap `(DAT_004b6e50−0x100)`, slice `+0x30` left lanes | `u = rand(); place(2, -1-(u%lanes), …)` (`:542-545`) | `+= oncoming_reload`, even when gate shut (`:547`) |
| Behind/same-dir `0x004b7e94` (`timers_.behind`) | `-= d; += div256(rate)` (`:550-551`) | `< 0` (`:552`) | same front tap, slice `+0x2c` right lanes | `place(0, rand()%lanes, …)` (`:556-557`) | `+= other_reload` on fire (`:559`); else clamp to `2·other_reload` (`:560`) |
| Rear/ahead `0x004b7e80` (`timers_.ahead`, INVERTED) | `-= d; += div256(rate)` (`:563-564`) | `> 0` (`:565`) | rear tap `(DAT_0049f240+0x100)`, slice `+0x2c` | same kind-0 draw shape (`:569-570`) | `-= other_reload` on fire (`:572`); else clamp to `−2·other_reload` with `!=` guard (`:574-575`) |

Derived states (not new captures): 59 holds → `oncoming = 3515−531`,
`behind = 531`, `ahead = −3515+531`; density-40 fill → reloads `23437`
(`tests/traffic_test.cpp:162-185`); disabled-pool double expiry at tick 391 draws
exactly 2 lane RNG draws and reloads (`:188-207`). Nulled pool consumes the lane
draw but skips placement — RNG parity holds. Countdown must not re-bank the 12
zero-motion race ticks and must not pass the ring residual (timer-independent).

## Arrest flow (delays 180/0) + spawn-due periodic branch

Arrest decision (`0x00407f30.c:18-39`, native `police_arrest`, `src/police.cpp:70-73`):
owner `+0x324 ∈ {−1, 0}` (native `owner == 0`), rider `+0xc0 & 0x10 == 0` on the
rider half, stopped (`+0xec < 300`, or `||speed| − 0x708 < 0` i.e. `< 1800` when
`+0x2f4 == 2`), cop from `0x00407860` non-null, un-crashed (`cop +0x2f4 == 0`;
`+0xc0 & 0x10` veto inside find), `|cop+0x1c − rider+0x1c| < 0x200 (512)`,
`|cop+0xec − bike+0xec| < 5000`. On pass with local owner (`+0x324 == 0`):
`FUN_00416010(4, 0xb4)` — outcome 4 (busted), delay `0xb4 = 180`.
Native boundary tests (`tests/police_test.cpp:13-24`) cover every threshold edge;
all 1,024 `napa-police-arrest.tsv` rows assert `delay == (arrested ? 180 : 0)` (`:59`).

Delay setter (`0x00416010`, REA-verified this session): guarded by
`DAT_004753c4 == 0 && DAT_004753b4 == 0`; stores `DAT_004c5d7c = param_1`
(`0x7f` remaps for netplay only); `param_2 < 1` → `DAT_004753b4 = 1` (immediate end,
the delay-0 path), else `DAT_004753c4 = param_2` (the 180 path). `0x00442e10`
(read, 52 lines) then counts `DAT_004753c4 −= param_1` down per frame and raises
`DAT_004753b4` at `< 1`. No RNG in either function. Busted flows onward through
existing `finish_race` (`src/career.cpp:104-139`, screen `0x1d`, fine
`(level·5+5)·0x50`) → `Frontend::race_finished` (`src/frontend.cpp:917`) →
`BUSTED1–6` (`src/career.cpp:166`); still no live producer — open for the
implementing worker.

Spawn-due periodic branch (`0x00407f30.c:40-47`, native `police_spawn_due`,
`src/police.cpp:75-77`): same outer gate (owner `−1/0`, flag `0x10` clear) plus
`+0x324 == 0`, `(DAT_004b7ecc & 0xfff) == 0`, `DAT_004642dc == 0` (single-cop guard,
**no level guard**). Fires `0x00449910(slice, −0x2700)` then
`0x00406b80(slice, 0xffffd900 = −0x2700)` (cop `0x2700` Q8 behind player) and zeroes
the new cop `+0xec`. Native `police_spawn_due(tick,owner,flags,cop_present)` is the
exact gate (`owner==0 && !(flags&0x10) && !cop_present && !(tick&0xfff)`).
DZHR path (`0x004409e0` else-branch) is unchanged from police-spawn-20261016d:
`DAT_004642d8 != 0`, two `rand % 0x81` draws, ring slices `(r+0x200)·16`.
`0x0040a600` (REA-verified this session) adds the cop-owner side: the `0x4b0 = 1200`
run-back timer kill at `:23-24` is skipped when the cop owns `DAT_004642dc`
(`src/crash.cpp:38` documents the exemption); player timer-expiry calls
`0x00416010(5, 0xb4)` (outcome 5, same 180 shape).

## NOT changed (explicit)

No tracked file was edited, staged, or committed by this worker. In particular,
none of these were touched:

- Sibling-modified eight (live work in tree, excluded from search scope):
  `src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
  `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp`.
- Live-claimed / shared paths: `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`,
  `docs/VERIFICATION.md`, `docs/plans/24-road-hazards-police-crash-control.md`,
  `docs/plans/32-completion-review-next-steps.md`, `docs/plans/10-simulation-oracle.md`,
  `src/race.cpp`, `src/traffic.*`, `src/police.*`, `src/crash.*`, `src/terrain.*`,
  `src/race_setup.*`, `src/frontend.cpp`, `src/career.cpp`, `src/game_main.cpp`,
  the oracle proxy, `CMakeLists.txt`, `tests/oracle_sim_test.cpp`,
  `tests/traffic_test.cpp`, `tests/police_test.cpp`, `tests/race_setup_test.cpp`.
- No build directory created or built; no Wine/oracle capture run; no Ghidra project
  write (`tools/decompile.sh` untouched); no commit/push.

## Remaining work (for implementing worker, not done here)

1. Live-wire `make_police_actor` into `Race::spawn_pedestrian`'s `type ≥ 0xb` branch
   preserving the two `rand % 0x81` draws, plus the `0x00407f30` periodic branch
   via `police_spawn_due`; add single-cop guard, video-slot cycle, scheduler links.
2. Prove `end.outcome == busted` arrives from the live `0x00416010(4, 180)` path and
   triggers a `BUSTEDx` video (slice-D acceptance); capture-gated scenarios
   (`napa-cops` L3+, `city-cops` L1) per police-spawn-20261016d.
3. No golden changed here; countdown lane-tap words and `param_1` distance-delta
   series remain capture-gated for branch-A/B attribution at tick 376.
