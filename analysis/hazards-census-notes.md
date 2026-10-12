# Road-hazards census — plan 24 Task 4 Step 1 (2026-10-12, read-only)

> **Superseded (October 12, 2026):** this verdict is wrong for pedestrians. Pool `DAT_004c1274` (type 0x20) is the pedestrian pool, not cops. DZHR actor events (`0x004520d0` → `0x004409e0`) spawn them, and their artwork is FAMILIES.RSC category 2 (family 101 is the woman on crutches). See REVERSE_ENGINEERING.md "Pedestrians" and plan 24 Task 4. The oil-slick finding stands.

Original verdict: **no pedestrians and no oil slicks exist in this binary.**
Every pool, every `+0xc4` type test, every template, the scripted-spawn
path, the surface system and the string table were censused; there is no
slot for either hazard. Details + how-confirmed per claim below.

## 1. Pool census (every `0x00438b70` call site in `analysis/decompiled/`)

| Pool global | Size x count | Template | Type | Owner |
| --- | --- | --- | --- | --- |
| `DAT_00463ad0` (`00405470`) | 0x32c x `DAT_004642d4` | caller-supplied (rider halves) | varies | riders |
| `DAT_00463ad4` (`00405490`) | 0x208 x `DAT_004642d4` | caller-supplied | varies | riders |
| `DAT_00468ee0` (`0041ebf0`) | 0x108 x 0x28 = 40 | `0x00468f30` | `0x40` | signals |
| `DAT_00468ee4` (`0041ebf0`) | 0x114 x 10 | `0x00468f10` | `0x400` | intersection controllers |
| `DAT_004c1274` (`00440640`) | 0x468 x 0xf = 15 | `0x0046dde0` | `0x20` | pedestrians (was mislabelled cops) |
| `DAT_0049e654` (`0044db80`) | 0x210 x 0x10 = 16 | `0x00475698` | `0x10` | cars |
| `DAT_004b6e68` (`00450d00`) | 0x18 x 4 | none | none | ring/sample cursor structs |
| `DAT_00476ce8` (`004552e0`) | 0xec x 0x14 = 20 | `0x00476cb8` | 1 | BORR solid objects |

How confirmed: `grep 438b70 analysis/decompiled/*.c` — 8 call sites, all
listed. No other pool exists. A pedestrian or slick would need a pool or
a carve-out of an existing one; the car pool spawns only template
`0x00475698` (type `0x10`), the BORR pool only template `0x00476cb8`
(type 1, no draw/think).

## 2. Template census (every `0x00438930`/`0x004387a0` user)

- `0x00468f10` -> type `0x400`, think `0x0041f040`, teardown `0x0041f1f0`
  (EXE words 4/6 read directly). Controller.
- `0x00468f30` -> type `0x40`, draw `0x0041f220`, teardown `0x0041f1e0`
  (empty stub). Signal.
- `0x00475698` -> type `0x10` (EXE word 1). Cars incl. crossing kinds 1/3.
- `0x0046dde0` -> type `0x20` (EXE word 1). Cops.
- `0x00476cb8` -> type 1, no draw/think (EXE words 3+ = 0). BORR solids.
- `0x00463ad0/ad4` riders via `0x00405990` (caller templates).
- `0x00468f00` is NOT a template — it is a per-direction table indexed by
  kind (`0041ef60` writes entry `00468f34`, `004415b0`/`0044e710` read
  entries). Not an actor template.
- How confirmed: grep of both constructors + direct EXE word reads
  (image base 0x400000, section-mapped).

## 3. `+0xc4` type-test census — what type 2 is

- `== 1`: BORR solid objects (also `& 0x15` contact branch in
  `0x004046b0`: `0x15 = 1|4|0x10`).
- `== 2`: the rider's own bike / self reference — NOT a pedestrian.
  `0x004046b0` line 15: `if (*(iVar2+0xc4) == 2 || ...) goto no-contact`
  — the first thing rider-vs-bike contact does is ignore type 2 (self).
  The only other `== 2` test is `004310e0` on `DAT_0046c4d0` (a global
  mode flag, not an object type). No think, draw, move or spawn path
  creates or handles a type-2 actor.
- `== 4`: bikes. `== 8`: rider halves (on-foot contact).
- `== 0x10`: cars (incl. crossing). `== 0x20`: cops.
- `== 0x40`: signals; `== 0x400`: controllers.
- Mask tests `& 0x15`, `& 0x3c`, `& 0x3c0`, `& 0xffffc000` admit no other
  allocated type value.
- How confirmed: `+0xc4` pattern census over decompiles + `004046b0.c`.

## 4. RRAN-animated families vs DZHR motion/collision

- `0x00452060`/`0x00452230` are NOT scenery animators: `00452060` sets a
  section-iterator cursor, `00452230` advances it across sample
  boundaries. They move an iteration cursor, never an actor; they assign
  no motion and no collision span.
- BORR collision spans come from `0x00454240` line 136 (spawn into the
  BORR pool at `iVar4 << 8`), i.e. static course-builder placement — no
  animation, no family lookup.
- String scan of the EXE for FAMILIES/FAMILY/PED/SLICK/OIL/WALK/RRAN/DZHR
  returns 0 hits each. Pedestrian-looking City figures are baked scenery.
- NOT fully traced: which FAMILIES.RSC entries animate via RRAN is a
  disc-asset question outside the EXE; nothing in the EXE gives an
  animated family motion or a collision span.

## 5. Surface handling (`0x0043a610`, `0x0043a0c0`, `0x00438c70`)

- `0x0043a0c0` (surface_class) returns 0..3 only: 0 road, 1/2 shoulder
  variants, 3 deep off-road. Full body read — no oil/gravel/water class.
- `0x00438c70` traction reads grip `DAT_0046dce8[surface]`, steering
  `DAT_0046dcf8/8`, rough factors `DAT_0046dd28` — all indexed by the 0..3
  surface class. A low-grip "slick" surface would be class 3 (off-road
  drag), which already exists and is not an oil slick.
- `0x0043a610` (292 lines): edge-of-shoulder stop / crash-when-fast (the
  port's `race.cpp:888` stand-in). `grep oil|slick|ped` over all
  decompiles: no hits (only Win32 OVERLAPPED noise).
- How confirmed: `0043a0c0.c` full read, `00438c70.c` lines 1–40,
  `0043a610.c` region grep. Native `surface_class` in `src/bike.cpp`
  matches the 4-class semantics exactly.

## 6. `0x0044e220`/`0x0044e270` scripted spawns — car-only

- `0x0044e220`: looks up an existing type-`0x10` car by id and applies a
  `0x0044e5a0` update; else falls through to `0x0044e270`.
- `0x0044e270`: spawns from the car pool with the car template in both
  arms (longitudinal `0x80` and crossing kind). Kinds are car kinds only.
  No non-car payload: every object created here is type `0x10`.
- How confirmed: `0044e220.c` + `0044e270.c` lines 1–82.

## Verdict (for Task 4 Step 2)

Pedestrians and oil slicks do not exist in this binary: no pool, no
template, no type code, no spawn path, no surface class, no asset string.
Per the plan, say so in PROGRESS.md and close the task — do not build
either hazard. The only open (non-blocking) item: enumerating which
FAMILIES.RSC entries RRAN-animates, to document that City figures are
scenery; that is asset confirmation, not a hazard.
