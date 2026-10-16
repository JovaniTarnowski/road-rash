# Spawn-chain background — grid setup / rider slots (Plan 10 Task 6 fix order #1)

Task: READ-ONLY background for Plan 10 Task 6 Step 2 fix order #1 (grid setup at
tick 0: rider slots, values, personalities). Covers `0x00405600` player build,
`0x00405990` bike+rider pair, opponent slots 1..14. Input to the fix owner only;
no code changed.

Baseline: `d6ebde0` (HEAD at session start; tree carries sibling sessions'
uncommitted edits throughout — none touched).

## Owned writes (only these two paths)

- `docs/agent-handoffs/spawn-chain-20261016k.md` (this file)
- `analysis/spawn-20261016k/` — scratch: two pinned REA `--format md`
  dossiers + empty `.log` sidecars (disc-derived pseudocode, LOCAL ONLY,
  never commit; `analysis/spawn-20261016k/` is not in `.gitignore`, so the
  integrator must not stage it):
  - `rea-407990-brain.md` — `0x00407990` brain/personality bind
  - `rea-438930-init.md` — `0x00438930` object init (previous-position copies)

## 1. REA spot-checks (pinned toolchain, 2/2 allowed calls used)

Env per `docs/TOOLING.md`: `PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`
(node v24.13.0), `GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=openjdk@21`. Both calls exit 0 on the hash-verified EXE
(`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`,
matches `analysis/inventory.json` and the task pin). Provider Ghidra 12.1.4,
`rea-agents@6.4.0`, `import_mode: ephemeral-source-immutable`.
Whitespace-normalized comparison against `analysis/decompiled/`:

| Function | evidence_id | REA chars | Export chars | Match |
| --- | --- | --- | --- | --- |
| `0x00407990` | `ev_d8314133f2d1f39e4af3b11b33c3e4207c7f04b3d13b31422ad2bdb02d1e44f1` | 1264 | 1267 | TRUE |
| `0x00438930` | `ev_02e45e7142a38a7a469a0211728c20d106598ebd508d12ebad825f8bb4ce9377` | 1258 | 1261 | TRUE |

Assembly spot cites (from the dossiers' complete-body reference sets):
- `0x00407990` range `0x407990-0x407a81`: `CALL 0x00414910` (`0x204` alloc,
  `0x4079c9-0x4079ce`), `CALL 0x00448080` (fatal, `0x407a05-0x407a0a`),
  personality select `LEA [ECX*0x4+0x4c6b40]` (`0x4079e9`, ×0x81 words),
  cop `LEA [ECX*0x4+0x4c4cfc]` (`0x407a47`), lane write `[EAX+0x18c]`
  (`0x407a64`), grid-word write `[EDX+0x190]` (`0x407a78`; 0x190 = 400).
- `0x00438930` range `0x438930-0x438a0f`: pool alloc `CALL 0x00438c10`
  (`0x438936`; project decompile names the thunk `thunk_FUN_00438a80` —
  same target, naming only), lateral `[EBX+0x1c] = *(*(EDI+0x28)+8)+param_4`
  (`0x4389c7-0x4389d3`), sample `CALL 0x00439b10` (`0x4389dd`),
  previous-copy `MOVSD.REP ×5 [EBX+0x30]` (`0x4389ec-0x4389f7`), tail
  `CALL 0x00437bb0` (`0x438a04`). No `rand()` in either function.

Why these two: both were uncovered by prior REA (405600/4078f0/441750,
44df60/44dd90, 44db80/44dd00/4552e0, 4520d0/4409e0, 405990/439390/438610,
4387a0/439b10 all done) and both sit on the Task 6 fix-order critical path —
`0x00407990` binds values/personalities per pair, `0x00438930` writes the
`0x3c` lateral gate word that decides contact EXISTENCE at tick 210.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof.

## 2. Slot order (original, `analysis/decompiled/00405600.c`)

1. `FUN_004317a0(&local_80)` fills the −1/1 path selector; `0x004642d4`
   rider count from mode (`0x10` solo?/offline, 9, `0x00431840()+1`).
2. Player FIRST: `FUN_00405990(&DAT_00463ba8, &DAT_00463c10, <slot from
   `0x0049e820` ring via `PTR_DAT_00464298` + `0x00464218[level]` word>,
   grid lateral, param_5=0, rider_idx, level)` (`00405600.c:65-72`).
3. Loop `iVar2 = 1 .. DAT_004642d4-2`, per slot:
   - `local_80[i] == -1` → param_5=-1 path, templates ba8/c10, grid from
     `PTR_DAT_00464298` (ring-selected lateral/position, no throttle/gear
     scale); rider brain `FUN_00407d40`.
   - else → param_5=1 path, templates ad8/b40, grid from `0x00464088`
     (level<4) / `0x00464108` (level≥4) by slot, WITH throttle/gear scale.
   - `DAT_004c7f70/DAT_004c7fb0[i]` = bike / `piVar5[0xbf]` rider.
4. Tail: `_rand() % 100` → `FUN_00402e10` (one draw).

`0x00405990` body order per pair: `0x004387a0` ×2 (bike image `0x00463ad0`,
rider `0x00463ad4` offline; ad8/b40 AI; `0x464310/0x464378` row 15 cops) →
`rand()%3` lane (offline only) → `0x00407990` brain → rider-row select
`0x00463c78 + 0x40*param_6` → `0x00405c80` + `0x00405ea0` → param_5 1/2
throttle/gear scale → `0x00446060` ×2 + `0x0043ae90` → `+0x2b` clears
(param_5 0/1-gated).

## 3. Table words (carried from grid-setup-20261014 EXE reads; not re-measured)

- Templates: `0x00463ba8` bike words 0-9 =
  `3,4,1,0x446250,0x407a90,0x4060b0,0x406940,0x12c,0x100,0x82`;
  `0x00463c10` rider words 0-9 =
  `0,8,1,0x4463e0,0x434660,0x406890,0x406b40,0x12c,0x100,0x82`.
  `0x00405990` passes object images (`0x00463ad0/ad4` offline,
  ad8/b40 AI, `0x464310/0x464378` cops), never these tables directly.
- Personality `0x00464558` (10 × `0x204`): row 0 `+0x18c=0, +0x190=0,
  +0x1a8=70, +0x1bc=87`; row 1 `+0x1bc=99`. Bike `0x004641c8`
  (level + rider*5): L0 rider 0/1/2 = `1,1,2`. Rider `0x00463c78`
  (`0x40`/row): row 0 `+0xc=180` (value 90), `+0x18=0`,
  `+0x30..+0x3c = 7/2500/2500/2500`; row 1 `+0xc=215`.
  Throttle `0x004642a0` = `116,116,114,114,114`.
- Start grid `0x00464298 → 0x00464218` (8 B/level): L0 `(16384,0x3200)`,
  L1 `(-3616,0x3200)`, L2 `(-23616,0x3200)`, L3 `(-43616,0x3200)`,
  L4 `(16384,0x3300)`.
- Full grid `0x00464088` (level<4, 8 B/slot): s0-3 lateral all 16384,
  positions `0x3200,0x7b00,0x3600,0x3800`; s4-15 laterals
  `40960/16384/8192/36864/20480/0` (e.g. s4 `(40960,0x3400)`,
  s6 `(8192,0x6c00)`, s15 `(0,0x5800)`). `0x00464108` (level≥4):
  s0-3 `(16384, 0x3200/0x7600/0x4400/0x4e00)`.

## 4. Native-vs-original table (native via read-only `rg`, this session)

| # | Item | Original | Native (`src/race.cpp`, `src/race_setup.cpp`) | Verdict |
| --- | --- | --- | --- | --- |
| 1 | Player build | slot from `0x0049e820` ring + `0x00464218[level]`, param_5=0 | `place_rider(spec, rider_value, 0, 0, 0)` — hardcoded slot 0, L0 laterals (`race.cpp:171`) | GAP (bias wrong L1-4) |
| 2 | Opponent slots 1..14 | `local_80==-1` → param_5=-1/ba8-c10/noscale vs param_5=1/ad8-b40/scale | all opponents via `original_opponents` SPEC path + scale (`race.cpp:26-109,177-178`) | GAP (`0x004317a0` untraced) |
| 3 | Lateral anchor | `*(slot+0xcc) + param_4` sub-slice centre + grid (`00405990.c:15-16`) | `route.offset*256 + grid + road_base_` (`race.cpp:158`); anchor exact at one sample only | GAP (tick-210 trigger) |
| 4 | Previous copies `0x30..0x44` | `0x00438930` writes `[0x30..0x40]` = build lateral (REA-confirmed §1); `0x004387a0` slot words | `place_rider` writes only `0x1c/0x138/0x28`; harness seeds `0x3c` only with node dump | GAP (contact existence) |
| 5 | Brain bind | `0x00407990`: `+0x1d8` = `0x004c6b40+0x81*r` (r<9) / `0x004c7d64` (r=15) / `0x004c4cfc+0x204*r` +mark; `+0x18c`=lane, `+0x190`=grid (REA-confirmed §1) | `race.cpp:93` comment already encodes the r<9 branch; brains from `personalities[]`; lane from personality byte, never −1 so never drawn | PARTIAL (lane draws missing) |
| 6 | Setup RNG | 50 jitter + 15 lane + 1 tail + per-pair DZHR/trigger (see §5) | zero draws at setup | GAP (green anchor covers) |
| 7 | Init zeroes | `0x104/0x114/0x118/0x60/0xf0/0x100` explicitly zeroed (`00438930/004387a0`) | zero via `Bike{}` | MATCH |
| 8 | Stale words | `0x108/0x10c/0x110` never written in chain (stale pool) | 0 (frozen path never runs `move()`) | GAP (impulse magnitude) |
| 9 | Cop path | `0x00406b80` param_5=2, row 15 | not ported | OUT OF SCOPE (police slice) |
| 10 | Rider-commit | `0x00439390` (REA-identical, prior session) | `cross_sample`/`move_along` faithful | MATCH, do not touch |

## 5. Setup-RNG accounting note (11,461 pre-green calls)

The 11,461-call pre-green stream (per `REVERSE_ENGINEERING.md` rider-build
and TrackRing audits, `startchain-rea-20261012`): bulk = track-ring slice
build / BORR / DZHR iterator draws (`0x00454c80`, `0x00453030`; K = 2363
draw-count evidence in `track_ring_test`/`ring_probe`); rider-building head
in strict order = 50 `0x004078f0` jitter draws (`rand%0xd−6`) → ring fill
`0x004552e0` (ZERO draws, REA-proven) → per-pair `0x00405990` lane `rand%3`
(player/personality path skips) + per-pair DZHR/trigger draws in record order
→ `0x00405600` tail `rand%100` → `0x00402e10`. `0x00407990` and `0x00438930`
draw nothing (REA-confirmed §1: only `0x00414910` alloc / pool / sample calls).
Single-anchor-at-green stays the acceptance rule until a from-`srand(1)`
harness ports ALL draw sites in order. Known tension for the fix owner:
cc-plan10-task5 notes 16 setup draws vs 11 measured post-ring draws
(grid-setup-20261014 §5 gap 3) — unresolved.

## 6. NOT-changed list

Tracked files: `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`,
`docs/VERIFICATION.md`, `docs/plans/10-simulation-oracle.md` (no plans-32
file touched), `src/race.cpp`, `src/traffic.*`, `src/terrain.*`,
`src/race_setup.*`, oracle proxy (`oracle/aweman_proxy.c`, `oracle/build.sh`),
`CMakeLists.txt`, `tests/oracle_sim_test.cpp`, `tests/traffic_test.cpp`,
`tests/race_setup_test.cpp` — none read-for-edit, none modified.
No builds run, no captures run (shared prefix untouched), no commits.
`git status` after this write must show only this handoff (modified/new) plus
the pre-existing sibling-session dirt plus `analysis/spawn-20261016k/`.

## 7. Remaining work (fix owner)

- `0x004317a0` (`local_80` −1/1 path select): which slots take which path in
  an offline race — still untraced, still open.
- `0x00446060` ×2 + `0x0043ae90` anim-init port check; `0x00405c80` /
  `0x00405ea0` tail; cop build `0x00406b80` — still uncovered by REA (this
  session spent its full 2-call budget on §1).
- Grid port itself per grid-setup-20261014 §7 (previous-copy sync, sub-slice
  anchor, lane-draw stream) — unchanged, still the task's fix order #1.

## Live resources

No live REA/Ghidra/Wine processes. No builds, no locks, no captures.
REA dossiers are local under `analysis/spawn-20261016k/` (disc-derived —
do not commit, do not copy into `analysis/oracle/` churn).
