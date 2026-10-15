# Handoff: bike-lateral divergence trace — the bike 0x28 formula (October 15, 2026)

## Task / plan

Plan 10 Task 6 fix order #1, gap (b): resolve the true per-sample/per-rider
centre term behind the tick-210 native-only contact. Baseline: `fd64e2e`
(GRIDPROBE table) on top of `4d1b343` (move-audit). READ-ONLY: no source
files changed, no oracle captures run (SIM fault owner debugging; prefix
shared). This handoff is the only write:
`docs/agent-handoffs/bike-lateral-20261015.md`.

Evidence: `analysis/decompiled/*.c` first, every load-bearing point
re-checked against `analysis/original-disassembly.txt` (element scaling,
`0xfa` multiplier, IDIV 3). REA spot-check skipped as redundant: the
deliverable below is verified against the authoritative disassembly, which
is what REA would only re-render (per AGENTS.md, disassembly outranks
pseudocode). Golden: `analysis/oracle/sim-napa-solo-human.tsv` frame 0
(tick 1). EXE grid table read from the reference binary with python3
(`struct`, no build).

## (a) The bike lateral formula with addresses

### Identity: which object is which (0x00405990, `analysis/decompiled/00405990.c`)

- Disassembly `4059a9-4059d4`: `movl 0xcc(%ebp),%eax; addl %esi,%eax` is
  passed to **both** `0x4387a0` calls — first with `ecx=0x463ad0`
  (decompile lines 15-16). So bike and rider-half nodes start with
  **identical `0x28`** = `*(slot+0xcc) + param_4`.
- `405a06`: `movl %ebp,0x2fc(%ebx)` = decompile line 23
  (`piVar1[0xbf]=piVar2`). Golden cross-check: every type-4 node has
  `half (+0x2fc)` nonzero, every type-8 node has `half == 0`. Therefore
  **`piVar1` = bike (type 4), `piVar2` = rider-half (type 8)** — not the
  reverse.
- Consequence: rider-half `0x28` is **never rewritten** after build, so it
  keeps `*(slot+0xcc)+param_4`. The golden's 15 type-8 laterals equal the
  EXE grid words exactly (§b table), fixing that sum empirically.

### The divergence point (deliverable): 0x00405c80 line 31, fed by 0x004091f0

- `0x00405c80` (`00405c80.c:31`, the only writer of bike `0x28` in the
  whole build chain — `0x004387a0`/`0x00438930`/`0x00407990`/`0x00432b40`/
  `0x00405ea0`/`0x004082f0`/`0x00407410` never touch it):
  `*(bike+0x28) = *(*(bike+0x2fc)+0x1d8)+0x194` (brain `+0x194`).
- `0x004091f0` (`004091f0.c:13-35`, byte-for-byte confirmed at
  `4091f0-409298`) computes that word just before (called at `405da3`
  inside the build path):
  - `cVar1` = brain `+0x18c` = lane byte, **signed** (`movsbl`, `409202`).
  - `iVar5` = `*(*(bike+0x2c)+0x10)` = course record one slice ahead
    (bike `+0x2c` is the slice pointer; the per-tick re-seat `0x004060b0:245`
    stores the ring-walk slice there — see below).
  - lane 0 → `iVar7 = 0`; lane 1 → `iVar7 = (sar8(*(iVar5+0x2c)) - 1) / 2`;
    lane 2 → `iVar7 = |sar8(*(iVar5+0x2c)) - 1|` (lanes from the record's
    Q8 width word). Else → 0.
  - `brain+0x198 = bike+0x1c + 0x3c0`;
    **`brain+0x194 = sar3(FUN_00449b40(iVar5,8)) + (*(iVar5+0xcc) + iVar7*0xfa)*0x100 + brain+0x190`**.
  - The `*0xfa` is real, not a decompiler guess: `409271-40928c`
    multiplies by 125 then `leal (%edx,%ebp,2)` = `+ iVar7*250`.
- Lanes come from the per-rider draw at `0x00405990:37-38`
  (`405a69-405a76`: `call 0x441750; cltd; mov $0x3,%ecx; idivl %ecx` —
  **`rand()%3`**, edx passed to `0x00407990`), stored to brain `+0x18c` by
  `0x00407990:39`. Offline only (netplay forces 0). Draw order: player
  first (`0x00405600:65`), then opponents 1..14 in loop order — one draw
  per rider, so **per-rider jumps are expected and the continuity objection
  in move-audit §6 is dissolved**.
- `brain+0x190` (`0x00407990:40`) = `param_4` of `0x00405990` = the grid
  lateral from the `0x00405600` call site (player golden proves it:
  `rider.0.brain_190 = 16384` = EXE grid word, and
  `rider.0.brain_18c = 1` = player's lane draw).

So, with `curv8 = FUN_00449b40(sliceNext,8)` (sum of 8 `+0xb4` bends,
`00449b40.c:12-20`, `/8` with `sar` rounding per `40926d-40927d`):

**`bike.0x28 = sar3(curv8) + (sliceNext[0xcc] + laneIdx*250)*0x100 + grid_lat`**

`laneIdx ∈ {0, (lanes-1)/2, |lanes-1|}`, `lanes = sar8(sliceNext[0x2c])`.

Native `ai_straight_target` (`src/race.cpp:1513-1525`) already implements
exactly this shape (`(route[next].offset + lane*0xfa)*0x100 + ai.bias`,
`bias` = grid lat) — but native `place_rider` (`src/race.cpp:158`) sets
`0x28 = offset*256 + grid + road_base_`, missing `sar3(curv8)`,
`sliceNext[0xcc]*256` (vs relative `offset*256`), and the lane term.

### Second copy point (per-tick): 0x004060b0 lines 253-256

`0x004060b0` (per-tick rider slice re-seat) re-derives position/lateral
from the ring (`piVar7 = FUN_00449910(...)`, `param_1[10] = piVar7[0x33]`),
re-stores the slice (`param_1[0xb]`), calls `0x004091f0`, and copies
`param_1[10] = brain+0x194` **twice** around the overtake call
`0x00408e40` (lines 253-256). It fires only when the rider's `0xc2` is
inside the current slice window (lines 211-217 early-return otherwise).
Smoking gun that this is real: **player `rider.0.lateral (0x28) = 18176`
but `rider.0.brain_194 = 10240`** — the brain target was recomputed
during tick 1 (via a `0x004091f0` caller such as the `0x004090xx` curve
path, which writes the brain without copying to `0x28`) while the re-seat
did not fire, so `0x28` kept its build value. Brain `+0x194` is a scratch
target; `0x28` syncs only at build and in the re-seat.

### 0x00405600 caller paths (template / param_5 per solo slot)

- Player: `FUN_00405990(&DAT_00463ba8, &DAT_00463c10, ..., 0, ...)` (line
  65-71), `param_5 = 0`.
- All 14 solo opponents: `local_80` is zeroed (lines 50-54), then
  `FUN_004317a0`'s **offline** second loop (lines 35-44; its netplay first
  branch requires `DAT_004c5b00/0x4c5af9`) fills entries `1..14` with
  `(flag=1, char=i)` — `FUN_00431840` returns 1 offline (scans an empty
  netplay mask `0x00497304-0x00497404`, returns `0+1`). Since no entry is
  `-1`, **every solo opponent takes the `param_5 = 1` grid path**
  (lines 106-109, templates `DAT_00463ad8/DAT_00463b40`); the `param_5 =
  -1` path (lines 88-95) is netplay-only and never fires in solo.
- Opponent build slot `i` (1..14) reads grid slot `i` (`iVar4+0x464088`,
  lines 98-105) — confirmed against the EXE table below.

### 0x00454240 fields +0x34/+0x38/+0x3c and native equivalents

(`00454240.c:24-55`; record stride 500 confirmed at `00454c80.c:269`:
`(param_2+4)*500`):

- `[0x33]` = centre from input `param_2+0xc` (line 24).
- `[0x34]/[0x35]` = centre ± slope-skewed edges
  (`sar8(-255-[0xc])*250`, `sar8([0xb]+255)*250`, lines 37-41);
  `[0x36]=[0x34]`, `[0x37]=[0x35]`.
- **`[0x38]` = `[0x33] + sar8([0xc]*-250)` (line 47-48), `[0x39]` =
  `sar8([0xb]*250) + [0x33]` (49-50)** — left/right edges;
  `[0x31] = [0x39]-[0x38]` width (51); `[0x3a]/[0x3b]` =
  shoulder-extended edges (54-55).
- `[0xb]/[0xc]` come from `FUN_004528f0` (line 34) — lane counts;
  `src/course.cpp:90` documents it reports `0x200` (two lanes each side).
- Native equivalents (`src/course.cpp:92-99`): `left_q8/right_q8` (Q8,
  `q16/256`), `left/right` (world, `q8*250/256` — the same 250), centre
  `RouteSample::offset` (world) anchored by `road_base_`. The `0xfa`
  in `0x004091f0` is that lane width. **Caveat (do not force):**
  `sliceNext[0xcc]` (absolute record centre word) has no proven-absolute
  native twin yet — native `offset` is relative/bend-integrated and reads
  0 at every grid sample. Whether `[0xcc]` equals
  `offset*256 + road_base_` or carries its own absolute term is the
  remaining numeric unknown for the sane-slot residuals (§b).

## (b) 15-point verification table (frame 0, tick 1)

Grid words re-read from the reference EXE (`0x00464088`, level<4) — they
match the type-8 laterals 1:1 and confirm the slot mapping
(slot `i` → grid slot `i`; slot 15 `(0, 22528)` unused in solo):

| slot | pos | grid (=type-8) | bike (type-4) | Δ vs grid | Δ vs native¹ | hex(bike) |
| --- | --- | --- | --- | --- | --- | --- |
| 0 (player) | 12800 | 16384 | 18176 | — (grid+1792) | 0 | 0x4700 |
| 1 | 31488 | 16384 | 15360 | -1024 | -2816 | 0x3c00 |
| 2 | 13824 | 16384 | 19200 | +2816 | +1024 | 0x4b00 |
| 3 | 14336 | 16384 | 83712 | +67328 | **+65536** | 0x14700 |
| 4 | 13312 | 40960 | 43264 | +2304 | +512 | 0xa900 |
| 5 | 25088 | 16384 | 69632 | +53248 | **+51456** | 0x11000 |
| 6 | 27648 | 8192 | -9984 | -18176 | -19968 | 0xffffd900 |
| 7 | 30208 | 36864 | 28672 | -8192 | -9984 | 0x7000 |
| 8 | 22528 | 20480 | 24320 | +3840 | +2048 | 0x5f00 |
| 9 | 13056 | 40960 | 43008 | +2048 | +256 | 0xa800 |
| 10 | 13568 | 40960 | 43520 | +2560 | +768 | 0xaa00 |
| 11 | 14080 | 40960 | 108032 | +67072 | **+65280** | 0x1a600 |
| 12 | 14592 | 40960 | 44544 | +3584 | +1792 | 0xae00 |
| 13 | 21248 | 16384 | 28672 | +12288 | +10496 | 0x7000 |
| 14 | 23808 | 40960 | 36864 | -4096 | -5888 | 0x9000 |

¹ native = grid + 1792 (`route.offset` is 0 at all grid samples per GRIDPROBE).

Readings — hypothesis 1 (lane): the mechanism is certain (§a), and
per-rider draws dissolve the continuity objection, but **the numbers do
not yet close**: a lane-1 term is +64000, lane-2 +128000. Residuals
(Δnative − 64000): slot 3: +1536; slot 11: +1280; slot 5: **−12544**.
Slots 3/11 fit lane-1 plus a small shared term (`curv + cc*256 − 1792 ≈
+1300..+1500`); slot 5 needs `≈ −10750`, and sane slots 6/7/13/14
(−19968/−9984/+10496/−5888) also exceed small-curvature size. So the
wild-3 are **not** all explained by "lane-2 on a wide plaza"
(2×64000 = 128000 matches nothing). Fits lane-1 for two of three;
slot 5 and the mid-size residuals need the lane draws + slice records.

Hypothesis 2 (frame/flags): slot 3 is **exactly** `native + 0x10000`
(83712 − 18176 = 65536) and its low 16 bits equal native exactly
(0x4700 = 18176). But slot 11 is `native + 0xFF00` (low16 42496 vs
42752, −256) and slot 5 is `native + 0xC900` (low16 4096 — not physical
anywhere near the grid). High word (`>>16`) is 1 for all three wild
nodes and 0 (or negative) elsewhere. Verdict: suggestive for slot 3,
**refuted as a general frame rule by slot 5**. Do not force either;
both reduce to needing the runtime lane/record values.

Rule-outs (from rider/node rows, tick 1):
- Post-build motion: `rider.0/1.speed = 0`, `velocity = 0`, `lat_vel =
  0`; bike-node `half108 (+0x108)` = 0 on **all 15** bike nodes;
  `rider.0/1.lat104/lat108/lat10c/yaw60` all 0. Nothing has moved.
- Second-roadway preview: grid samples are 50–123 (`pos>>8`); first Napa
  split is just before sample ~740 (`docs/VERIFICATION.md:136`, NAPA 740
  screenshot). 600+ samples away — ruled out.
- Crashed state: `rider.0/1.crash = 0`. Limitation (stated, not hidden):
  the proxy dumps `rider.*` only for think-gated nodes, so `0x2f4` is
  proven 0 for 2/15; the other 13 are frozen grid riders with zero speed
  words, but their `0x2f4`/`0xc0` bytes are not in this golden.

## (c) Minimal native edit (no code changed by this session)

1. `Race::Race` `place_rider` (`src/race.cpp:140-169`): after computing
   the anchor, replicate the build-time `0x004091f0`:
   `0x28 = sar3(curvature8) + (sliceNextCentre + laneIdx*250)*256 +
   grid_lat`, where `curvature8`/centre/lanes come from the route record
   one sample ahead of the grid sample (same record family
   `ai_straight_target` already uses at `src/race.cpp:1516-1523` —
   factor the shared computation), `laneIdx` from the lane byte.
   Keep `r.brain.bias = grid_lat` (already there, line 161) and store
   the lane byte (`ai.lane`) per rider.
2. `original_opponents` + `Race` ctor (`src/race.cpp:76-115, 170-183`):
   consume **one `random()` `%3` per rider in build order (player first,
   then slots 1..14)** into the lane byte — the shared-stream position
   matters as much as the value (plan-index §73-75 setup-stream note).
   Player lane golden: 1 (`rider.0.brain_18c`).
3. `Opponent` struct (`src/race.h` region): carry `lane` (and, if the
   `[0xcc]`-vs-`offset` question in §a resolves against `offset`, the
   absolute centre word) from `original_opponents` into `place_rider`.
4. Regression anchor: extend the oracle proxy's per-node brain dump
   (`oracle/aweman_proxy.c:344-362`, currently `0x0-0xec` + ease) to
   include brain words `0x18c-0x198` for **every** `nodeaddr` (not just
   think-gated riders) — one-line-class change giving lanes + build
   targets for all 15, which closes §b exactly. (Proposed, not done;
   captures need SIM-owner coordination.)
5. Do NOT touch: `move()` rounding/lean-boost/`bVar3` (cleared by
   move-audit §1), `ai_straight_target` shape (already correct),
   `0x004060b0` re-seat porting (tick-1 evidence shows it correctly does
   not fire for stationary riders; port only with a moving golden).

## (d) What stays open

- Exact per-rider lane draws (slots 1..14) and `sliceNext`
  (`+0x2c` Q8 width / `+0xcc` centre / 8-bend curvature) values at the 15
  grid samples — needed to turn §b residuals into an exact fit. Next
  experiment: env-gated GRIDPROBE-2 in `build_race` dumping per rider
  `(lane, brain+0x190, brain+0x194, bike+0x28)` plus `routes_[0][sample+1]`
  `(offset, right_q8)`; or the proxy extension in (c).4 + re-capture.
- Whether `sliceNext[0xcc]` has an absolute component beyond native
  `offset*256 + road_base_` (slot 5 / 6 / 7 / 13 / 14 residuals).
- `rider.1` (think `0x434660`, pos/lat 12800/18176 = player-identical,
  blank spec words): identity of this second think-gated node — likely a
  demo/rival holder, untouched by this trace.
- `0x2f4`/`0xc0` for the 13 non-think-gated frozen opponents (see §b
  limitation).
- 0x004060b0 re-seat gating for moving riders (fires only inside slice
  window) — port when a moving golden exists.

## Checks / live resources

- Read-only: `rg`/`fd`/python3/`read`; two `struct` reads of the
  reference EXE (hash path `../cdrom/...`, not modified). No builds, no
  captures, no LSP (not needed for a disassembly-anchored trace).
- `git status --short` before/after: only pre-existing modifications
  (`oracle/*`, untracked analysis/review dirs) + this handoff; `src/`
  untouched.
- Live resources: none held.
