# Handoff: plan-16 Task 6 Step 1 trace — needle pen + opponent list (READ-ONLY)

- Task: docs/plans/16-gameplay-systems.md Task 6 Step 1 (trace only).
- Baseline: `a30bdbb`. Working tree has other agents' in-flight changes
  (`oracle/aweman_proxy.c`, `oracle/build.sh`, untracked analysis dirs,
  `tests/dash_bench.cpp`); nothing touched by this session except this file.
- Mode: read-only. No source edits, no oracle runs, no REA calls (justified
  in §10). Only new file: this handoff.

Evidence key: `analysis/decompiled/00446c30.c` (dashboard draw),
`analysis/decompiled/0044f510.c` + `0044f4a0.c` (trig),
`analysis/decompiled/00446880.c` / `004468e0.c` (angle writers),
`analysis/original-disassembly.txt` line numbers cited as `dis:N`
(file line, not address; addresses inline as `0x…`),
EXE-image reads via `python3` from `../cdrom/ROADRASH/ROADRASH.EXE`
(offsets through the `.data` mapping `VA→raw = VA−0x463000+0x61600`,
per `src/bike.cpp:49`), existing read-only capture
`analysis/oracle/napa-dash-solo-{f,d}30/100/104/110/119.raw`
(8-bit indices, 640×480; `f` = pre-dash, `d` = post-dash).

Starting-point verification: `00446c30.c:142-176` matches the brief
exactly (pen cache, two `FUN_0044f510` + `MoveToEx`/`LineTo` pairs,
`DAT_00467a98 == 0` gate). All itemized traces below.

## 1. Pen semantics (exact)

- Call sequence, `00446c30.c:143-148`, confirmed in disasm `dis:97547-97558`
  (`0x447112-0x44713f`): `if (pen_cache[HiRes] == 0) pen_cache[HiRes] =
  CreatePen(0, HiRes+2, 0x81172)`, then `SelectObject(hdc, pen)` saving the
  old pen to `edi`, restored at `dis:97659-97662` (`0x44727e-0x447284`).
- Cache: two `HPEN` slots at `0x49e0b0[HiRes]` (`dis:97549`
  `movl 0x49e0b0(,%ebx,4),%eax`; store `dis:97558`). Created once, never
  deleted in this function. Selector (`uVar7`, `00446c30.c:143`) is the byte
  `DAT_00475380` (`dis:97548` reloads it).
- `CreatePen` args, `dis:97552-97557`: `push 0x81172; push (HiRes+2);
  push 0` through IAT `*0x4ca60c` (`dis:97555-97557`). So
  `CreatePen(style=0, width=HiRes+2, color=0x081172)`.
  Style 0 = `PS_SOLID` cosmetic (width is in device pixels, not logical).
- `MoveToEx` via IAT `*0x4ca608` (`dis:97579/97630`), `LineTo` via IAT
  `*0x4ca604` (`dis:97609/97658`), `SelectObject` via `*0x4ca610`.
  Per-needle order: `MoveToEx(origin)` (`dis:97572-97584`), `LineTo(tip)`
  (`dis:97585-97613`); second needle `dis:97619-97658`. Draw order: speed
  needle first, rpm needle second (rpm on top).
- Width rule: `DAT_00475380 == 0` (low 320-wide) → width 2;
  `!= 0` (high 640-wide) → width 3. Native never implements this
  (`src/hud.cpp:176` passes `width=scale`, i.e. effective 3-wide at scale 1
  in both modes).

## 2. Color `0x81172` — settled, with one consequence

- `0x081172` as `COLORREF` (`0x00BBGGRR`) = R `0x72`, G `0x11`, B `0x08`.
  Verified arithmetically; byte-identical to the native constant
  `0x721108` (`src/hud.cpp:176`). It is a raw `COLORREF`, not a palette
  index and not `PALETTEINDEX` (no `0x01000000` flag bit).
- Read-only capture finding (existing frames, no new capture):
  tach-box rows 425–450 × cols 355–396 in `napa-dash-solo-d30.raw` and
  `-d100.raw` carry the needle as index **37** (92 px); in `-d104.raw`,
  `-d110.raw`, `-d119.raw` the same shape (92/91/95 px) is index **52**.
  Same `COLORREF`, different index ⇒ the realized 8-bit palette animates
  (countdown→green switch between frames 100–104; cause already open in
  VERIFICATION.md plan-11a-Task-2-Step-5). No dithering observed (solid
  single index). Consequence for the port owner: the native RGB pipeline
  (`RgbImage` + per-frame `DashLut`) is already immune to this — keep
  writing raw RGB, do NOT hard-code index 37/52. Only the raster SHAPE
  needs porting (§7).

## 3. `FUN_0044f510(angle, &cos, &sin)` — fixed-point format

- Decompile `0044f510.c:8-26` verified against disasm `dis:109216-109263`
  (`0x44f517-0x44f588`): index `i = angle & 0x3ff`, quadrant
  `q = (angle & 0xfff) >> 10` (`andl $0x3ff`; `sarl $0xa`, `dis:109219/109224`).
  Table base `0x475768` (`dis:109220`), mirrored read
  `0x476764 − i*4` (`dis:109222-109226`), jump table on `q` (`dis:109230`).
- Table contents (EXE image): `T[0]=0, T[186]=9223, T[256]=12540,
  T[512]=23170, T[1022]=T[1023]=32768`. So `T` is a Q15 quarter-sine
  (peak 32768; `0x476764 − 0x475768 = 0xFFC = 1023*4`, i.e. the mirror read
  is exactly `T[1023−i]`).
- Outputs are `T[…]*2`, i.e. **Q16** full-cosine/sine pairs
  (magnitude 65536):
  `q0: (T[i], T[1023−i])`, `q1: (T[1023−i], −T[i])`,
  `q2: (−T[i], −T[1023−i])`, `q3: (−T[1023−i], T[i])`, all ×2.
  Native `sine()` (`src/bike.cpp:182-190`, the `0x0044f4a0` single-output
  twin) implements the identical quadrant logic over the identical table
  (`src/bike.cpp:64-69` corroborates `sine[512]==23170`), and the handoff
  author verified `Hud::compose`'s
  `sx=sine(angle), cy=sine(angle+1024)` (`src/hud.cpp:174`) equals the
  `0044f510` pair element-for-element. Trig needs NO porting work.

## 4. Angle sources — unit table

Writers: `DAT_0049e108` (rpm) from `FUN_00446880` (`00446880.c:13-17`);
`DAT_0049e10c` (speed) + `DAT_0049e110` (mini second value) from
`FUN_004468e0` (`004468e0.c:9-34`).

| Mode (`DAT_00467a98`) | `DAT_0049e108` (rpm angle) | `DAT_0049e10c` (speed angle/value) | `DAT_0049e110` |
|---|---|---|---|
| 0 (full) | `bike+0x2dc / 44 − 1024`, `+4096` if `< 0` (`00446880.c:13-17`). Range 0..4095, 4096 units/turn | `(((+0xec & 0xfffff001) >> 1) + (+0xec & 0xffffff80)*16 + (+0xec & 0xfffffe00)*4) / 200 − 1024`, `+4096` if `< 0` (`004468e0.c:11-17`). Same 0..4095 dial | untouched (stale) |
| != 0 (mini) | untouched (stale; `00446880` returns early) | numeric speed decomposition, clamped `>= 0`, NO wrap: metric (`0x0042f490` true) `(((v&0xfffff0ff)>>8)+((v&0xffffff87)>>3)+((v&0xfffffe1f)>>5))/10`, else `(v>>12)+(v>>7)+(v>>9)` (`004468e0.c:20-31`) | `(bike+0x108 >> 12)+(>>7)+(>>9)` (`004468e0.c:32-33`), sign-extending shifts |

- Direction: increasing raw value ⇒ increasing angle ⇒ clockwise on the
  dial (endpoint `x = ox + cos·rx`, `y = oy − sin·ry`, §5; tach climbs
  post-green per VERIFICATION.md).
- Parked values (countdown, `+0x2dc = 0x2000`, speed 0): rpm angle
  `8192/44 − 1024 + 4096 = 3258` (`q3, i=186`, cos −62858, sin 18446);
  speed angle `0 − 1024 + 4096 = 3072` (`q3, i=0`, cos −65536, sin 0).
- Native `Hud::state` (`src/hud.cpp:93-97`) already computes both angles
  with these exact formulas (including the `+4096` wrap); `hud_test.cpp:73-76`
  pins the rpm one. No angle work remains.

## 5. Origin / radius / endpoint math — settled bit-for-bit

- Layout: stride `(MiniDash + HiRes*2) * 0xdc` above `0x474f50`
  (`00446c30.c:64`). High-full stride 2 dumped from the EXE image:
  tach origin `(388, 441)` radii `(1835008, 1310720)`;
  speedo origin `(250, 449)` radii `(2883584, 1835008)`.
  Radii are **Q16 pixels**: 1835008 = 28×65536, 1310720 = 20×65536,
  2883584 = 44×65536. (Low-full stride 0 differs: tach `(194,220)` etc.;
  mini strides 1/3 zero the tach entries.)
- Byte offsets (= decompiler element index ×4, AGENTS.md word-scale rule):
  speedo origin `+0x54/+0x58` (idx 21/22), radii `+0x5C/+0x60` (idx 23/24);
  tach origin `+0x40/+0x44` (idx 16/17), radii `+0x48/+0x4C` (idx 18/19).
  Disasm order confirms `x` comes from `+0x54`, `y` from `+0x58`
  (`dis:97573-97584`: `0x58`-load pushed before `0x54`-load = `MoveToEx`
  `y`-then-`x` push order).
- Endpoint, `dis:97585-97613` (`imul` + `mov %edx,%eax` = signed high-32):
  `tip_x = ox + floor((cos_Q16 * rx_Q16) / 2^32)`,
  `tip_y = oy − floor((sin_Q16 * ry_Q16) / 2^32)`
  (logical `>>32` of the two's-complement product = floor; native
  `high_product` in `src/hud.cpp:9` uses arithmetic `>>32` — identical).
- Dash origin `DAT_00475394/98` (on-disk 0,0; runtime fullscreen offset)
  is added to both endpoints (`dis:97574-97582`); it translates, never
  reshapes. `DAT_0049e120 = HiRes+1` (`00446c30.c:84`, `dis:97248-97252`)
  is the TEXT shadow offset only (`FUN_00447b20`); needles ignore it.
- Parked spines (HiRes, dash origin 0): tach `(388,441)→(361,436)`
  (`dx=floor(−62858·1835008/2^32)=−27`, `dy=floor(18446·1310720/2^32)=5`);
  speedo `(250,449)→(206,449)` (`dx=−44`, `dy=0`). Native `needle()`
  (`src/hud.cpp:170-178`) computes these exact endpoints (same table, same
  quadrant logic, same `high_product`) — verified by reading, not by
  execution. Spine math needs NO porting work.

## 6. Selectors and the non-needle branch

- `DAT_00475380` (`HiRes`, registry `HiRes`, `0x004457f0` choices 1/2/3 →
  low/low-inset/high; `004457f0.c:10-17`): selects layout stride
  (`HiRes*2`), pen width (`HiRes+2`), pen cache slot, and artwork family
  (`00416310.c:11-28`). On-disk image value is 1.
- `DAT_00467a98` (`MiniDash`, F10 `0→1→2→0` in `0x004431d0`, `004431d0.c:37-39`):
  `0` = full (needles, §1); `2` = everything in this region skipped
  (`00446c30.c:89,142`, `dis:97268-97270` `cmpb $0x2` → `je 0x44728a`);
  `!= 0` = numeric branch (`00446c30.c:177-188`, `dis:97498-97546`):
  `DAT_004c8029==0` → `sprintf(buf, "%d", DAT_0049e10c)` (format is the
  `"%d"` at `0x466654`, confirmed `%d\0` in image);
  else → `sprintf(buf, "%d:%d", DAT_0049e10c, DAT_0049e110)`
  (format `"%d:%d"` at `0x475310` per `string-references.tsv:655`;
  this corrects the brief's `%d %d` — it is colon-separated).
  Drawn once at the speedo rect (`+0x54`) via `FUN_00447b20`
  (black, then white at `−DAT_0049e120,−DAT_0049e120`). Record-only: native
  mini mode instead shows distance (`src/hud.cpp:154-163`) — a known
  divergence for the port owner, no action here.

## 7. GDI width-2/3 fill rule — PARTIALLY settled, CAPTURE-NEEDED

Ground truth (read-only, `napa-dash-solo-d30.raw`, HiRes ⇒ width 3,
needle index 37). Complete 37-pixel maps (rest of box = dial artwork):

- Speedo, spine `(250,449)→(206,449)`: y448: x206–250; y449: x205–251;
  y450: x206–250. Fits symmetric ±1 band + 1 px flat caps on the middle
  row ONLY (outer rows do not extend ⇒ caps are flat, not round r≥1.4).
- Tach, spine `(388,441)→(361,436)`: y434: x361–366; y435: x360–370;
  y436: x361–375; y437: x366–379; y438: x370–384; y439: x375–388;
  y440: x379–388; y441: x384–389; y442: x388. (92 px; matches the
  plan-11a-Task-2-Step-5 box `x[360..389] y[434..442]`; native renders
  `y[435..442]` — the delta is exactly row y434 x361–366, 6 px.)
- The tach band sits ≈0.5–1 px ABOVE the mathematical spine with a
  ≈3–4-row profile; no uniform rule tried fits both needles + caps:
  symmetric ±1 band + flat caps fits the speedo perfectly (including
  `(205,449)`/`(251,449)` and the absence of `(205,448)`), but predicts
  tach rows 435–437 at x361–366 vs observed including 434 and excluding
  437 at x361; round-cap r1.5 predicts `(360,436)`/unpainted-diagonal
  pixels that contradict observation. Corner-vs-center conventions move
  predictions by 0.5 px only and cannot cover the 2.4 px gap at (366,434).
- **CAPTURE-NEEDED — do not guess.** Settling the rule needs one of:
  (a) a controlled capture sweeping a static needle angle (or several
  parked `+0x2dc` values) with a palette dump, isolating band vs cap
  pixels; or (b) a Windows GDI reference raster of
  `CreatePen(0,3,0x081172)` over the two parked segments. The port owner
  owns this capture; the spine math in §5 is frozen and must not be
  "fixed" to fit the raster.

## 8. Opponent list — full spec from code (no pixels exist yet)

- Gate (`00446c30.c:215`, `dis:97737-97740`): drawn iff
  `DAT_00475390 (Inset) != 0 || DAT_004753a8 (OpponentList) != 0`.
  In `napa-dash-solo` rows 0–60/cols 0–320 are bit-identical pre/post
  (both flags were 0 at capture runtime), so NO existing frame shows the
  list — a future `napa-dash` capture must set OpponentList on
  (plan-16 Task 6 Step 2 already requires this).
- Writers of `0x004753a8`: `/` key (`0x2f`) → `0x423777` toggles 0↔1 via
  `sete` AND clears `0x46afdc` (REVERSE_ENGINEERING.md:967; natively
  PORTED: `toggle_opponent_list`, `src/frontend.cpp:1168-1171`, menu `/`
  wiring `src/frontend.cpp:1293`, pinned by `frontend_test.cpp:526-533`).
  Registry save/load `00416120.c:17-18` (`OpponentList`, 1 byte). On-disk
  image byte is 1; runtime value comes from the registry. Plan 18 Task 1
  records the same key (docs/plans/18-settings-and-options.md:22 lists the
  storage key; no separate options-screen row — it is menu-`/`-only).
- Geometry (`00446c30.c:262-273`, `dis:97810-97837`): rect
  `{left 0, top 0, right 0x140, bottom 0x3c}` (320×60 logical px, top-left).
  If `Inset != 0`: `FillRect(BLACK)` + `bk = 0`; else no fill + `bk =
  0xffaf93`. `SetBkMode(OPAQUE)`, font `DAT_004c11f4`, `DrawTextA` flags
  `0x4850` per row. Row height `0xb + (bVar4 ? 0 : 4)` = 11 when
  low-res fullscreen (`bVar4 = HiRes==0 && Inset==0`, `00446c30.c:209-214`),
  else 15. Update rate: every drawn frame (no tick gate; inside the
  per-frame `0x00446c30` from the `0x00416310` dashboard path).
- Rows shown: at most 4 (`00446c30.c:277-308`, `dis:97843-97879`):
  rider-count−1 < 4 ⇒ all from 0; `0x46afdc==0` ⇒ 4-window around the
  player row; `==1` ⇒ last 4; else ⇒ first 4. Writer of nonzero
  `0x46afdc` is UNKNOWN (the `/` toggle clears it) — OPEN, record-only.
- Content per row (`00446c30.c:313-372`, formats from
  `string-references.tsv:649-654`): entries built from the 7 slots
  `DAT_004c7f74..DAT_004c7f8c` (`dis:97746-97770` loop to `0x4c7f90`),
  valid iff `slot != 0 && slot+0x324 == −1` (`dis:97752-97756`); gap
  `d = player.+0x1f8 − opp.+0x1f8` recomputed inline (`dis:97757-97762`;
  same formula as `0x00438550` but WITHOUT its ahead/behind target
  selection — every valid slot is listed, bubble-sorted ascending by gap,
  `dis:97771-97809`). Player row (slot 0): normal `"%s"` (name from
  `0x0041c140`), debug (`0x4c8029`) `"%3dK %d %d"`; color `0x10e7`.
  Opponent behind (`d >= 0`): normal `"+%s %s"`, debug `"+%s %3dK %d %d"`;
  ahead: `"-…"` variants; first `%s` = `"%d%c%03d"` gap text
  (`|d|·1000>>4)/(metric ? 3279 : 5280)` via `0x0042f450`. Name pointer =
  `FUN_0042fb10(slot+0x328)` = `DAT_0049730c + i*8` (8-byte name records);
  opponent color = `FUN_0042fb20(same char)` if nonzero else `0x1ca328`,
  else (`Inset==0`) pink-suppressed (`00446c30.c:359-365`). Row count for
  the window = `FUN_00431840()` = highest set flag in the `0x20`-stride
  name table `0x497304..0x497404` + 1 (`00431840.c:8-20`).
- Data-source answer for the brief: NEITHER the `0x00438550` target nor
  HUD entry 10 — the list re-scans the opponent slots and recomputes gaps
  inline (same `+0x1f8` metric, different selection: all-valid-sorted vs
  single-target). HUD entry 10 / `0x00446b80` (name+gap of the ONE target)
  is the separate single-target display the native already ports
  (`src/hud.cpp:187-205`); the multi-row overlay has NO native counterpart.

## 9. Native counterpart map + minimal edit (for the port owner; NOT done here)

- `src/hud.cpp:10-18` `line()` — 3×3-block Bresenham, `width=scale`.
- `src/hud.cpp:170-178` `needle` lambda — color `0x721108` (value-correct
  per §2), spine math bit-exact per §§3–5, width `scale` (ignores the
  HiRes 2-vs-3 rule), no caps, no pen cache.
- `src/hud.cpp:121`, `hud.hpp:30-31` — the code already documents the GDI
  pen as "documented, NOT ported".
- `tests/hud_test.cpp:89` pins needle pixel `(250,449)`; `:101-104` pins
  mini = no needles; NO test covers the opponent list (nothing to pin it —
  native has no list code at all).
- Minimal edit: (1) keep `sine()`/`high_product`/angle/endopoint code
  untouched (verified bit-exact); (2) replace the `line()` raster with the
  settled GDI band/cap rule (blocked on §7 capture) parameterised by
  `width = HiRes ? 3 : 2`; color stays raw RGB through the per-frame
  palette path; (3) add the §8 list block (rect, ≤4 rows, 11/15 px rows,
  `DrawTextA`-equivalent through `TextRenderer`, formats above,
  `settings.opponent_list || inset` gate — `settings.hpp:41` already
  carries the bool); (4) note the mini-mode divergence (numeric angle text
  at the speedo rect vs native distance readout) for the owner to resolve.
  Opponent-list pixel acceptance needs the OpponentList-on capture (§8).

## 10. Method notes / limits

- REA (`npx --yes rea-agents@6.4.0`) NOT invoked: every pseudocode region
  used here (`0044f510`, `00446880`, `004468e0`, the `00446c30` needle and
  list blocks) was cross-checked instruction-for-instruction against
  `analysis/original-disassembly.txt` with zero discrepancies found, so no
  "suspect pseudocode" trigger fired. Pinned toolchain per docs/TOOLING.md
  would have been REA 6.4.0 / Node 24.13.0 / Ghidra 12.1.4
  (`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
  `JAVA_HOME=openjdk@21`).
- Read-only verification of no shared-state impact: `git status --short`
  before/after shows only pre-existing entries; no builds (doc-only),
  no oracle processes, no Wine prefix use.
- Two decompiler-reading traps avoided with disasm: (a) `aiStack_64[u*3+1]`
  is really entry+8 (name ptr), not entry+4 — the int view base is 4 bytes
  into the 12-byte entries (`dis:97747-97748`: int view at `esp+0x124` vs
  byte view at `esp+0x120`; name-ptr read `dis:97938` = entry+8);
  (b) `00446c30.c:182` format is `"%d:%d"`, not `"%d %d"`.

Remaining work (not mine): §7 pixel capture + pen port; §8 OpponentList-on
capture + list port; `0x46afdc` nonzero writer; mini-mode readout decision.
