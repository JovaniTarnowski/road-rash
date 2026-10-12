# Intersections trace — plan 24 Task 1 Step 1 (2026-10-12)

Owner REA-traced `0x0041eca0` this turn (phase tables, signal objects,
roadways); this note verifies every claim against `analysis/decompiled/`
+ `analysis/original-disassembly.txt` and records deltas. One REA second
opinion was spent on `0x0041eca0` (pseudocode identical to the checked-in
decompile — no delta). All addresses below are original EXE addresses.

## 0x0041ebf0 / 0x0041ec30 — pool create / destroy (confirmed)

- `0x0041ebf0`: `DAT_00468ee0 = 0x00438b70(0x108, 0x28)` → signal pool,
  40 × 0x108; `DAT_00468ee4 = 0x00438b70(0x114, 10)` → controller pool,
  10 × 0x114. Guarded: only allocates when both are NULL.
- `0x0041ec30`: frees both via `0x00438bd0`, NULLs them.
- How confirmed: `analysis/decompiled/0041ebf0.c` + `0041ec30.c`, and
  disassembly `41ebf0–41ec67` shows the same sizes/addresses.
- Delta vs plan: none. Plan says "40 × 0x108 … 10 × 0x114" — exact match.

## 0x0041ec70 — CESR event entry (confirmed)

- Calls `0x0041eca0` first, then `0x0044dd00(param_1[10])` (density byte
  +10) when not in network play, or when the byte is negative.
- How confirmed: `analysis/decompiled/0041ec70.c` (7 lines).

## 0x0041eca0 — controller placement (verified, no delta)

- Early-out: `(char)param_1[2] + 1 > 3` returns (event word 2 selects
  intersection variant 0..2; 3+ means no intersection).
- `bVar1` (cross-road present, short phases): slice type byte `+0x1c == 2`
  at the cross node AND event byte `+0x3c == 4`.
- `bVar13`: event flag byte `+0xb == 0` (signals get `+0xdc = 0` cleanup
  path at the end when set).
- World offsets: `iVar10` = cross-node sample − event sample (Q8-masked);
  controller `+0x108/+0x10c` = event words 0/1 scaled `*0x100 + iVar10`.
- Controller object: `0x00438930(pool 00468ee4, template 00468f10)`.
  Phase table at `+0xfc/+0x100/+0x104`:
  - `bVar1 && bVar13`: `{0x4b, 0, 0xf0}` = `{75, 0, 240}`.
  - else: `{600, 0xb4, 0x30c}` = `{600, 180, 780}`.
  Matches the plan's two tables exactly (owner's `{75,0,240}` condition
  "slice type 2 and CESR flag +11 == 0" matches `bVar1 && bVar13`;
  note the flag is event byte `+0xb`, i.e. decimal offset 11 — confirmed).
- Signals (up to 4, via `0x0041ef60`), stored at controller
  `+0xec/+0xf0/+0xf4/+0xf8` (`piVar8+0x3b..0x3e` zeroed first):
  - `+0xec`: `ef60(0, 1, 0x50, iVar5, cVar2)` iff forward roadways
    (`iVar4+0x2c`) > 0. FAIL-SAFE BUG RISK: on NULL return it `goto`s the
    cleanup label, skipping the remaining signals — partial intersection.
  - `+0xf0`: `ef60(2, 0, 0xb0, iVar4, cVar2)` iff cross roadways
    (`iVar5+0x30`) > 0, same goto on NULL.
  - `+0xf4`: `ef60(3, 0, 0x50, iVar5, cVar3)` iff `!bVar1` and
    `cVar11 == 1 || cVar11 == 2`.
  - `+0xf8`: `ef60(1, 1, 0xb0, iVar4, cVar3)` iff `!bVar1` and
    `cVar11 == 1 || cVar11 == 3`. NOT NULL-CHECKED (falls through) —
    asymmetry worth a boundary test.
  - `cVar2` = rearward lane count − 2; `cVar3` = forward + cross roadway
    count. So the main-road pair uses `cVar2`, the cross pair `cVar3`.
- How confirmed: decompile `0041eca0.c` lines 19–103 + REA re-trace
  (identical output); disassembly `41ed89` (`mov $0x468f10,%edx`) and
  `41efc4` (`mov $0x468f30,%edx`) confirm both templates.
- Delta vs plan: none on values. Two port-relevant details the plan text
  omits: (a) the NULL-`goto` partial-build path, (b) signal `+0xf8`
  unchecked.

## 0x0041ef60 — signal setup (verified, one correction)

Signature `(param_1 = signal index 0..3, param_2, param_3, param_4 =
road node, param_5, param_6 = controller)`:

- Position: midpoint of the node's `+0xe0/+0xe8` edge pair, scaled
  `*0x100`, plus lateral `param_3` (0x50 or 0xb0); sample id derived from
  the edge-midpoint delta. Object: `0x00438930(pool 00468ee0, template
  00468f30)`.
- Fields: `+0xec [0x3b]` = controller ptr; `+0xf0 [0x3c]` =
  `(~index & 1) * 2` → initial phase 0 (odd index) or 2 (even index);
  `+0xf4 [0x3d]` = 0x30c (phase accumulator seed); `+0xf8 [0x3e]` = signal
  index (crossing-spawn flag: odd indices spawn, see `0x0041f040`);
  `+0xfc [0x3f]` = param_5 (lane byte); `+0x100 [0x40]` = 1;
  `+0x102` = sequence `DAT_004c5cd2++`.
- Also writes `_DAT_00468f34 = DAT_00468f00[index]` (car-dispatch table
  entry for this direction).
- CORRECTION vs plan: plan says "`+0xf0` phase and `+0xf8` flags" —
  more precisely `+0xf0` is the phase state (init 0/2, cycles 0..2 in
  `0x0041f040`) and `+0xf8` is the signal index whose bit 0 gates
  crossing spawns. `+0xf4` is the per-signal phase accumulator.
- How confirmed: `analysis/decompiled/0041ef60.c` (32 lines).

## 0x0041f040 — phase advance + crossing spawn (verified)

- Loops the 4 signal slots (`+0xec + i*4`); skips NULL.
- `+0xf4 += controller +0x18` (per-tick increment).
- Active window: `DAT_0049f240 + 0x100 <= signal->+0x1c <=
  DAT_004b6e50 - 0x200` (plan's `[back+0x100, front-0x200]` — exact).
- On overflow past `controller +0xfc[phase]`: subtract duration,
  `phase = (phase+1) % 3` (`if (2 < phase) phase = 0`), loop while still
  overflowing (multi-phase catch-up).
- Crossing spawn iff ALL of: phase wrapped to 0, signal index odd
  (`+0xf8 & 1`), global tick bit 3 set (`DAT_004b7ecc & 8` — "every 8
  ticks" is bit 3 of the tick, i.e. ticks where `tick & 8 != 0`).
- Spawn kind/lane: controller `+0x110 == 1` → kind 1, lane
  `rand() % (+0x111 / 2)` (extra lane byte if signal flags == 3);
  `+0x110 == 2` → kind 3 (lane = full `+0x111` range); else kind 1.
  Lateral speed `(-(kind==3) & 0xf0000) - 0x78000`: kind 1 → `-0x78000`,
  kind 3 → `+0x78000`. Position: `(lane+1)*0x100 + controller +0x108`
  mapped via `0x00449990`; `rand()` from `0x00441750`.
- Spawn call: `0x0044df60(kind, lane, lateral, signal, mapped_node)`.
- How confirmed: `analysis/decompiled/0041f040.c` (62 lines).
- Delta vs plan: none — every constant matches.

## 0x0041f220 — signal draw (verified)

- Object `+0xa8` = `DAT_00468ee8` (head sprite; skipped entirely if NULL);
  `+0xb0/+0xb4` = half width/height from the sprite dims; `+0xb8` =
  `(+0xf8 & 2) == 0 ? 1 : 0`.
- Draws head via `0x0044ba40`, then re-points `+0xa8` at
  `DAT_00468ef0[controller(+0xec)-> +0xf0 phase]` (lamp for current
  phase) and draws again if non-NULL.
- How confirmed: `analysis/decompiled/0041f220.c` (50 lines).
- DAT load sites: **NOT FOUND.** `DAT_00468ee8`/`DAT_00468ef0[0..2]`
  are only *read* in this function (disassembly `41f233/41f250/41f303`);
  no writer exists in `analysis/decompiled/` or
  `analysis/original-disassembly.txt` (all `468ee8/468ef0` hits are
  reads; the only `468ee0/468ee4` writes are pool create/destroy).
  In the file they are zero, so signals are invisible until whatever
  course-load path populates them runs. OPEN QUESTION for the port:
  find the writer (suspect course/scenery init, possibly via the signal
  template words 9..12 `40 1e 41f220` — unexamined).

## 0x0041f1f0 / 0x0041f1e0 — teardown (verified)

- `0x0041f1f0`: frees each non-NULL signal slot via `0x00437c10`, then
  frees the controller itself. `0x0041f1e0` (signal teardown callback in
  template `00468f30` word 6) is an empty stub — verified in
  `analysis/decompiled/0041f1e0.c`.
- Template words (read from EXE, image base 0x400000):
  - `0x00468f10` (controller): type `0x400`, think `0x0041f040`,
    teardown `0x0041f1f0` (words 4/6).
  - `0x00468f30` (signal): type `0x40`, draw `0x0041f220`, teardown
    `0x0041f1e0` (words 3/6); words 9..12 = `00 35 54 73 87` (unexamined,
    may feed the missing DAT load).
- Scheduler wiring: **NOT FOUND in `0x00438170`.** The scheduler
  decompile shows no `0x0041f040` call; think dispatch must be via the
  template word-4 function pointer (type-driven object iteration), not a
  direct call. Port must register think by template, not by call site.
  OPEN QUESTION: confirm the exact iteration that invokes think word 4.

## Crossing-car movement (`0x0044e710`) and rider collision

- Kind bit 0 set (kinds 1/3) takes the crossing branches: the
  `uVar1 == 1` / else (`== 3`) arms at lines ~200–245 use lateral
  position math (`+0xb` node edges, `+0x5d`, `+0x10`) instead of the
  longitudinal follow logic of kinds 0/2. Kinds 0/2 use `+0x108` edge
  offsets; kinds 1/3 use `+0xe0/+0xe4` edges and negate (`-iVar14`).
- Proximity flag: kind-odd cars within `|pos - DAT_004b6e5c| + 0x400 <
  0x2801` set bike-flag bits `+0x30 |= 3` (line 36–44).
- Rider collision (`0x004046b0`): crossing cars carry car-template type
  `0x10` at `+0xc4`, which matches the `& 0x15` contact branch
  (`0x15 = 1|4|0x10`) — riders DO collide with crossing cars (damage path
  identical to normal traffic). Type-2 early-out does not apply.
- How confirmed: `0044e710.c` lines 25–130/130–260, `004046b0.c` lines
  13–27. Full crossing-motion tail (lines 260–340) NOT read line-by-line
  — port step must verify the remainder.

## What the port needs (handoff)

1. Controller pool 10 × 0x114 (type `0x400`), signal pool 40 × 0x108
   (type `0x40`); create in race init, destroy at race end.
2. CESR-event hook: call placement then density (`0x0041ec70` order).
3. Think-by-template dispatch for `0x0041f040`; draw `0x0041f220` needs
   the `DAT_00468ee8/ef0` writer found first (else invisible signals).
4. `0x0044df60` odd-kind path already exists for normal spawns; crossing
   passes `param_4 = signal` (non-NULL → copies signal `+0x108/+0x10c/
   +0x110` into car `+0x74/0x75/0x76`).

## 0x0041f220 draw is dead (October 12, 2026, REA xref)

`DAT_00468ee8` (signal head sprite) has exactly two xrefs in the
executable, `0x41f233`/`0x41f250` — both the reads inside `0x0041f220`
itself. No writer exists, and the DAT is zero in the file, so `+0xa8`
is always NULL and the head draw is always skipped; the lamp array
`DAT_00468ef0` was already censused read-only the same way. The port
draws no signal heads, matching the original. Plan 24 Task 1 Step 5's
pixel frame is unaffected by signals.

## Corrections (October 12, 2026, second pass)

Re-checked against `0041eca0.c`, `0041ef60.c`, `0041f040.c`, `0044df60.c`,
`0044e710.c`, `0044dc10.c`, `00449a80.c`, `00449da0.c` and `004415b0.c`:

- **Slots.** `+0xec/+0xf0/+0xf4/+0xf8` hold signal indices 0/1/2/3. Creation order is 0, 2, 3, 1; `piVar8[0x3c]` is `+0xf0` and receives index 1. The think walks slots 0..3, so the `rand()` order follows the index.
- **`cVar2`** is `end − start − 2` (zone length), not a lane count.
- **`0x50/0xb0`** are position fractions passed to `0x00438930`. The lateral is the shoulder middle.
- **Controller `+0x108`** is the event START sample (·0x100 + node delta); `+0x10c` is the end. Crossing cars spawn at start + (lane+1) samples. The first port used the end.
- **`+0x110`/`+0x111`** are the event type and record byte +9. They select the crossing kind and lane range; they were not a fixed kind 1.
- **Controller cadence.** Template word +8 = 0x1e, so the controller thinks every 30 ticks with dt 30, not every tick.
- **`±0x78000`** in `0x0044df60` is the crossing car's starting lateral relative to the slice centre. The car drives at its 0x00449880 speed toward a ±0x32000 target. The first port added 0x78000 every tick.
- **Signal type** is `0x40 << index` (`0x00468f30+4` rewritten from `0x00468f00`). Kind-0 cars stop at index-0 signals, kind-2 cars at index-2.
- **Observed in the port:** a road car inside the zone and a crossing car can be each other's lane obstacle. Both follow at speed 0, and kind-0 lane changes skip car obstacles (`0x0044e710`: `(type & 0x10) == 0`), so this can hold until the ring drops them. It is unconfirmed against a capture.
