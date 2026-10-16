# Proximity/contact-box background — REA spot-check (read-only, 20261016r)

- Task: read-only background on proximity/contact-box helpers (proximitybox verify, spatial helpers,
  speed helpers). Spot-check two helpers that had NO prior REA dossier; table the cluster against native.
- Baseline: HEAD `b5be006` (`git rev-parse HEAD`). No tracked file edited, no builds, no Wine captures,
  no Ghidra writes, no commits.
- EXE: `../cdrom/ROADRASH/ROADRASH.EXE`, sha256
  `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
  (verified via `shasum -a 256` before the REA calls; matches `analysis/inventory.json`).
- Sibling eight off-limits (tracked modifications owned by other workers; excluded from every
  `rg`/`fd` via `--glob`, neither read nor edited): `src/course_scene.cpp`, `src/race_caches.hpp`,
  `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp`.

## 1. Covered vs uncovered survey (prior handoffs read, not modified)

Covered — NOT re-fetched: remount proximity box `40a785–40a7c9` (proximitybox-verify-20261012),
`0x00449c20` (schedwalk-rea-20261012), `0x00435840`/`0x00434db0` (spatialhelpers-rea-20261012),
`0x00409020`/`0x00449a80`/`0x0044a070`/`0x00449b40` (speedhelpers-rea-20261012),
`0x00449ce0`/`0x00449e60` (schedwalk-rea-20261012), `0x00449c80` (schedwalk-20261016q),
`0x00449da0` (damagesplits-rea-20261012), `0x0044a110`/`0x00449fe0` (overtake-chain-20261016m),
`0x00449e90` (overtake-chain-rea-20261012).

Uncovered (no prior dossier) — the two REA targets of this session:
`0x00449c30` (car-AI gap comparator; schedwalk-20261016q §S5 still-open list) and
`0x004359b0` (slide-voice driver over spatial `0x00435840`).

## 2. REA spot-check (pinned 6.4.0, exit 0 both, ≤2-call budget spent)

Env per `docs/TOOLING.md`: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=…/openjdk@21`, `npx --yes rea-agents@6.4.0 function <exe> <addr> --format md`.

| # | Address | evidence_id | Result |
|---|---|---|---|
| 1 | `0x00449c30` (gap comparator) | `ev_5d42745f1227e844b83e5ce76fd73a8106af6ef5ceb0e4c82944280f677e0a1a` | body `0x449c30–0x449c75` (70 B); REA 236 chars vs export 236, TRUE |
| 2 | `0x004359b0` (slide voice) | `ev_aa0d6a85a3e286271eabcf34dae960264787c165f1e114afd4f1e99c00a1c67a` | body `0x4359b0–0x435ade` (303 B); REA 824 chars vs export 824, TRUE |

(Char counts are `## raw_result.value.pseudocode` whitespace-stripped vs `analysis/decompiled/` same
normalization.) Raw outputs (scratch only, disc-derived, never commit):
`analysis/proximity-20261016r/rea-449c30-gap.md(.log)`,
`analysis/proximity-20261016r/rea-4359b0-sirenvoice.md(.log)`,
`analysis/proximity-20261016r/notes.md`.

Semantics confirmed (static only):
- `0x00449c30(self+7, obst+7, kind)`: `kind&1==0` → position gap `>>4` (`kind==2`: self−obst, else
  obst−self); `kind&1==1` → lateral word `[3]` gap `>>12` (`kind==1`: obst−self, else self−obst).
  No RNG, no callees. Sole export-grep caller `0x0044e710:64` feeds `gap < (iVar7*3)/2`
  (lane-change) and `gap < iVar15 = iVar7*2` (`0x00449a80` follow engagement, bias `−iVar7`).
- `0x004359b0(voice)`: `0x00435840(0, voice, &dist, &vol, &pan)` then `+0x18 > 5` gate — start
  (`DAT_004977b0 = 13000`, freq `+0x18+4000`), stop, or swell (`+0x1e`/update, cap `0x4fff`,
  kill when freq `< 4000`). Pan comes from the spatial helper. No RNG in body.

REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay or pixel claim is made from it.

## 3. Box thresholds (original constants the porter owns)

| Box / helper | Thresholds | Native carrier | Verdict |
|---|---|---|---|
| Remount proximity (`40a785–40a7c9`, anim 38) | `\|Δpos\| ≤ \|Δlat\|/128` (sign-fixed `sarl $0x7` w/ `0x7f` rounding) and `\|Δlat−0x1770\| ≤ 0x1770` (lateral within 6000 of bike after offset); `+0x2f0` tested at `40a7cb–40a7d3` | `recover_crash_actor` (`src/crash.cpp:44-53`): `\|long\| ≤ 9`, `\|lat\| < 6001` (lat pre-offset by −6000) | ported (prior verify) |
| Anim-37 close pick (`0x00449c20` logical-shift abs) | `distance = \|bike.pos − rider.pos\|`, `!+0x2f0 && distance < 80`: `loops ≥ 1` → wreck (player) or `+0x2f0 = +0x2ec/4`; else `loops > 1` → queue 38 | `src/crash.cpp:66-78` (quirk comment `:67`) | ported |
| Walk timer / states | `state_elapsed > 1200` ticks, police-owner skip, walk speed `0x4b0` = 1200, steer bits `0x7800`; `+0x2f4`: 2 = down, 3 = remount-pending; anims 37/38/41/2 | `src/crash.cpp:31-43,56-60` | ported |
| Spatial (`0x00435840`/`0x00434db0`) | range gate `0x1e00`; `d = \|dp\| − min/2 + \|dl\|`; `vol = ((0x1e00−d)×8/256)²/256`; Doppler `0x13b0000/(0x13b00−Δv)`; pan octant `×0x62` | `SoundSystem::distance` (`src/sound.cpp:213-233`, `hearing = 0x1e00` at `:30`) | ported |
| Speed (`0x00409020`/`0x00449a80`/`0x0044a070`/`0x00449b40`) | curve taper at `\|avg\| > 0x30000`; follow slew `±max_dv` + lo/hi rails; obstacle lane/0x100-page scan; bend mean `>>2/>>3/>>4` | `ai_curve_limit` (`src/race.cpp:1338-1349`), `follow_speed`/`plan_speed` (`src/traffic.cpp:150-170,177-238`) | ported |
| Gap (`0x00449c30`, NEW) | `>>4` position modes / `>>12` lateral modes; caller gates `(iVar7*3)/2` and `iVar7*2`, `iVar7 = max(0x14, speed-units×0x300>>8)` | `follow_speed` arms + `plan_speed` gap (`src/traffic.cpp:152-156,183-184,228-237`) | match except GAP (static): `(iVar7*3)/2` lane-change branch not ported (`:230`); single `gap < window` gate |
| Slide voice (`0x004359b0`, NEW) | start vol 13000, swell `+0x1e`/update cap `0x4fff`, freq `speed+4000`, kill `< 4000`, `+0x18 > 5` gate, spatial pan | `SoundSystem` slide case (`src/sound.cpp:333-343`) | APPROX (static): freq/vol/swell match; pan is 0 (no `distance()` call) and start is event-driven, not `+0x18 > 5` gated |

## 4. NOT-changed list (explicit)

- No tracked file edited — in particular untouched: the sibling eight
  (`src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`, `src/road.cpp`,
  `src/road.hpp`, `tests/course_scene_test.cpp`, `tests/race_cache_test.cpp`,
  `tests/road_test.cpp`) plus `src/crash.*`, `src/race.*`, `src/traffic.*`, `src/sound.*`,
  `oracle/`, `CMakeLists.txt`, all shared docs and plans, `analysis/decompiled/*`, Ghidra project.
- No builds, no test runs, no Wine captures, no prefix/DLL changes, no golden/allowlist edits,
  no reanchor, no commits.
- New files only: this handoff + untracked scratch `analysis/proximity-20261016r/`
  (`rea-449c30-gap.md/log`, `rea-4359b0-sirenvoice.md/log`, `notes.md`).

## 5. Remaining work (owner / integrator)

1. Porter decides the §3 gaps (lane-change `(iVar7*3)/2` arm in `plan_speed`; slide-voice spatial pan +
   `+0x18 > 5` start gate): both additive, synthetic-fixture testable, no capture needed.
2. Still uncovered in the wider scan cluster (committed text only): `00449e90`-adjacent overlap widths are
   covered, but `00455260`/`004551c0`/`004515a0`/`00451cb0`/`004520d0` remain for the ring/slice backlog
   (schedwalk-20261016q).
3. Integrator: link from the plan-24 Task 3 / plan-13 Task 1 index on next refresh; keep
   `analysis/proximity-20261016r/` local (disc-derived REA dumps, never commit).

## Live processes / resources

No live processes (both `npx` calls exited 0 and returned). No build locks, no worktrees, no LSP/database
writes, no shared resources held. No `git add`, no commit.
