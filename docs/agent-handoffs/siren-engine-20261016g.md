# Siren + car-engine background (20261016g, read-only)

Task: Plan 11 background — sirens + car engines (open per plan 00:
race MIDI + sirens + car engines not done; police siren still
`default`-break in `sound.cpp:349`). READ-ONLY: no tracked file edited,
no builds, no captures. Baseline HEAD `242f718` (verified `git rev-parse
HEAD`; `git status --short` at turn start shows only `?? analysis/*` scratch
dirs, no tracked modifications).

Owned outputs (this turn only):
- `docs/agent-handoffs/siren-engine-20261016g.md` (this file)
- `analysis/siren-engine-20261016g/` (`notes.md`, `rea-435ae0-siren.md`,
  `rea-4350e0-engslots.md`, plus `.log` files)

## REA spot-check (2 calls max, pinned env, `--format md`)

Env: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`rea-agents@6.4.0`, Ghidra 12.1.4 backend. EXE
`../cdrom/ROADRASH/ROADRASH.EXE` SHA-256 verified
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
pre-call. Spot-check only per `docs/TOOLING.md`, not runtime proof.
No captures run.

| evidence_id | addr | normalized vs `analysis/decompiled/<addr>.c` | verdict |
| --- | --- | --- | --- |
| `ev_fb6263db9cf333d5292294375951c18859e8d2f420877c012981617fa2fec7dd` | `0x00435ae0` siren | 1472 / 1472 (raw 2119 / 2122, whitespace-only delta) | MATCH |
| `ev_1983374e7046ba2b4ba07f51f13c754bea98f7b8cbcae7620fbf8f75a42bf8aa` | `0x004350e0` nearest-two select | 1069 / 1069 (raw 1695 / 1698, whitespace-only delta) | MATCH |

Detail (phase counters, ring-walk filter, Doppler/pan/rpm law): scratch `notes.md`.

## Exact addresses (slots + chain)

- Slots (`0x00435e60`, 0x45): 0 player (`EngRat/Sport/Super` by bike id),
  1 `EngNitro`, 2 `EngTunne`, 3/5 bikes `EngOther`, 4/6 cars `Engcar11`,
  29 `BMWLONG` (horn), 30/31 `SIREN`/`SIRENFST` (siren dual buffers
  `DAT_004c40d8`/`DAT_004c40dc`).
- Siren `0x00435ae0` (event 19): enable gate `DAT_0049768c==0`; spatial
  `0x00435840` (police slot `DAT_004642dc`); level `(DAT_00497688*v)/100`
  clamp `0x4000`; zero-volume stops both buffers; else dual-buffer alternation
  at fixed `0x2b11` Hz with phase counters `0x259/0x79` ticks, direct-vtable
  mute + `0x00414180` playing-check handoff (`DAT_0046d998/9a0/9a4/99c`).
  No RNG (pure phase machine).
- Engine select `0x004350e0`: ring walk from `*DAT_004642d8` via `pi[1]`,
  metric `|pi[7]-DAT_004b7ef4|`, filter `pi[0x31]==4||0x10`, keep two smallest
  within `0x1e00`. No RNG.
- Voice law: `0x004349c0` rpm (`+0x2dc`, gear `+0x1cc+gear*0x10`, slew
  `+-0x2000`, `+0x1000` airborne cap `0xffff`, `-0x1000` post-crash,
  `+0x2e0=0x9ffe`); `0x00434db0`/`0x00435840` spatial (hearing `0x1e00`,
  `d=|dp|-min/2+|dl|`, `v=((0x1e00-d)*8/256)^2/256`,
  `doppler=0x13b0000/(0x13b00-rel)`, `pan=octant*0x62`);
  `freq=doppler*rpm/512`, `vol=(0x9ffe*vol/0xef10)*level`;
  DS map `0x004141c0`/`0x004142c0` (`(v*10000>>14)-10000`, loop-stop `-8999`).
- Levels `0x004362e0` 101-entry `curve[level*master/100]`; Mixer `0x00463030`.
- Callers/tables: consumer `0x004351f0` (siren-19 fast path); per-vehicle mix
  `0x00434ce0` (player) / `0x00434fd0`; horn `0x0044efa0` (event 18,
  `rand()%10==1` + `0x1f8` timer gates); police build `0x00406b80`, spawn
  `0x004409e0` else-branch (DZHR type `>=0xb`), arrest `0x00407f30` ->
  `0x00416010` code 4 + `0xb4` ticks.

## Trigger points

- Siren: cop spawn (DZHR type `>=0xb`) -> per-tick `0x00435ae0` -> event 19 ->
  `0x004351f0`. Napa L1 has NO cops (census 42 ped / 0 cops); siren scenarios
  must use Napa L3+ (2 records) or CITY L1 (6).
- Car engines: per-tick `0x004350e0` nearest-two -> `0x00434fd0`-class mix into
  slots 3/5 (bikes) or 4/6 (cars). Horn: `0x0044efa0` (player nearest ahead
  within `lookahead_[kind]`, lateral `<600*256`) -> event 18 ->
  `TrafficSystem::take_sound_events` -> `game_types.cpp:148` -> `post`.
- Native gaps (read-only findings): event 19 hits `default: break`
  (`src/sound.cpp:349`); `siren_` level stored never consumed; engine update
  (`src/sound.cpp:284-285`) always `stop(base+1)` + `play_loop(base)` so slot
  4/6 `Engcar11` never sounds; `tunnel_=false` stand-in (`:252`).

## Hook plan (plan 11 Task 6/7 + plan 24 Task 2 + plan 13 seam)

Owner files per plans (DO NOT touch in this turn; listed for the implementer):
plan 11 Task 6 (`src/police.hpp/.cpp`, `src/race.cpp` call point,
`src/actor_nodes.cpp` cop nodes, `src/sound.cpp` slots 30/31 + event 19,
`src/game_main.cpp` busted outcome, `tests/police_test.cpp`);
plan 11 Task 7 Step 1 (`src/sound.cpp/.hpp`, `src/hud.*`, `src/game_main.cpp`,
`tests/sound_test.cpp`, `tests/hud_test.cpp`: slots 4/6 car rule +
`nearest_vehicles` synthetic test); plan 24 Task 2 Step 3 (spawn/think/move,
`0x004049c0` type-`0x20` contact, sirens, HUD slot-14 check).

Conflict-free seam (follows midi-race-20261016f): keep ALL siren/engine voice
work inside `SoundSystem` (`play_loop` slots 4/6 + 30/31, `distance`,
`update_engine_rpm` already shared with the dash); plan 11/24 keep owning
`race.cpp`/`game_main.cpp`/`actor_nodes.cpp`; plan 13 keeps owning
`race_music.*` + the `sound.cpp:236-238` pump / `:144-148` level forward only.
No new `game_main.cpp` hook for audio voices. Suggested commit split:
`sim: live police, sirens and arrests` (Task 6/24-2) then
`audio: car engines and horns` (Task 7 Step 1).

## Capture spec (for the owner; none run here)

- `napa-cops` (Napa L3+, cops present), `napa-cops-crashed` (arrest while on
  foot), `city-cops` (CITY L1, 6 records): each must reach busted outcome
  `0x00416010` code 4 + video choice; single-anchor `oracle-sim` accept + one
  cop frame pixel-matched in `oracle-scene` (plan 24 Task 2 Step 4).
- Car-engine golden: two cars + two bikes at chosen distances proving
  `nearest_vehicles` picks the original 4 slots (plan 11 Task 7 Step 1
  synthetic test first; golden traces nearest-two + `+0xcc` volumes).
- Proxy prerequisite: pipeline still down per sirenprobe (`smoke-napa-default`
  exit 124, 2 frames frozen + Wine page fault); `g_race_started` menu re-entry
  fix is shared-ownership blocked — coordinate prefix/DLL-install/process
  ownership before any capture; require valid header + contiguous ticks +
  completion marker; never overwrite an accepted capture with a partial run.

## NOT-changed list

No tracked file created, modified, staged, or committed. Untouched in
particular: `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`,
`docs/VERIFICATION.md`, `docs/plans/10/*`, `docs/plans/32/*`, `src/race.cpp`,
`src/traffic.*`, `src/terrain.*`, `src/race_setup.*`, oracle proxy,
`CMakeLists.txt`, `oracle_sim_test`, `traffic_test`, `race_setup_test`,
`src/sound.cpp/.hpp`, `src/game_main.cpp`, `tests/*`, `oracle/*`,
prefix/install, Ghidra project, build dirs, `analysis/decompiled/`,
`analysis/oracle/`. Only the two owned outputs above were written. No builds,
no captures, no processes left running, no locks held.

## Remaining work (not done here)

Siren + car-engine ports (plan 11 Task 6/7, plan 24 Task 2 owner scope);
`0x00434fd0` car-kind/`+0xcc` gate disassembly check (element-scaled indexing
suspect); `0x00435840` vs `0x00434db0` `+0xd8/+0xc8/+0xcc/+0xd4` store mapping
for the siren voice struct; plan 13 Task 1 Step 2 AWE voice trace still open.
Integrator: file under plan 11 background; keep
`analysis/siren-engine-20261016g/` local incl scratch REA md (disc-derived).
