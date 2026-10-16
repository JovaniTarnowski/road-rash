# Police live-wiring background — 20261016d (READ-ONLY)

Task: Plan 24 Task 2 / Plan 32 slice D background — live police spawn/pursuit/arrest.
Helpers exist (`src/police.*`, `tests/police_test.cpp`); no live cops.
Baseline: HEAD `8851ef6`. **No tracked files changed; no commits; no builds/captures.**

## Evidence (spot-check only)

- Pinned REA `rea-agents@6.4.0` / Ghidra 12.1.4, env per `docs/TOOLING.md`
  (`node/v24.13.0`, `GHIDRA_INSTALL_DIR=…/ghidra/12.1.4/libexec`,
  `JAVA_HOME=…/openjdk@21`), EXE sha256
  `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (matches `analysis/inventory.json`).
- 2 calls, `--format md`, both exit 0, both whitespace-identical to `analysis/decompiled/`:
  - `0x004409e0` setup — 1922 vs 1921 chars, TRUE,
    `evidence_id ev_12321f005c9ac5716f062345d60df1fd0da61df6139a7602b82e408db2402500`.
  - `0x00407860` cop-find — 659 vs 658 chars, TRUE,
    `evidence_id ev_f4b80862e103dfa593419f6bfa1ce62a542838585edea39f3752b4fd60fc50ec`.
- Prior triple-confirm (handoff `policechain-rea-20261012.md`, not re-run):
  `0x00406b80` (3676), `0x00407860` (614), `0x00407f30` (5040) pseudocode.
- Native helpers + goldens: `tests/police_test.cpp` (boundaries, imported config,
  `analysis/oracle/napa-police-spawn.tsv` numeric match, 1,024 `napa-police-arrest.tsv`
  outcomes with delay 180/0). Scratch detail: `analysis/police-20261016d/summary.md`
  (+ the two `rea-*.md`/`.log` outputs).
- Read-only LSP: `python3 tools/agent_lsp.py --check src/police.cpp` → 0 errors.

## Exact addresses and gating

- **Periodic spawn** (`0x00407f30` → `0x00406b80`): fires only when owner == 0,
  rider flag `0x10` clear, `DAT_004642dc == 0` (single-cop guard), and
  `(tick & 0xfff) == 0`. **No level guard.** Cop built `0x2700` Q8 behind the player
  via `0x00405990` (templates `0x464310/0x464378`, rider row 15 at `0x464038`,
  `param_5 = 2`), video-slot cycle 0–7, 10-column relationship reset + level-scaled
  rows, cop-becomes-owner, speed `+0x1a4` from `+0x1bc[level]`, bike from
  `0x4641c8+15*5+level`. Native mirrors: `police_spawn_due` / `make_police_actor`.
- **DZHR spawn** (`0x004520d0` → `0x004409e0` else-branch, type `word0 & 0xf` ≥ `0xb`):
  requires `DAT_004642d8 != 0`; two `rand() % 0x81` draws; ring slices
  `(r+0x200)·16` via `0x00449910` from the player's slice; then `0x00406b80`.
  Level gate for these records comes from `placed_scenery`'s `level − 4` probability,
  not from the spawn functions. Census: CITY 449 ped/12 cop, HIWAY 93/11,
  MEDLY 476/17, NAPA 85/20, CANYN 43/12; **Napa level 1 has 0 cops** — `napa-cops`
  must use level 3+ (2 records); CITY L1 has 6.
- **Pursuit/find** (`0x00407860`): two `0x00449ce0` passes (windows `0xa00`, then `0x500`);
  returns the cop bike only if `DAT_004642dc` matches and its `+0x2f4 == 0`
  (un-crashed); caller `+0xc0 & 0x10` vetoes. Cops are riders: rider think/move/draw,
  not the ped `0x00440dd0` machine. Native `+0x1a4` decay / `0xa00` vs `0x5000` caps
  are cop-only branches still open in `src/race.cpp` (`:1356`, `:1369`, `:1724`, `:1733`, `:1796`).
- **Arrest** (`0x00407f30` decision → `0x00416010(4, 0xb4)` busted): owner == 0,
  flags `0x10` clear on both halves, cop found and un-crashed, stopped
  (`bike_speed < 300`, or `|speed| < 1800` when `crash == 2`), `|cop−rider pos| < 512`,
  `|cop−rider speed| < 5000`. Race then runs `0xb4` more ticks (`0x00442e10`).
  Native: `police_arrest` (+ `0x0040a600` police-owner 1,200-tick exemption already in `src/crash.cpp:38`).
- **RNG order** (must be preserved): 1 draw per DZHR record; ped trigger `rand % 0x4fb1`;
  cop two `rand % 0x81`; no draws in `0x00406b80`/arrest/siren. Native
  `Race::spawn_pedestrian` (`src/race_pedestrians.cpp:85-93`) already consumes the two
  cop draws to hold the stream — the live port replaces the `return` with construction.

## Live-wiring plan (for the implementing worker)

1. **Factory spawn.** In `Race::spawn_pedestrian`'s `type >= 0xb` branch, call
   `make_police_actor` with `PoliceSpawnInput` from the ring slice (player-slice-derived
   position via `0x00449910` picks, lane, ground/road target from the slice, tick).
   Add single-cop guard (`DAT_004642dc` equivalent), scheduler/rider-list ownership,
   video-slot cycle, and route/scheduler links (all open; `make_police_actor` documents
   caller-owned ground/route). Keep draw count identical. Also port the `0x00407f30`
   periodic branch (`police_spawn_due` → same constructor, `0x2700` behind player).
2. **Pursuit.** Run the cop through the rider think/move/draw path with
   `police_pursuit` personality; port cop-only `+0x1a4`/`0x5000`/`0xa0`-window branches.
   Verify HUD `0x00438550` slot-14 (`DAT_004642dc`) inclusion for "nearest opponent".
3. **Actor contact.** Rider↔cop contact is the `0x004049c0` type-`0x20` branch
   (plan 16 Task 1); native bike-vs-bike contact exists (`src/race.cpp:948+`) but the
   cop-typed branch check is open. Sirens: native `src/sound.cpp:349` leaves siren as
   `default: break` (plan 05); wire event 19 → `0x00435ae0` dual-buffer siren
   (slots 30/31, no RNG) with `0x00435840` spatialization when cops go live.
4. **Career penalty + busted cutscene hook.** Outcome 4 flows through existing
   `finish_race` (`src/career.cpp:104-139`: screen `0x1d`, Big Game fine
   `(level*5+5)*0x50` messages 197/194/198, Thrash clears qualifications 193/196)
   → `Frontend::race_finished` (`src/frontend.cpp:917`) → `results_video` `BUSTED1–6`
   (`src/career.cpp:166`), already triggered from `game_main.cpp:381`. The worker must
   prove `end.outcome == busted` actually arrives from the live `0x00416010(4)` path
   (currently no live producer) — that plus the video trigger is the slice-D acceptance.

## Scenario capture spec (all open; proxy/pipeline still down per prior handoffs)

- `napa-cops`: level 3+ Napa run reaching DZHR cop records; dump cop bike/rider
  numeric fields (same field list as `police_test.cpp:42`), `+0x1a4`/relationship words.
- `napa-cops-crashed`: arrest while on foot (cop-owner timer exemption + `crash == 2`
  speed window); must reach `0x00416010` reason 4.
- `city-cops`: CITY L1 (6 cop records); must reach reason 4 and the `BUSTEDx` video choice.
- Accept with a single RNG anchor; one cop frame must match pixels (`oracle-scene`).

## Files written by this session (only these)

- `docs/agent-handoffs/police-spawn-20261016d.md` (this file).
- `analysis/police-20261016d/` — `summary.md`, `rea-4409e0-setup.md`/`.log`, `rea-407860-copfind.md`/`.log`.

## NOT changed (explicit)

No tracked file was edited. In particular, none of the live-claimed paths were
touched: `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`,
`docs/plans/24-road-hazards-police-crash-control.md`, `docs/plans/32-completion-review-next-steps.md`,
`src/race.cpp`, `src/traffic.*`, `src/terrain.*`, `src/race_setup.*`, the oracle proxy,
`CMakeLists.txt`, `tests/oracle_sim_test.cpp`, `tests/traffic_test.cpp`,
`tests/race_setup_test.cpp`. No build, capture, or commit was run.
 Baseline `git status` shows only pre-existing untracked `analysis/*`/`docs/agent-handoffs/*`
scratch from sibling sessions; nothing staged or modified by this worker.
