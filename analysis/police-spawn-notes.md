# Police spawn trace — plan 24 Task 2 Step 1 (2026-10-12)

Scout-traced (read-only; no REA call spent — decompiles sufficed). All
addresses are original EXE addresses.

## Two cop-creation mechanisms, one arrest outcome

**(A) Live DZHR path (per-tick, course-driven).** Per-tick slice builder
`0x00454240` → DZHR cursor `0x004520d0` → `0x004409e0(kind < 0xb)` →
cop pool `0x004c1274` (template `0x0046dde0`, type `0x20`; draw
`0x00440680` / think `0x00440dd0`+`0x00440ddb` / move `0x00440990`).
One-shot per DZHR record (`*rec |= 0x3f00`). Trigger is DZHR records, not
CESR events — `0x00454240` calls the DZHR loop, then BORR, then the CESR
hook in that order.

**(B) Offline timer path (rider think).** Rider think `0x00407f30`
(`tick & 0xfff == 0`, owner 0, flag `0x10` clear, `DAT_004642dc == 0`) →
ring lookup `0x00449910(-0x2700)` → constructor `0x00406b80` → rider-pair
`0x00405990` (contains `% 3` rand).

**Hybrid:** `0x004409e0(kind >= 0xb)` — a DZHR record with kind ≥ 0xb
calls the same offline `0x00406b80` with two random lateral offsets.

## Live spawn body (`0x004409e0`, kind < 0xb)

Pool alloc via the generic allocator, then the kind<0xb vs ≥0xb branch;
a single `rand()` use (lateral). Only caller: the DZHR record loop
`0x004520d0`. Full body in `analysis/decompiled/004409e0.c`.

## Cop think / move / draw

- Think: `0x00440dd0` is an 11-byte thunk; real state machine
  `0x00440ddb` (1703 bytes, states 0–5/7, kinds 4–7/10, 1–2 rands/tick).
- Draw: `0x00440680` gate + sprite submit (no rand); one-time anim bind
  via `0x0040b6a0` (LOCOP frames).
- Move: `0x00440990` = `0x0043adc0` + ring correction via `0x00439270`
  (no rand). Pool creation `0x00440640` (0x468 × 0xf).
- RNG helper: `0x00441750` = `_rand()` unless `DAT_004753b0 == 3`.

## Arrest → busted

`0x00407f30` → nearby-cop lookup `0x00407860` (radii 0xa00/0x500, slot
`DAT_004642dc` match) → thresholds → `0x00416010(4, 0xb4)` → screen
`0x1d` via `0x00416120` → video Busted1..6 via `0x00449150`/`0x004023e0`
+ picture Results/Bust1..6 via `0x0041d970` (`_rand() % 6 + 1`) /
  `0x0041d7d0`.

## Sirens, HUD, contact (pointers for the port)

- Siren alternation on `DAT_004642dc`, slots `0x004c40d8/dc`, event 19
  dispatch: `0x00435ae0`.
- HUD nearest-opponent scan incl. slot 14 = `DAT_004642dc`:
  `0x00438550` — cops DO count when in that slot.
- Contact handler with type-`0x20` cop branch still to port:
  `0x004049c0`.

## Napa level 1 verdict: OPEN

Not settled — needs the Napa DZHR record census (does any record invoke
`0x004409e0`) plus the offline-timer gate evaluation on a Napa capture.
Step 2 captures (`napa-cops`, `napa-cops-crashed`, `city-cops`) will
answer it empirically.

## Port gap (for Step 3)

`src/police.cpp` today covers only the numeric core of (B) + arrest
thresholds. Missing: all of (A), think/move/draw, sirens, contact, HUD
integration.
