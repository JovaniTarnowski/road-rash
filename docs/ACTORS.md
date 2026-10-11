# Actor census

Static census of the original's road actors (plan 05 task 1, step 2), built
from the decompiled corpus, the disassembly and the reference executable's
data tables. Golden object dumps (step 1) are still open: the oracle has not
yet captured a traffic-bearing scenario, so no tick-level verification of
car behaviour exists yet.

## Vehicle resources (per course)

`0x004465f0` loads `DATA/CARS/<CourseName>.CAR` (course names from the
selector table `0x0046df58`, indexed by the active course `0x00467a6d`):

- Vehicle i pairs the i-th `ANIM` (RRAN, relocated by `0x0044a200`) with the
  i-th `CANS` wrapper when present; the loop stops at the first missing
  `ANIM` (i+1), so the vehicle count `DAT_00474d88` equals the number of
  `ANIM` entries (10 for NAPA).
- `CANS` payloads are `SNAC` wrappers: magic `SNAC`, size word, type word
  (+8), an embedded sub-container at +size, and a cel block reachable through
  the `0x0040b5f0` chain `base + sub[+4] + 0xc`. The vehicle mask
  `DAT_004b8fe0[i]` is the cel block's CCB flags word at +4 (`0x0040b6f0`
  via `0x0040b620`).
- Mask bits `0x1c0000` are the speed class: `0x40000` slow/parked,
  `0x80000` oncoming-capable, `0x100000` same-direction-capable. Measured
  NAPA masks: `0x110111`, `0x90109`, `0x50411`, ... (speed classes
  same-direction, oncoming, slow respectively).
- Vehicles without a `CANS` keep the zero-initialised mask (native
  approximation: accepted for every class; the original's fallback slot is
  static junk data whose exact behaviour is unrecovered).

## Car object (0x210 bytes, pool 0x00438b70(0x210, 0x10) = 16)

Template `0x00475698` (copied by `0x004387a0`/`0x00438930`):

| Template word | Object offset | Meaning |
| --- | --- | --- |
| +0x00 | +0xc0 | kind byte 3 (vehicle) |
| +0x04 | +0xc4 | mask 0x10 (car LOD branch in `0x00437e60`) |
| +0x08 | +0x14 | LOD init 10 |
| +0x0c | +0xdc | aux dispatch `0x00446830` |
| +0x10 | +0xe0 | think `0x0044e710` |
| +0x14 | +0xe4 | move `0x0043adc0` |
| +0x18 | +0xe8 | slice change: none (despawn uses `0x00437c10`) |
| +0x28 | +0x160 | 10: sprite size class (drawing) |
| +0x44.. | +0x174.. | per-kind words from `0x00475700 + kind*0x18` |

Per-kind words (`0x00475700`, 6 per kind): kind 0/2 `0x4000`, kind 1/3
`0xa000` (following-gap offsets, `>>8` = 64/160 samples), plus look-ahead
windows `0x00468f00` = `0x40/0x80/0x100/0x200` per kind.

Key object offsets: position along the course `+0x1c` (Q8 samples),
lateral `+0x28` (Q8 world), forward speed `+0xec`, lane `+0x7a`, kind
`+0x73`, vehicle `+0x7f`, phase `+0x71` (int slot, 192..319), serial
`+0x82` (`DAT_004b7e98`), animation state `+0x1c` block, animation
templates `+0x70/+0x74/+0x78` (cel, RRAN, mip table from `0x004467e0`).

## Spawning (0x0044dd90, driven per frame by 0x0044b3b0)

Ring cursors (`0x004552e0`, scrolled by `0x00454120`):
`DAT_004b6e50` = front (camera + 0x5000), `DAT_0049f240` = back
(camera - 0x3000); the ring spans 0x8000 (128 samples).

| Timer | Address | Decay | Spawn |
| --- | --- | --- | --- |
| oncoming | `0x004b7e90` | rate/256 + distance (runs while stopped) | kind 2 at front-1, lane `-1 - rand % left_lanes` (sub-slice +0x30) |
| behind | `0x004b7e94` | distance - rate/256 (runs while moving) | kind 0 at front-1, lane `rand % right_lanes` (sub-slice +0x2c) |
| ahead | `0x004b7e80` | distance - rate/256, spawns when positive (rider slow) | kind 0 at back+1, right lanes |

Rate 2500 = 0x19*100 (`0x0044dd00(-25)`), density default 6; reloads =
`(rate*density*60) >> 8` rounded toward zero. `0x0044df60` places the car:
lateral `(road centre + lane*250 + 125) * 256`, speed-class filter
`(mask & 0x1c0000) == 0 || (mask & filter)` with filter 0x40000/0x80000/0x100000
by kind, vehicle pick `rand() % accepted` (the type loop excludes the last
vehicle), four random bytes (phase, jitter), serial++.

## Behaviour

- Think `0x0044e710`: window check `0x0044dc10` (only riders with the
  knock-off flag `+0x1cc` park inside the window; free cars pass), obstacle
  search `0x0044a070`/`0x00449e60`, lane changes, speed control through the
  solver `0x00449a80` and accumulator `0x004498c0` (`+0xec += dt * rate`,
  clamped to the phase-scaled target `((25 * phase) >> 8) * 100`), curve
  compensation `0x00449880`/`0x0044f5a0`, drift rate
  `0x00475694[kind] low byte * 800`, animation LOD by distance
  (`0x00449c20` vs 0xa01/0x501) and advancement `0x0043ae10`/`0x0043ae90`.
- Move `0x0043adc0`: `0x004390c0` speed integration, `0x00439920`
  forward/lateral decomposition (lateral delta `+0x128`), `0x0043a610`
  walls; cars skip the vertical motion (`0x0043a190`) because their mask
  is 0x10.
- Secondary `0x0044efa0` (template word +0x58): horn `0x00436830(0x12)` when
  the car's look-ahead target is the player and `rand()%10==1`.
- Despawn `0x00437e60` tail: outside `[back, front)` the object parks via
  `0x00437c10` (returns it to the free pool).

## Not ported yet

- Obstacle avoidance/following (`0x0044a070`, `0x00449e60`, solver target
  logic) and lane changes: native cars keep their spawn lane and cruise.
- Scripted DZHR moving vehicles (`0x0044e270`), including police cars.
- Rider-car and car-car collisions (`0x004049c0` non-rider branches).
- Animation advancement `0x0043ae10`/`0x0043ae90` (native frame cycling is a
  stand-in) and the CCB world-size scaling law for cars.
- Police (`LOCOP.DAT`), arrests and the `BUSTED*.AVI` selection.

October 11 actor update: live Napa vehicles now load imported EXE/CANS descriptors and original view cells, use camera-distance code selection and size-0x500 actor projection, and merge with riders/scenery. The earlier animation/scaling stand-in bullets apply only to the legacy mask-only inspection path. Obstacle/traffic movement, scripted vehicles, collision and police bullets remain open. Live crash actors now separate bike/rider motion and render both through imported playback; whole-frame ordering and split interpolation still require original goldens.
