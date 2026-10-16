# Wall/crash background research — Plan 11 Task 4 / Plan 32 slice D

Baseline: `aae00a1` (HEAD matches). Read-only; no tracked files edited, no builds, no captures. REA budget used: 2/2 calls.

## Stand-ins (per PROGRESS.md Assumptions)

- `src/race.cpp:1064` `Race::walls` — stand-in for `0x0043a610`: clamps lateral at shoulder edge (`SliceEdges`), zeroes `+0x10c/+0x104/+0xf0/+0x288`; above speed (`+0xec`) 4000 crashes via `crash(r)` (player-only `SoundEvent::grunt_c`), else `+0xec *= 3/4`.
- `src/race.cpp:1149` `Race::crash` / `step_crash` — stand-in for `0x00403d60`: legacy path sets `+0x2f4=1`, `+0xc0|=0x10`; library path calls `begin_crash` + `recover_crash_actor` with `r.crash_timer`. Plan 11 Task 4 acceptance: port `0x0043a610`, crash states (flying/sliding/standing/run-back/remount), car-contact branches; all four `napa-crash-*` scenarios pass `oracle-sim` zero-divergence; remove stand-in line.
- `src/crash.hpp` / `src/crash.cpp`: `begin_crash` (entry), `advance_actor_motion` (local part of `0x00439920`), `recover_crash_actor` (offline branch of `0x0040a600`), `remount_crash_actor`. Read-only LSP: `agent_lsp.py --check src/crash.cpp` exit 0, 0 errors (DB healthy=false only: `CMakeLists.txt` newer than `build-clangd` DB; no proof of client loading).

## REA evidence (pinned: node v24.13.0, Ghidra 12.1.4 libexec, JDK 21; EXE sha256 `73d7f974…f18a2` matches)

Full outputs (git-ignored scratch): `analysis/wall-crash-20261016c/rea-43a610-wall.md`, `rea-403d60-crashentry.md`.

- `0x0043a610` — evidence_id `ev_38ed3600…13ea3be`. Signature `void __fastcall FUN_0043a610(int param_1)`; body 1942 bytes; pseudocode 9798 chars. Callers: `0x43a580` (per-tick dispatcher), `0x43adc0` (crash independent motion). Callees: `0x00403d60` (×2: reason 3 at wall-test, reason 6 at response), `0x00436830` (sound), `0x00439270` (route advance).
- `0x00403d60` — evidence_id `ev_355915bf…e76b1e28`. Signature `void __fastcall FUN_00403d60(int param_1, int *param_2, int *param_3, undefined4 param_4)` = (bike, rider_impulse[3], bike_impulse[3], reason). Body 864 bytes; pseudocode 4803 chars. Callers: `0x4049c0`, `0x4052c0`, `0x438c70`, `0x43a190`, `0x43a610`. Callees: `0x00407410` (thunk), `0x0040ac70`/`0x0040aca0` (anim set/queue), `0x00436830` (sound), `0x0043cc80` (network/route branch). No rand import in either body; RNG lives upstream (`0x004049c0` grunt/crash draws) and downstream (`0x00407a90`/`0x00407f30` anim).

## Wall test geometry / response (from 0x0043a610 pseudocode)

- Early-out: `x = +0x28 ± +0x174`, `>>8` vs slice `+0xe0/+0xe4` (road edges); inside → return.
- Split/bridge pre-pass (`+0x38==5`, bike `+0xc4` 4/8): per-side wall proximity via interpolated `+0xe8/+0xec` edges and `±4` inset; sets `uVar9=1`, target `iVar7`.
- Type switch on slice `+0x3c`: case 0 (open road edge L/R with `0xfa/3` taper), case 1 (player-only `DAT_004642d8` split window `+0x14c/0x150/0x154/0x158/0x15c/0x160`), case 3 (symmetric `+0xe8/+0xec`), case 5 (player split variant). Scrape flag `bVar1` when clearance < 1.5–2× half-width `+0x174` (uVar9=2).
- Response: `uVar9<3` → lateral velocity `+0xf0 *= -100>>8`; `uVar9==3` (+0xc4==4, `+0x160==0`, airborne-threshold) → `FUN_00403d60(p,0,0,3)`; zero `+0x60`, clear `+0x288` on matched lateral sign, `+0x5c=0` (bike) or `0x800000` sign term (rider `+0xc4==8`). Grunt/crash gate: `|v_lat|>0x9c3` (2507) and `uVar9!=3` (or `bVar1`): player (`+0x324==0`) → sound `FUN_00436830(5,…)` (event 5, wall grunt), then `FUN_00403d60(p, local+3, local, 6)` with rider impulse `{0,0x10,0x50}` layout. Final `+0x28 = iVar7` (all int32 Q8; `+0x174` half-width, `+0x28` lateral, `+0xf0` lateral vel, `+0xec` speed).

## Crash state machine / timers (entry + 0x0040a600 notes)

- Entry (`0x00403d60`): network/owner gate (`+0x324` −1 remote → return; `+0x2f4!=0` → return); copies `+0x1c–0x40`, `+0x30–0x44`, `+0x58/0x5c/0x60`, `+0xec/0xf0/0xf4`, `+0x104/0x108/0x10c/0x110`, `+0x12c–0x160` bike→rider (`+0x2fc` link); zeroes rider `+0x114/0x118/0x11c/0x120/0x124/0x128/0x198/0x1a0`, `+0x164/+0x19c` (u8); `+0x154` bike=0x5f/rider=0x46; rider `+0x20 += 0x4b00`, bike `+0x20 += +0x16c`; `+0x2f8=reason`, `+0x2f4=1`, rider `+0xc0|=3`, bike `+0xbc=0/+0x288=0/+0x244=1`; dims `+0x174/178/17c` from `0x4643f8`; anim `29` (reason 2) / `35` (4,5) / else `33`, queue −1 (matches native `begin_crash` + `read_crash_config` `0x4643f8/0x463c10/0x4643e0`).
- Recovery (`0x0040a600`, already ported in `recover_crash_actor`): `+0x2f4` 1=flying/sliding → 2=on foot → 3=remount pending; anim 37 (prone) / 38 (walk) / 41 (remount queue 2); 1200-tick auto-remount timer; walk speed 1200 (`0x0040a9d0`); proximity box `|Δlong|≤9`, `|Δlat−6000|<6001` (+6000 offset on remount); player walk bits `0x7800` (`0x800/0x1000/0x4000/0x2000` = accel/brake/left/right); `state_elapsed>1200` + no bike (`+0x2f0==0`) → player wrecked; bounds-wreck on slice type 1 (`+0x14c/+0x158`) and remount gate (state 3 + anim frame 0x29 → `0x00404560`) still open per plan 24 Task 3.

## Function signatures (porter input)

- Original: `FUN_0043a610(int bike)`, `FUN_00403d60(int bike, int rider_impulse[3] | NULL, int bike_impulse[3] | NULL, int reason)`.
- Native: `Race::walls(Rider&)`, `Race::vertical(Rider&)` (`0x0043a190`, ported), `Race::crash(Rider&)`, `Race::step_crash(Rider&,bool,uint32_t)`, `Race::crash_car(Rider&)` (reason 2), `Race::crash_reason(Rider&,int32_t,std::array<int32_t,3>,std::array<int32_t,3>)`, `Race::knock_off(...)` (`0x004052c0`, no RNG inside); `crash.hpp`: `read_crash_config`, `init_crash_actor`, `begin_crash`, `advance_actor_motion`, `recover_crash_actor` (+`CrashRecoveryInput{state_elapsed,player,police_owner,walk_controls}`), `remount_crash_actor`. All motion fields int32 (`i32`), flags u8 (`+0xc0,+0x164,+0x19c,+0x244`).

## Scenario-capture plan (no captures run — proposal for porter)

Base: `docs/SIM_SCENARIOS.md` solo line (`RR_ORACLE_COURSE=4 … RR_ORACLE_SOLO=1`); human-branch stream `sim-napa-solo-human.controls` (1250 words) exercises player-control path. Derive per plan 11 Task 4 Step 1:
- `napa-crash-wall`: human stream to race speed, then hard-left/right override into wall at `+0xec>4000` (isolates `0x0043a610` uVar9=1/3 + reason 6 path).
- `napa-crash-car`: `drive=0` straight into oncoming traffic (isolates reason-2 car path via `0x004049c0`).
- `napa-crash-split`: crash inside first split window (isolates case-1/split branch + remount-on-same-road rule).
- `napa-crash-wreck`: repeated crashes until `+0x2f0==0` (isolates `0x0040a600` wreck outcome).
Each: record controls, replay with single RNG anchor; zero-divergence `oracle-sim` is acceptance. Coordinate prefix/DLL/output ownership per AGENTS.md before capturing.

## NOT-changed list

No tracked files edited. Untouched live-claimed paths: `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`, `docs/plans/10-simulation-oracle.md`, `docs/plans/32-completion-review-next-steps.md`, `src/race.cpp`, `src/traffic.*`, `src/terrain.*`, `src/race_setup.cpp`, `oracle/aweman_proxy.c`, `CMakeLists.txt`, `tests/oracle_sim_test.cpp`, `tests/traffic_test.cpp`, `tests/race_setup_test.cpp`. Only created: this handoff + `analysis/wall-crash-20261016c/` (2 REA outputs + `notes.md`). No commits, no builds, no captures.
