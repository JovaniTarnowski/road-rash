# Handoff: grid-setup port input — 0x00405990 audit vs native factory (October 14, 2026)

## Task / plan

Plan 10 Task 6 fix order #1 (grid setup port). Input to the fix owner; unblocks
tick-210 native-only bike-bike contact (`docs/agent-handoffs/sim-fault-2.md`:
lateral off by exactly one 960-unit nudge, speed off by shit/4 = 424 from native
S184 = −2262, native rng +2 draws from contact sounds; original shows no
nudge/speed-loss/draws).

Baseline commit: `5661068` + sim-fault-2's uncommitted edits (`src/race.cpp`,
`tests/race_test.cpp`, `tools/sim_known_divergences.json`,
`oracle/aweman_proxy.c`, `oracle/build.sh` — PRESERVED untouched).

## Owned files

READ-ONLY research: no source files touched, no file claims. This handoff doc
(`docs/agent-handoffs/grid-setup-20261014.md`) is the only write. No builds run,
no oracle captures run (SIM fault owner is debugging; prefix is shared).

## 1. Existing native grid-setup code (map)

- `src/race_setup.cpp:118-176` — `build_race()`: builds opponents via
  `original_opponents(exe, specs, level, solo ? 14 : opponents)`, freezes them
  when `config.solo`, constructs `Race` with `config.road_base_q8`, optional
  `prepare` hook (demo hold), RNG state, ring, terrain, traffic, crash/ride/nitro.
- `src/race.cpp:25-115` — `original_opponents()` + `read_grid_slot()`:
  personality `0x00464558 + 0x204*(k<9?k:0)`, bike
  `byte(0x004641c8 + level + k*5)`, rider row `0x00463c78 + 0x40*k`
  (value `+0xc / 2`, stats `+0x30..+0x3c`, weapon `+0x18 + level*4`), grid
  `read_grid_slot` (`0x00464088` level<4 else `0x00464108`), lane
  `byte(personality + 0x18c)` (NEVER −1, see §5 gap 3), throttle
  `word(0x004642a0 + level*4)`.
- `src/race.cpp:140-212` — `Race::Race` `place_rider` lambda: `sample/0x1c/0x138`
  = `grid_pos ? grid_pos : grid_position(slot)` (`0x3200`/`0x3300`);
  `0x28 = routes_[0][sample].offset*256 + lat + road_base_` with
  `lat = grid_pos ? grid_lat : grid_lateral[slot%4]`
  (`{16384,-3616,-23616,-43616}`); `brain.bias = lat`; `0x2f0 = 0x2ec`;
  AI throttle scale + gear softening (`0x260,%`, `0x1d0/0x1e0/0x1f0/0x200`);
  speeds from opponent struct; lane drawn ONLY if `o.lane < 0`
  (`random() % 3`, never taken today); brains from `personalities[]`.
- `src/race.cpp:123-131` — `race_road_base_q8()`: `golden − route_offset*256 −
  grid_word`, i.e. the `0x00405990` lateral formula solved for the anchor.
- `tests/oracle_sim_test.cpp:585-597` — frozen seeding stopgap (ONLY when the
  capture has a node dump): frozen `0x28` = frame-0 bike lateral, `0x30..+4k`
  previous copies synced, `half_detached` + `half_lateral` from half dump.
  Captures without node laterals keep native grid laterals (the
  `sim-napa-solo.tsv` case) and `0x30..0x44` stay 0 (see §5 gap 4).
- `tests/race_test.cpp:523,559`, `tests/race_setup_test.cpp` — factory tests
  (slots/riders 1..14).

## 2. REA spot-checks (pinned 6.4.0, Ghidra 12.1.4)

Env per `docs/TOOLING.md` (`node v24.13.0`, `GHIDRA_INSTALL_DIR=.../12.1.4/libexec`,
`JAVA_HOME=openjdk@21`); each call exit 0 on the hash-verified EXE:

- `0x00405990` — pseudocode IDENTICAL to `analysis/decompiled/00405990.c`
  (103 lines, char-for-char incl. the `DAT_00463ad0/ad4` pair build,
  `rand()%3` lane, `0x00407990` brain, rider-row select, `0x00405c80` +
  `0x00405ea0`, param_5 1/2 throttle/gear, `0x00446060` x2 + `0x0043ae90`,
  `+0x2b` clears). Assembly cites: pair build `0x4059a9-0x4059d4`
  (`MOV EAX,[EBP+0xcc] / ADD EAX,ESI` then `CALL 0x004387a0` twice);
  lane draw `0x405a69-0x405a76` (`CALL 0x00441750 / IDIV ECX=3`);
  `0x00407990` call `0x405a76-0x405a78`; `0x00405c80`/`0x00405ea0` tail calls.
- `0x00439390` (rider-commit) — IDENTICAL to `analysis/decompiled/00439390.c`
  (62 lines): `+0x134`/`+0x1c`/`+0x138` commit (`0x439395-0x4393a7`),
  same-sample early return (`TEST EAX,0xffffff00 / JZ 0x004394ba`),
  forward/backward slice walk with the `+0x28>>8 vs +0xe4` edge test
  (`0x4393e7-0x4393f5`), player-only `0x004540e0` fork, tail
  `+0x140`/`+0x2c` update + `+0xc4==4` cache `+0x304/+0x308`.
  Relevance: NOT a gap — native `cross_sample`/`move_along` already match
  (plan10-contact-tail audit); no edit proposed here.
- `0x00438610` (scenery half-dispatch) — IDENTICAL to
  `analysis/decompiled/00438610.c` (57 lines: `0x438610-0x438790`).
  Relevance: none for grid setup — scenery only, no rider words; checked to
  close the task's "if relevant" item. No edit.

Prior REA (placement-chain-rea-20261012, same pinned toolchain):
`0x004387a0`, `0x00439b10` whitespace-identical to project exports; placement
chain `0x00405990` lateral = `*(slot+0xcc) + grid`.

## 3. Exact addresses and measured table values (EXE reads, this session)

- Templates: `0x00463ba8` bike behaviour table (26 words; words 0-9 =
  `3,4,1,0x446250,0x407a90,0x4060b0,0x406940,0x12c,0x100,0x82`),
  `0x00463c10` rider behaviour table (words 0-9 =
  `0,8,1,0x4463e0,0x434660,0x406890,0x406b40,0x12c,0x100,0x82`).
  `0x00405990` does NOT pass these as param_1/2 — it passes the object images
  AT `0x00463ad0` (bike) / `0x00463ad4` (rider) for the offline path
  (`00405990.c:15-16`), `0x00463ad8`/`0x00463b40` for the AI path
  (`00405600.c:106`), `0x00464310`/`0x00464378` row 15 for cops
  (`00406b80.c:45`). `init_bike_object` (`src/bike.cpp:74-84`) covers the
  `0x00463ba8` words 7.. only — the object-image words come through
  `0x00438930`/`0x004387a0` field copies, which are ported as constants.
- Personality `0x00464558` (10 × `0x204`): row 0 `+0x18c=0, +0x190=0,
  +0x1a8=70, +0x1bc=87`; row 1 `+0x1bc=99`. Bike `0x004641c8`
  (level + rider*5): level 0 rider 0/1/2 = `1,1,2`. Rider `0x00463c78`
  (`0x40`/row): row 0 `+0xc=180` (value 90), `+0x18=0`,
  `+0x30..+0x3c = 7/2500/2500/2500`; row 1 `+0xc=215`.
  Throttle `0x004642a0` = `116,116,114,114,114`.
- Start grid `0x00464298 → 0x00464218` (8 bytes/level: lateral, position):
  L0 `(16384,0x3200)`, L1 `(-3616,0x3200)`, L2 `(-23616,0x3200)`,
  L3 `(-43616,0x3200)`, L4 `(16384,0x3300)`.
  Native `grid_lateral[4]` == the LATERAL COLUMN of these rows — correct for
  level 0 only; levels 1-4 player bias/placement is wrong (gap 5).
- Full grid `0x00464088` (level<4, 8 bytes/slot: lateral, position):
  slots 0-3 lateral ALL 16384, positions `0x3200,0x7b00,0x3600,0x3800`;
  slots 4-15 mix laterals `40960/16384/8192/36864/20480/0` (measured this
  session; e.g. s4 `(40960,0x3400)`, s6 `(8192,0x6c00)`, s15 `(0,0x5800)`).
  `0x00464108` (level≥4): s0-3 `(16384, 0x3200/0x7600/0x4400/0x4e00)`.
  Native `read_grid_slot` reads these correctly — the gap is the ANCHOR, not
  the table (§5 gap 1).
- Callers of `0x00405990`: `0x00405600` player (param_5=0, templates
  ba8/c10, slot from `0x0049e820` via `0x00464298`, `00405600.c:65-71`) and
  AI loop (param_5=-1 via `0x0049e820` for `local_80==-1` rows, templates
  ba8/c10, `00405600.c:88-95`; else param_5=1 via `0x00464088/0x00464108`,
  templates ad8/b40, `00405600.c:106-109`); `0x00406b80` cop (param_5=2,
  templates `0x464310/0x464378`, row 15, `00406b80.c:45`).
- Placement rule: bike/rider `+0x1c..` lateral = `*(slot + 0xcc) + param_4`
  (`00405990.c:15-16`; `0xcc` = absolute Q8 sub-slice centre from
  `0x00439b10`). The "grid + position − 0x2b00" shorthand in
  `sim_known_divergences.json`/cc-plan10-task5 is the OBSERVED frame-0
  relation for rows 50-54/57 (bike lateral = grid word + (position − 0x2b00));
  other rows add a lane or more — field not traced. Native uses
  `route.offset*256 + grid + road_base_` (`race.cpp:158`): equal to the
  original at exactly one anchor sample, off elsewhere by
  (sub-slice-centre − route-offset) deltas.

## 4. Frozen-word audit: stale pool vs init (bike object, BYTE offsets)

Chain order `0x00438930` → `0x004387a0` → `0x00405990` body → `0x00405c80` →
`0x00405ea0`. Word-index notation in decompile = byte/4.

| Byte off | Written by | Value at build end | Native | Match? |
|---|---|---|---|---|
| `0x108` (fwd vel) | NOTHING in chain | STALE pool (`004387a0`/`00405990` never write it; contact-tail handoff proof: orig ≈321/6417) | 0 (`Bike{}` zero; frozen never run `move()`, `race.cpp:1765`, so stays 0) | NO — impulse source |
| `0x10c` (lat vel) | NOTHING | STALE pool | 0, same reason | NO — impulse source |
| `0x110` (vert vel) | NOTHING | STALE pool | 0 | NO — latent (`0x18c` impulse source) |
| `0x3c` (prev-lateral copy) | `00438930.c:39-43` (lateral into `[0xc..0x10]` = bytes `0x30..0x40`) | INIT = build lateral | 0 when unseeded; seeded value when node dump present (`oracle_sim_test.cpp:594`) | CONDITIONAL — see §6 |
| `0x104` | `004387a0.c:57` (`[0x41]=0`) | INIT 0 | 0 | yes |
| `0x114`/`0x118` | `004387a0.c:41` (`[0x45]=[0x46]=0`) | INIT 0 | 0 | yes |
| `0x60` | `00438930.c:46` (`[0x18]=0`) | INIT 0 | 0 | yes |
| `0xf0` | `004387a0.c:36` (`[0x3c]=0`) | INIT 0 | 0 | yes |
| `0x100` | `004387a0.c:57` (`[0x40]=0`) | INIT 0 | 0 | yes |

So of the task's candidate list, ONLY `0x108`/`0x10c` (+`0x110`) are truly
stale; `0x114`/`0x118`/`0x60`/`0xf0` are explicitly zeroed by the chain and
already match native. `0x3c` is initialized (not stale) but NATIVE FAILS TO
WRITE IT in `place_rider` — same observable consequence as staleness when the
harness cannot seed it. Lateral gate `00436a40.c:122-124` reads `+0x3c`
(verified: native `race.cpp:410` is faithful), while the nudge writes `+0x28`
(lines 125-141) — so a wrong `0x3c` changes contact EXISTENCE, and wrong
`0x108/0x10c` change contact MAGNITUDE (`0x184/0x188` impulses,
`00436a40.c:109-116,142-149`) and hence speed loss + sound draws.

## 5. Native gaps vs `0x00405990` (ordered = fix order)

1. Lateral/position anchor (THE tick-210 trigger): `place_rider`
   (`race.cpp:149-158`) uses route-offset + table lateral + road_base instead
   of sub-slice `+0xcc` + grid offset. Road_base absorbs it at one sample;
   every other slot is off by centre-vs-offset deltas. Frozen rows are dropped
   from solo comparison (`oracle_sim_test.cpp:644-651`), so the error is silent
   until the player rides into a misplaced frozen bike (tick 210, slots 0v2).
2. Previous-position copies `0x30..0x44` never written by `place_rider`
   (only `0x1c/0x138/0x28`). Original has build lateral in `0x3c` from the
   first instruction. Harness patches it only with a node dump.
3. Setup RNG draws missing: original draws `rand()%3` lane per `0x00405990`
   call INCLUDING the player (`00405990.c:36-43`, 15 draws for 1+14) plus
   `rand()%100` tail (`00405600.c:149-150`); `0x004078f0` draws 10×5 jitter
   earlier (outside race stream). Native draws ZERO (`original_opponents`
   takes lane from the personality byte; `Race` draws only when `lane<0`,
   never). Covered today by the single green anchor; a from-`srand(1)` setup
   must reproduce draws in order: 50 jitter → ring (`0x004552e0`, before
   `0x00405600` per `0x0044b250`) → 15 lane → 1 tail. (Known tension:
   cc-plan10-task5 notes 16 setup draws > 11 measured post-ring draws.)
4. `param_5=-1` vs `1` opponent paths (`00405600.c:87-110`): `local_80==-1`
   rows build with ba8/c10 templates and NO throttle/gear scale; others with
   ad8/b40 + scale (`00405990.c:76-82`, also ported at `race.cpp:177-178` for
   ALL opponents). Native applies scale + `original_opponents` tables to every
   opponent and always uses ba8/c10-equivalent SPEC path. `FUN_004317a0`
   (`local_80` source) not traced — open which slots take which path in an
   offline race.
5. Player slot: native hardcodes slot 0 + level-0 grid laterals; original
   player slot comes from the `0x0049e820` ring via `0x00464298` +
   `0x00464218[level]` words (bias = level lateral: only L0 = 16384 matches).
   Rider param_6 = `DAT_00467a6c`, param_7 = `DAT_004b8a10` (level),
   `+0xca` = level (`00405600.c:65-72`); native player rider_idx = 0.
6. Cop path (`0x00406b80.c:45-58`) not ported (templates `0x464310/0x464378`,
   row 15, param_5=2, relationship reset) — police slice owns it; listed so
   the grid port doesn't collide.

## 6. Tick-210 mechanism (synthesis)

Native frozen slot-2 bike sits at the route-offset anchor approximation while
the original sits at sub-slice-centre + grid (off by the §3 deltas, observed
as the `position − 0x2b00` relation). The `0x00436a40` lateral gate reads
`+0x3c` (prev lateral): native's `0x3c` is 0-or-grid-approx (unseeded era),
original's is the true build lateral. At tick 210 the player (slot 0)
laterally overlaps native's copy but not the original's → native-only
`separate()` nudge (exactly one 960-unit push, `33427 vs 32467`), then
`contact()` speed loss `shit/4 = 424` (native S184 = −2262, scaled by
`0x180` = 192) and exactly 2 RNG draws (contact sounds `0x00436830`),
all absent originally. Stale `0x108/0x10c` shape the impulse magnitude but the
contact EXISTENCE comes from the `0x3c`/placement error — hence grid setup is
fix order #1, and a current-era SIM capture with frozen bike-word coverage
(`0x108/0x10c/0x110/0x3c` for frozen nodes) is the alternative evidence path.

## 7. Minimal next code edit (proposal only — no code changed)

In `src/race.cpp` `place_rider` (+ `original_opponents` as needed), keeping
synthetic-route behaviour byte-identical when `grid_pos == 0`:

a. After computing `pos`, sync previous copies
   `0x30..0x44 = 0x1c..0x2c` (mirrors `00438930.c:39-43` + `004387a0`
   slot words; one loop, no RNG). Fixes the `0x3c` gate for ALL captures,
   seeded or not.
b. Replace the `grid_pos ? ... : synthetic` lateral with
   sub-slice-centre + grid-word when EXE data is available (thread the
   `CourseScene`/terrain slice centre or the `0x0049e820`-equivalent through
   `Opponent`), keeping `road_base_` as the anchor. Player first
   (`0x00464218[level]`), then AI slots. Verify against frame-0 frozen
   laterals of `sim-napa-solo-human.tsv` before touching RNG.
c. (Separate commit) Lane-draw stream: set `Opponent.lane = -1` for race
   builds and draw `random()%3` in build order inside the `Race` ctor
   (player first, then slots 1..N), documenting the 50-draw jitter + ring
   ordering dependency; do NOT reanchor goldens until (b) lands.
d. Do NOT touch `0x114/0x118/0x60/0xf0/0x104` (proven init-0 both sides),
   `0x00439390`/`cross_sample` (faithful), or `0x00438610` (irrelevant).
   Do NOT invent frozen `0x108/0x10c` values — needs the SIM capture; note
   contact-tail's orig ≈321/6417 estimate as the only current numbers.

## Checks

- `git status --short`: sim-fault-2's 5 modified files + pre-existing
  untracked dirs/files preserved; only addition is this handoff.
- REA: 3/3 exit 0, pseudocode identical to `analysis/decompiled/` exports
  (`00405990`, `00439390`, `00438610`).
- EXE table reads: `0x00464218` levels, `0x00464088` slots 0-15,
  `0x00464108` slots 0-3, `0x00464558` rows 0-1, `0x004641c8`,
  `0x00463c78` rows 0-1, `0x004642a0`, `0x00463ba8`/`0x00463c10` — all via
  read-only `python3`, no captures, no builds.
- NOT done: `0x004317a0` (`local_80`, −1/1 path select), `0x00446060` ×2 +
  `0x0043ae90` anim-init port check, cop build, any code edit — all for the
  fix owner.

## Live resources

No live processes, no builds, no locks, no captures. REA temp files under
`/private/var/folders/.../T/opencode/rea-*.md` (outside repo; disc-derived
pseudocode was NOT copied into `analysis/oracle/` to avoid git-ignored
churn — rerun the §2 commands to reproduce).
