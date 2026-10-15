# Task 4 Step 1 trace: off-road surfaces, reverse riding, finish line (READ-ONLY)

- Plan: `docs/plans/16-gameplay-systems.md` Task 4, Step 1 (trace ONLY). Steps 2–5 not started.
- Baseline commit: `6ecbca9` (hud: restore validated MiniDash numeric speed commit), branch `main`.
- Mode: read-only trace. No source edits made or attempted. No oracle captures run
  (SIM/pixel pipeline owned by the live worker; Wine prefix + oracle files shared).
- Method: `analysis/decompiled/*.c` first, disassembly cross-checks in
  `analysis/original-disassembly.txt`, EXE byte reads. REA **not** invoked by this
  session; where this trace relies on prior REA confirmations it cites the
  `docs/REVERSE_ENGINEERING.md` entries (pinned `rea-agents@6.4.0`, env per
  `docs/TOOLING.md` lines 38–74). Disassembly outranks pseudocode throughout.
- Verdict marks: **(a)** fully determined statically | **(b)** capture-gated
  (exact capture + expected observation given; no guessing).

## 1. Surface drag — how terrain type/flags change speed and steering

### 1.1 Surface class `0x0043a0c0` — (a) determined
`analysis/decompiled/0043a0c0.c` (43 lines). Inputs are the **traversal-slice**
(DLSR/course-iterator) words, NOT `TerrainSlice` (see §4):
`+0xe0/+0xe4` road edges, `+0xe8/+0xec` shoulder edges, `+0x37` marking-flags
byte, `+0x3c` type byte; bike lateral `+0x28>>8`. Rule (checked against asm
conventions; element-scaling n/a — all byte offsets on the slice struct):

| Condition | Class |
|---|---|
| inside `(e0, e4)` | 0 road |
| flags `& 0x20` | 0 |
| flags `& 0x18`: type 0 → 1, else 2 | 1 shoulder / 2 rough |
| type 3 | 1 |
| right of road, at/past `+0xec` | 3 beyond shoulder |
| right of road, inside shoulder, type 0 | 1 |
| right of road, inside shoulder, non-0 type: within `e4+0x30` → **0**, else **2** | 0 / 2 |
| left side mirrors; at/past `+0xe8`... `x <= +0xe8`... left: `iVar3 <= +0xe8` → 3, else mirror of right | 3 / 1 / 0 / 2 |

Precedence note (verified): `&` binds looser than `-`/`==`, so line 19
`2 - (type==0)` and lines 33/41 `(x<=edge+0x30)-1 & 2` (1→0, 0→2) are as tabulated.

### 1.2 Speed effect `0x004390c0` — (a) determined
`analysis/decompiled/004390c0.c` lines 12–50. `+0x160==0` (airborne/no contact):
quarter speed, no surface term. Else `+0x158 = 0x0046dce4[class]`,
`contact = (drag·|v|>>14... (c*|v|>>0xe)) + +0x154` rolling,
aero `+0x150·(base>>8)>>8` with `base=|v|` (or `|v|>>2` airborne),
`+0x15c` total, `accel = +0x114 − drag − (|+0x118|+bias>>4)`.
EXE bytes read today (`../cdrom/ROADRASH/ROADRASH.EXE`): `0x0046dce4` =
**[0, 2, 20, 30]** signed bytes (class 0/1/2/3); `0x0046dce5 = 2` (skid-sound
limit, matches RE notes). So: shoulder ≈ free (2), rough heavy (20), beyond
wall-to-wall (30). Native `integrate_speed` (`src/bike.cpp:261–286`) matches
line-for-line including the negative-speed branch (§2).

### 1.3 Steering effect — (a) determined (already ported, needs no change)
Surface enters steering only via `0x00438c70` traction tables
(`grip 0x0046dce8`, `steer_gain 0x0046dcf8`, `steer_damp 0x0046dd08`,
`skid_loss 0x0046dd18`, `curve 0x0046dd28`, all read from EXE at
`src/bike.cpp:56–63` with corroboration asserts at `:69–70`) plus the
`surface > 1` curve-restrict (`bike.cpp:357–363`, from the REA-confirmed
`0x00438c70` export). `0x0040af50` (`analysis/decompiled/0040af50.c:11–12,
77–79`): **`+0xec <= 0` → `+0x118 = +0xf0 = 0`, total steering authority loss**
at standstill/reverse. Native `steer()` early return (`src/bike.cpp:194`)
matches exactly.

### 1.4 Past the shoulder before a wall — (a) determined, port MISSING
`0x0043a610` (`analysis/decompiled/0043a610.c`, 292 lines; head/tail verified
against disassembly `43a617–43a647` early-return gate, `43abd8/43ad2a`
`cmpl $0x9c4`, `43ac62/43ad96` `call 0x403d60`). All widths are 32-bit except
noted bytes; `(+0x1c & 0xff)`/`(+0x30 & 0xff)` sample fractions; `±4` margins
are in slice units, `+0x174` half-width in Q8:
- Early return (no-op) when bike box `[+0x28−+0x174, +0x28++0x174]>>8` is
  strictly inside `[+0xe0, +0xe4]` — disasm `43a631–43a647` confirms
  `sarl $8` both sides, signed compares. **(a)**
- Slice type 5 (walls), player- or cop-owned bike only (`+0xc4==4`, or
  `+0xc4==8` with owner `+0x2f4==1`): height gate `+0x20` vs `+0x3b>>2<<8`
  (default `0x40`); per-side interpolated wall faces from `+0xe8/+0xec` with
  `+0x3a` shoulder delta, bit 0x1/0x2 of `+0x3b`; touch (`uVar9=1`, clamp
  `+0x28` to face ∓ `+0x174`) vs pass. `+0x160==0` + speed gate → `uVar9=3`
  crash-through: `0x00403d60(reason 3)` for `+0xc4==4` (AI `+0xc4==8` passes,
  lines 103–105). **(a)**
- Tail: `uVar9 1/2` → `+0xf0 *= −100>>8` (sar-rounded); `+0x60/+0x5c/+0x288`
  cleared (`+0xc4==8`: `+0x5c = sign(+0x108)<0 ? 0x800000 : 0`); crash gate
  `|+0xec| >= 0x9c4` (scrape path, bVar1) or `|+0xf0| > 0x9c3` (wall path) with
  `+0xc4==4` → grunt event 5 (`0x00436830`) + `0x00403d60(reason 6)`, else slide
  along the clamped `+0x28`. Threshold signedness: abs via xor-sub, signed
  `cmpl` — matches pseudocode. **(a)**
- Slice types 0/3 (road/shoulder/rough): clamp `+0x28` to shoulder/road edge
  face with scrape-vs-crash window (`uVar9=2` when flags/speed allow, else
  clamp); type 1: player/cop bike clamped to `[+0x14c, +0x158]` (or `+0x15c`
  when `+0x160`), others to road edges; **type 2: `default:` → no-op, free
  riding past everything**. **(a)**
- Native `Race::walls` (`src/race.cpp:1035–1048`) is the documented stand-in
  (clamp at shoulder, crash above 4000): it models NONE of the type dispatch,
  wall faces, scrape branch, reason 3/6 crashes, or thresholds. Type-2 free
  passage is also absent (native always clamps).

### 1.5 NATIVE DIVERGENCE (static, no capture needed)
`surface_class` (`src/bike.cpp:296–303`): for non-type-0 terrain the 0x30
margin rule is **inverted** vs `0043a0c0.c:33,41` — original gives class **0**
within `edge+0x30`, class **2** beyond; native gives 2 within, 0 beyond (both
sides). `bike_test.cpp:53–56` only covers type-0, so nothing catches it.

## 2. Reverse movement (negative speed) — (a) determined, one (b) below
- `0x004390c0` negative branch (`004390c0.c:42–50`): `accel = +0x114 +
  |lateral/16 + drag|`; `nv = v + dt·accel`; `nv > 0 → 0`, else keep. **(a)**
- Entry into reverse: throttle (`0040adb0.c`) and brake (`0040aea0.c:9–11`,
  `+0xec==0 → +0x240 = +0x114 = 0`) **cannot initiate reverse from standstill**
  — brake at zero writes zero drive. Reverse is entered only via impulse adds
  (contacts; tails clamp speed to `[−1000, 30000]`, e.g. `race.cpp:873–874`).
  Holding brake while `+0xec < 0`: `+0x114 = −(+0x240)>>8` (reverse-sign,
  `0040aea0.c:44–45`; native `bike.cpp:142` matches) → sustained backward
  acceleration; released → coasts to 0 via the negative branch. No reverse
  speed cap exists in `0x004390c0` itself. **(a)** rules;
  **(b)** max held-brake reverse speed — capture `napa-solo-reverse`, expect
  monotonic approach to an equilibrium where brake force = drag(−1000 clamp
  bounds it only on contact ticks).
- `0x004395e0` with `+0xec < 0` (`004395e0.c:36–42`): lateral negated,
  `forward = |v|·4` for yaw; yaw/`+0x5c` still computed; **lateral position is
  NOT applied** (`004395e0.c:130–135`: `300 < +0xec` gate; native
  `race.cpp:1032` matches). So a reversing bike keeps heading response but
  slides straight back. **(a)**
- `0x00439270` walks backward (`local_c = −0x100`, chunked `−param_2` walk,
  `00439270.c:24–51`; REA file `rea-439270-roadadv.md` per RE notes). `+0x130/
  +0x12c` NOT zeroed on the zero-distance path unless sweep flag
  (`00439270.c:13–19`; native `move_along` `:339` matches). **(a)**
- Reverse across a split boundary: `0x00439390` backward branch
  (`00439390.c:43–50`) follows the `+0x18/+0x1c` incoming halves with the
  lateral-vs-`+0x39` gate; route pointer tracks backward. Forward commit
  (`+0x76cd4`) unaffected. **(a)** statically; golden behavior needs
  `napa-solo-reverse` **(b)** (reverse 200 samples across a split, then forward;
  expect route/slice to rewind exactly and `+0x1f8` to grow again).

## 3. Finish detection `0x00416010` — (a) rules, (b) same-tick ordering
- `0x00416010` (`00416010.c:12–23`): first-write-wins on `DAT_004c5d7c`
  (gated `DAT_004753c4==0 && DAT_004753b4==0`); `0x7f` → 2 (won) iff 0-based
  place `DAT_004c5d78` < 3 in single player else 3 (lost); 4 busted, 5 wrecked
  pass through; `param_2` → `DAT_004753c4` countdown (0xb4 = 180 ticks);
  `param_2 < 1` ends race immediately. `0x00442e10` (`00442e10.c:10–16`)
  decrements per frame, `< 1 → DAT_004753b4 = 1` (results). REA-confirmed
  (1272 chars, RE notes §Race end). **(a)**
- Detection `0x00406890` (`00406890.c:19–22`): latch `0x00437800` when
  `+0x2c != 0 && DAT_004b6e58 != 0 && +0x1c >= DAT_004b6e58 && latch clear`.
  **No speed gate, no crash-state gate** — a crashed bike crossing sets the
  latch (the `+0x2f4` branch only selects crash-motion vs normal motion first).
  `0x00437800` (`00437800.c:8–18`): sets `+0xc0&0x10` on bike AND rider
  (`+0x1c4`), records tick/place via `0x00437860` (non-cop-owned). **(a)**
- Outcome firing `0x00434660` (`00434660.c`): finished rider (`+0xc0&0x10`)
  with `+0xec < 300` → `0x00416010(0x7f, 0xb4)` (lines 106–109); crashed
  rider (`+0x2f4 != 0`) WITH latch → `0x00437800` + `0x00416010(0x7f, 0xb4)`
  (lines 32–39) — **crossing while crashed still finishes (code 0x7f)** unless
  a wreck/bust won first-write; pre-race latch + slow → same (25–31). **(a)**
- Review Focus 3 edge cases (all statically read; golden proof gated):
  - Stopped ON the line: position `>=` finish fires the latch with no speed
    requirement; the `< 300` gate is already satisfied → outcome `0x7f`.
    **(a)** rule; **(b)** `napa-solo-finish-crash` variant holding position on
    the line, expect `0x7f`→won/lost by place, never stuck.
  - Reversing over it: latch is set-only and `0x00416010` first-write-wins, so
    rolling back across changes nothing; approaching from beyond without a
    latch still latches (`>=` is direction-blind). Reversing *before* the line
    never latches (position only decreases). **(a)** rule; **(b)** behavior in
    `napa-solo-reverse`.
  - Crossing while crashed: latch set by `0x00406890` regardless of `+0x2f4`;
    outcome `0x7f` via the crashed branch — **unless** `0x0040a600` fired
    `0x00416010(5)` first in the same tick. Tick order of `0x00406890` vs
    `0x0040a600` inside the step driver is NOT in the truncated
    `0044b3b0.c` export (that export covers only the ring/camera tail), so the
    same-tick winner is **(b)** — capture `napa-solo-finish-crash` (crash on
    the last 20 samples), expect either `0x7f` or 5 exactly as the original,
    first-write-wins deciding.
- Busted `0x00407f30` (`00407f30.c:18–39`): latch-clear + (`+0x2f4==2` ?
  `|+0xec|−0x708` : `+0xec−300`) underflow + cop within 0x200 pos / 5000 speed
  + `0x00407860 != 0` → `0x00416010(4, 0xb4)`; plus the 0xfff-tick cop-spawn
  timer (lines 40–47). **(a)**
- Wrecked `0x0040a600` (`0040a600.c`): walk timer `> 0x4b0` (1200) with zero
  condition + player → 5 (lines 22–33); type-1 slice bike/rider lateral outside
  `[+0x14c, +0x158]` → 5 and `+0x2f0 = 0` (lines 40–56, disasm-verified per RE
  notes); `%`-anim zero-condition player with latch clear → 5 (107–116);
  `0x0043a190` high fall (`dt·0x4600 <= height`, type-1): knock-off reason 2
  then player → 5 (lines 40–56). Quit-while-racing → 5 (`0x00423408`, prior
  evidence, not re-read). **(a)**
- `+0x1f8` distance-to-finish `0x00407410` (`00407410.c`): finish known →
  `DAT_004b6e58 − +0x1c` (goes negative past the line — the original does NOT
  clamp position at the finish); unknown → walk node lengths `·0x100` to the
  end (lines 40–59, split/merge link forms). **(a)** formula; **(b)** golden
  values in any finish capture.

## 4. `TerrainSlice` producer — which words the drag path consumes
- `TerrainSlice` (`src/terrain.hpp:29–37`) is produced by `TerrainIterator`
  over the SGS `wall/hill` records (`src/terrain.cpp`, `read_terrain`). Its
  consumers are ground/scenery height: `terrain_height`
  (`src/terrain_ground.hpp:7`) used by `Race::vertical` (`src/race.cpp:1061–
  1064`) and `course_scene.cpp:314`.
- The collision/drag path (`0x0043a0c0`, `0x0043a610`, `0x00439b10`,
  `0x00407410`) consumes **zero** `TerrainSlice` words. It reads the
  traversal-slice struct (`+0x2c`): edges `+0xe0/+0xe4/+0xe8/+0xec`, flags
  `+0x37`, kind `+0x38`, wall bytes `+0x3a/+0x3b`, type `+0x3c`, bounds
  `+0x14c/+0x150/+0x154/+0x158/+0x15c/+0x160`, heights `+0x140/+0x144`, sign
  bytes `+0x148/+0x149`, bend `+0xb4`. **(a)**
- OPEN (static, porter-side): native `Race::edges` (`src/race.cpp:262–267`)
  feeds `terrain_source_(s).type` (SGS terrain, default `0xff`) into the
  `+0x3c` slot and `PathSample::marking_flags` (computed `0x21/0x22/8/16/0…`,
  `src/course.cpp:142–151`) into the `+0x37` slot. Bit `0x20` set (as in
  `0x21/0x22`) forces class 0 in BOTH implementations (`0043a0c0.c:15–17`,
  `bike.cpp:292`) — whether these equal therau DLSR `+0x37/+0x3c` values is
  unverified; check against the `0x00454240`/`0x00451910` producers, not a
  capture. Related: `attach_terrain_type` (`src/race.hpp:373`) notes traffic's
  short-phase test reads the terrain type.

## 5. Native counterpart map (read-only; line numbers at baseline)
- `src/bike.cpp:34,57` + `src/bike.hpp:17` surface-drag table `[0,2,20,30]` —
  EXE-verified today (§1.2). OK, no change.
- `src/bike.cpp:261–286` `integrate_speed` (`0x004390c0`) — matches incl.
  negative branch. OK.
- `src/bike.cpp:288–304` `surface_class` (`0x0043a0c0`) — **DIVERGES lines
  299/303** (§1.5); inputs' provenance open (§4).
- `src/bike.cpp:192–225` `steer` + `:227–233` (`0x0040af50` via `0x0040ad30`)
  — `<= 0` gate verified vs `0040af50.c:77–78`. OK.
- `src/bike.cpp:124–143` `brake` (`0x0040aea0`) — reverse-sign `:142`
  verified. OK. `src/bike.cpp:145–167` `throttle` (`0x0040adb0`) — finished-
  latch→brake fallback present (`:146`) but the second disjunct (player AND
  `DAT_004753c4 > 0`, `0040adb0.c:9–10`) is absent — minor sketch item.
- `src/race.cpp:1035–1048` `walls` — stand-in; full `0x0043a610` port missing
  (§1.4). `src/race.cpp:1001–1033` `move` (`0x004395e0`) — matches incl.
  `> 300` lateral gate (`:1032` vs `004395e0.c:130`). `src/race.cpp:335–364`
  `move_along` (`0x00439270`) + `:288–333` `cross_sample` (`0x00439390` fwd +
  back branches verified) — matches.
- `src/race.cpp:295–317` finish in `cross_sample` (`0x00406890`/`0x00437800`):
  **two divergences**: (i) native clamps `to = finish_q8_` (`:302`) — the
  original never clamps (position sails past, `+0x1f8` negative); (ii) native
  `step` (`:2226`) returns when all finished and `step_rider` (`:1770–1774`)
  freezes finished riders — the original keeps stepping everyone for the
  `0xb4` window (`race_screen.hpp:18` already records the delay; rider-side
  motion + `[+0x14c,+0x158]`-style bounds wreck during the window unported).
  No `0x00416010` first-write-wins outcome variable exists natively (outcome
  mapping lives in `race_screen.cpp:171`, `career.hpp:11`).

## 6. Minimal native edit sketch (porter/integrator owns; no code changed here)
1. `bike.cpp` `surface_class`: swap the `0x30`-margin ternary arms (both
   sides) to `x <= edge+0x30 ? 0 : 2`; add `bike_test` cases for non-type-0
   slices at `edge+0x2f/0x31`. No golden needed (static inversion).
2. `race.cpp` `walls`: port `0x0043a610` branch-by-branch at the current call
   point (`step_rider` `:1827`, after `move`, before `vertical` — the export
   order `43a5fe` call site is inside the per-rider physics fn; confirm against
   `0x0043a580` while porting). Needs sub-slice `+0x37/+0x38/+0x3a/+0x3b/
   +0x3c/+0x14c…+0x160` plumbed from the course (see §4 open item first).
3. `race.cpp` finish: remove the `to = finish_q8_` clamp (keep latch/tick);
   model `DAT_004c5d7c` first-write-wins + `DAT_004753c4 = 0xb4` countdown and
   keep stepping finished riders (brake-only, per `00434660.c:106–112`)
   instead of early return; add crashed-with-latch `0x7f` arm
   (`00434660.c:32–39`) and the `0x0040a600` wreck arms.
4. Captures (blocked this session — SIM pipeline owned by live worker):
   `napa-solo-offroad` (10 s verge each side; expect drag classes 1→2→3 then
   type-dependent clamp/crash vs free passage), `napa-solo-reverse` (stop,
   reverse 200 samples across a split, forward; expect exact rewind +
   `+0x1f8` regrowth), `napa-solo-finish-crash` (crash last 20 samples; expect
   `0x7f` or 5 per first-write order). Register all three in
   `oracle-sim-test` + `SIM_SCENARIOS.md`; synthetic `bike_test`: reverse
   equilibrium + `uint32_t` wrap at `0`/`0xffffffff` (plan Step 3).

## 7. Session hygiene
- `git status --short` re-run before writing: only pre-existing live-worker
  modifications (CMakeLists, shared docs, oracle/*, tests) plus new
  untracked analyses/handoffs; this session authored exactly one file:
  `docs/agent-handoffs/offroad-trace-20261015.md` (this file). **Do NOT
  commit** — integrator owns commits on this tree.
- Claims held by the live worker (`CMakeLists.txt`, shared docs, `oracle/*`,
  `hud.*`, `oracle_sim_test.cpp` per `agent_workspace.py list`) were not
  touched, staged, or committed. No REA/decoder/oracle processes started
  (no PIDs held, no shared resources locked).
- Prior docs built on, not reduplicated: `docs/REVERSE_ENGINEERING.md`
  §§ physics/surface/outcome/race-end/crash-entry, `docs/ACTORS.md` (no
  terrain/wall/finish content found — nothing to build on there).
