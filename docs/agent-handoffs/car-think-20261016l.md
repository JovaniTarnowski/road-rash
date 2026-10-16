# car-think-20261016l — read-only car think/move/horn spot-check (plans 11/24 background)

Task: read-only background sweep of car think `0x0044e710` / move `0x0043adc0` / horn `0x0044efa0` + placement `0x0044df60` + cruise `DAT_004b7e8c`. Movement, honk and drawing in the port are approximations — record the laws, gates, RNG draws and approximation table, plus a capture spec. No source changes.

Baseline: `4a9b29b` (HEAD at task start; `git rev-parse HEAD`).
Worktree: `/Users/archit/Dev/rr/native-port` (shared tree; sibling in-flight edits present — none touched).
Builds/captures: none run. Commits: none.

## REA spot-check (pinned toolchain, ≤2-call budget: 1 success + 1 documented miss)

Env per `docs/TOOLING.md`: `PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`, `GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`, `JAVA_HOME=.../openjdk@21...`, `rea-agents@6.4.0`, Ghidra 12.1.4 backend, `--format md`.
Reference EXE SHA-256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (verified via `sha256sum`; matches `analysis/inventory.json` and the REA `subject.digest` in the new file).

| Addr | Role | REA result this session | Evidence |
| --- | --- | --- | --- |
| `0x0043adc0` | car move dispatch | SUCCESS, exit 0 | `analysis/car-think-20261016l/rea-43adc0-move.md` (35,660 bytes), evidence_id `ev_2312b40a10b00ca2a2e57238ee45c89b059e40c84c043c1c28a268ddcf3aaae3` |
| `0x0044efa0` | car horn (contact reaction) | MISS — `invalid_request`: "No procedure exists at the requested address: 0x44efa0", exit 1 | `analysis/car-think-20261016l/rea-44efa0-horn.md` (657-byte error record) + `.log` files |
| `0x0044e710` | car think | NOT re-fetched (already confirmed Oct 12: 10,170 chars whitespace-identical; see REVERSE_ENGINEERING.md) | prior `analysis/oracle/rea-44e710-carthink.md` (git-ignored, not re-run) |
| `0x0044df60` | placement | NOT re-fetched (already confirmed Oct 12: 3,472 chars whitespace-identical) | prior `analysis/oracle/rea-44df60-xing.md` (git-ignored, not re-run) |

`0x0043adc0` comparison (normalized-whitespace, same method as prior REA handoffs): REA `raw_result.value.pseudocode` norm-chars 295 == project `analysis/decompiled/0043adc0.c` norm-chars 295 → TRUE. Shape:

```
if (*(int *)(param_1 + 0x160) == 0) { +0x114 = 0; +0x118 = 0; +0x164 = 0; }
FUN_004390c0(param_1); FUN_00439920(param_1); FUN_0043a610(param_1);
if (*(int *)(param_1 + 0xc4) != 0x10) FUN_0043a190(param_1);
```

REA xref table in the same file confirms the four callees at `0x43ade3/0x43adea/0x43adf1/0x43ae03`. The horn miss reproduces the known Oct-12 finding (`soundhelpers-rea-20261012.md`: horn absent from REA's procedure database; port from the 651-char project export) — no new REA evidence obtainable for `0x0044efa0`.

## Movement law (from `analysis/decompiled/0044e710.c` + `0043adc0.c`)

- Cruise scale: `DAT_004b7e8c = 25` (set by `0x0044dd00(-25)`; distinct from spawn rate `DAT_004b7e84 = 2500` — the 100× bug documented in `docs/TOOLING.md` gotcha 8).
- Per-car cruise target (target units ≈ v/100): `iVar8 = (25 * phase[+0x71]) >> 8`, target speed `iVar8 * 100`; `phase = (rand & 0x7f) + 0xc0` (192..319, full int slot — byte truncation freezes the car).
- Current-speed units: `iVar9 = (v>>9) + (v>>12) + (v>>7)`; `near = iVar9*0x300>>8`; gap floor `iVar7 = max(near, 0x14)`; window `iVar15 = iVar7*2`.
- Signal latch (kinds 0/2, `+0x7b == 0`): nearest signal of type `0x00468f00[kind]` (index 0/2) ahead within `(window+0x80)*16` via `0x00449e60`.
- Obstacle scan: `0x0044a070(obj, lane, kind, mask 0x3f, iVar7<<5)`; even kinds scan driving direction by lane, odd kinds across the same sample; follow through `0x00449a80` (floor 0, step 300, bias `−gap_unit`).
- Stop lines: kind 0 at controller start `+0xa0`, kind 2 at end `−0xa0`, crossing at road edge `∓0x60`; red (phase 2) and close yellow brake to 0; past-the-line hurries (target ×2, accel `0x24` instead of `0x12`).
- Accel rate: `phase * ((current < target/2 || hurry) ? 0x24 : 0x12) >> 8`; applied via `0x004498c0` (`+0xec += dt·rate`, clamped).
- Drift: `0x00475694[kind] (low byte) * 800` toward lane-centre target `(centre + lane*250 + 125)*256`.
- Move dispatch `0x0043adc0`: zero `+0x114/+0x118/+0x164` when `+0x160 == 0`; then `0x004390c0 → 0x00439920 → 0x0043a610`, plus `0x0043a190` unless `+0xc4 == 0x10` (cars skip the landing-bounce call).

## Horn gating (`analysis/decompiled/0044efa0.c`, 651 bytes; REA has no entry)

```
if (DAT_004c5b00 == 0 && DAT_004c5af9 == 0 && +0x1a0 == DAT_004642d8 /* player is look-ahead target */) {
  if (_rand() % 10 == 1 && +0x1cc != 0) FUN_00436830(0x12 /* sound 18 */, +0x1c, +0xec);
}
+0x1f4 = 1;                                            // unconditional
if ((+0x1cc & 1) && +0x1f8 == 0) { +0x1f8 = 0x3c; if ((*(+0x1a0) + 0xc4) & 0xc) +0x1f8 = 0xf0; }  // chaser long timer
```

No stop, swerve or speed change — the car drives through the rider.

## RNG draws (shared MSVC CRT stream; order is replay-critical)

| Site | Draws per firing |
| --- | --- |
| Timers/crossing lane pick (callers of `0x0044df60`, incl. nulled-pool solo) | 1 `rand()` per gated expiry (`−1 − rand%left`, `rand%right`, crossing `lane = offset + rand%range`) |
| `0x0044df60` vehicle pick | 1 `_rand() % accepted` (loop excludes last vehicle; speed-class filter `0x40000/0x80000/0x100000` by kind) |
| `0x0044df60` per-car jitter | 4× `0x00441750`: phase `(r&0x7f)+0xc0`, `+0x72`, `+0x1c9`, `+0x1ca` |
| `0x0044e710` think proper | 0 draws itself (scans/follow/accel are pure); spawns that feed it draw in `0x0044dd90`/`0x0041f040` |
| `0x0044efa0` horn | 1 `_rand()`, gated (`%10==1`, player-target, `+0x1cc != 0`, demo-hold clear) |
| `0x0043adc0` dispatch | 0 draws itself (sub-call draws, if any, live in `0x0043a610` etc. — out of scope) |

Native order preserved in `src/traffic.cpp`: lane draw → `choose()` → phase/jitters → serial `DAT_004b7e98`; horn draw last in `move()` (`tests/traffic_test.cpp` nulled-pool test pins 1 lane draw per gated expiry with zero cars).

## Approximation table (native vs original)

| Area | Native (`src/traffic.cpp`, `traffic.hpp`) | Original | Gap |
| --- | --- | --- | --- |
| Cruise/accel | `plan_speed()`: cruise `(timers_.cruise*phase)>>8`, `speed_units()`, `follow_speed()` (`0x00449a80` shape), phase-scaled rate `0x12/0x24` | `0x0044e710` + `0x004498c0` | Lane-change past slower obstacle (`gap < iVar7*3/2` branch) NOT ported (comment line 230); surface drag neglected in position advance (comment line 319); split-roadway parks past other edge NOT ported (comment line 299) |
| Horn | `move()` tail: player-in-`lookahead_[kind]`-window + `\|player_lat − car_lat\| < 600*256` + `random()%10==1` → `SoundEvent::horn` | `0x0044efa0`: `+0x1a0 == player` (think-set look-ahead target, not raw player pos) + `DAT_004c5b00/004c5af9` gates + `+0x1cc != 0` + same 1/10 draw; writes `+0x1f4 = 1`, `+0x1f8` `0x3c`/`0xf0` | Native has no demo-hold gates, no `+0x1cc` gate, no `+0x1f4/+0x1f8` words (`TrafficCar` has no such fields); lateral `600*256` window is a port-side interpretation of the look-ahead relation |
| Move dispatch | `move()` integrates think→position→steer per kind in one pass | `0x0043adc0`: `0x004390c0 → 0x00439920 → 0x0043a610 (+0x0043a190 unless car)` chain | Native car motion does not run the `0x004390c0/0x00439920/0x0043a610` chain; wall/terrain response for cars is unported |
| Drawing | `traffic_draw_nodes()`: verified scenery projection + depth-table scale, frame `(tick + phase) % frames` | RRAN advancement `0x0043ae10/0x0043ae90` + actor billboard `0x004467e0/0x00446830` + CCB world-size scaling | Frame cycling and `sprite_size_` scaling are native stand-ins (comments lines 641–643, `traffic.hpp`); `traffic_car_node()` billboard path exists but the draw pass does not use it |

Prior carthink handoff (`carthink-rea-20261012.md`) covered think/teardown/signal-draw; `analysis/car-notes.md` covers the contact-pass horn-only reaction and the 256-case `0x004049c0` fixture. This note adds the move-dispatch REA confirmation and the horn-absence re-proof.

## Capture spec (for the porter; NOT run — no builds/captures per task)

1. `city-intersection` (plan 24 Task 1 Step 2): CITY attract-demo or human run through first type-1 CESR zone; dump car fields (`car.<i>.*` per `tests/sim_fields.hpp car_fields`) + ring cursors `DAT_004b7e50/DAT_0049f240/DAT_004b7e5c` every tick. Checks the +0x5100 ring offset, zone-density timing, signal/crossing behavior. Confirm native replay fails first (Step 5 needs zero divergence + one CITY intersection pixel frame in `oracle-scene`).
2. `napa-crash-car` tick scenario (plan 10): end-to-end rider–car contact parity (handler golden `analysis/oracle/napa-car-contact.tsv` is exact given seeded impulses; live impulses via real `0x00436a40` masses 1800/256 still open).
3. Horn audibility is NOT capturable in field dumps (scratch bike is never the player; `snd_after44ef − snd_after4049 = 0` in all 256 fixture cases) — needs a player-as-target run.

## NOT-changed list (read-only compliance)

- No tracked file edited. In particular the seven live-claimed paths were not touched: `src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`, `tests/race_cache_test.cpp`, `tests/road_test.cpp` (at handoff close `git status` also shows `tests/course_scene_test.cpp` modified — likewise untouched).
- New files only: this handoff + `analysis/car-think-20261016l/` (`rea-43adc0-move.md/.log`, `rea-44efa0-horn.md/.log`, `notes.md`).
- Native car code was READ (not edited): `src/traffic.cpp`, `src/traffic.hpp`, `tests/traffic_test.cpp`, plus `src/race.cpp` contact/ stepping references and `analysis/car-notes.md` / `analysis/decompiled/{0043adc0,0044e710,0044df60,0044efa0}.c`.
- No builds, no oracle/proxy captures, no commits, no shared-doc edits, no live processes or locks held.

## Remaining work (porter-owned)

- Port or explicitly defer: `+0x1f4/+0x1f8` horn-timer words, `DAT_004c5b00/004c5af9` horn gates, `+0x1cc` horn gate, think-set look-ahead target (`+0x1a0`) vs raw-player horn check, lane-change-past-obstacle branch, surface drag, split-roadway car parks, `0x0043adc0` sub-chain for cars.
- Captures + acceptance per spec above (plan 24 Task 1 Steps 2/5; plan 10 `napa-crash-car`).
