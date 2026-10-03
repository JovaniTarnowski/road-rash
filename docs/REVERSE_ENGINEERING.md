# Binary evidence and next work

## Reference binary

`cdrom/ROADRASH/ROADRASH.EXE` on the supplied 1997 rerelease disc:

- SHA-256: `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
- 541,184 bytes, PE32 / i386, image base `0x00400000`.
- Entry RVA `0x583c0`; timestamp August 23, 1996.
- No debug directory; symbols and line numbers stripped.
- 1,585 Ghidra-discovered functions exported; no export timeouts/failures. Function boundaries and inferred types are not all manually validated.

Work uses this original disc executable, not the modified files in the pre-existing `android/` or Wine prefix outside `native-port/`.

## Resource container

Observed in 13 files, including bike specifications, five courses, five car resources, families and global data. All integer fields below are little-endian. Tags appear reversed as ASCII on disk because they were stored as integer FourCC values.

| Offset | Field | Evidence |
| --- | --- | --- |
| `0x00` | `CRSR` magic (integer `RSRC`) | All 13 containers |
| `0x04` | Header value `0x30` | Observed; not yet assigned full semantics |
| `0x0c` | `LBTR` tag (integer `RTBL`) | All 13 containers |
| `0x10` | Absolute directory offset | Header reader at `0x00414ca0`, confirmed with assets |
| `0x14` | Directory byte extent | Header reader at `0x00414ca0` |

At the directory offset: `LBTR`, a length word, entry count at `+8`, and a runtime link at `+12`. Entries start at `+16`, with a 32-byte stride. Original runtime fixup loop `0x00415360` confirms the count and stride.

| Entry offset | Field |
| --- | --- |
| `+0` | FourCC integer |
| `+4` | Resource ID |
| `+8` | Absolute payload offset |
| `+12` | Payload byte length |
| `+16` | Flags; original loader tests bits `0x100` and `1` |
| `+20` | Runtime payload pointer/cache slot |
| `+24`, `+28` | Not yet identified |

Native code currently returns validated directory metadata. It does not implement nested payload decoding, compression, caching, or original runtime fixups. It deliberately treats file offsets and host pointers as different types.

`BIKESPEC.RSC` contains 15 `SPEC` resources, IDs 1–15, each 356 bytes. Original function `0x00405e30` explicitly loops over those 15 IDs; `0x00405ea0` consumes the records and applies mass-dependent calculations. Field meanings beyond the observed offsets need further reconstruction.

`NAPA.CRS` contains two `CEL ` resources plus `NOD ` and `SGS ` resources. The segment payload starts with bytes `SGSR` (integer tag `RSGS`), and the node payload starts with bytes `DONR` (integer tag `RNOD`).

## Course geometry recovered

Native reader: `src/course.cpp`. Validated against all five courses (`rr-course`): every node reachable, node count matches header, all lane intervals ordered.

- `SGS ` payload: `SGSR` chunk (16-byte header, count at `+8`), one 52-byte row per section. Each row points at per-section chunks, each walked by one iterator that `0x00452cb0` sets up in the traversal half. Tags below are as stored on disk; the in-memory integer tag is the reverse (e.g. `RPTH`).

| Row offset | Chunk tag (disk) | Sections having it | Iterator (half offset, init / step) | Status |
| --- | --- | --- | --- | --- |
| `+0x04` | `HTPR` path | 109 | `+0x2c`, `0x00451970` / `0x004519f0` | Decoded |
| `+0x08` | `BORR` | 108 | `+0x84`, `0x00451b60` / `0x00451e20`; per-record flag word unpacked by `0x00451a90` | Not decoded |
| `+0x0c` | `MSRR` | 109 | `+0x70`, `0x004522a0` / `0x00452580` | Not decoded |
| `+0x10` | `DLBR` / `LIHR` / `NTMR` (building / hill / mountain) | 34 / 64 / 11 | `+0xb4`, `0x004510c0` / `0x004516e0`; `0x00451910` dispatches on the tag and sets sub-slice byte `+0x3c`, the projection case in `0x00444630` | Type mapping traced (below); payload not decoded |
| `+0x14` | `DLSR` sides | 109 | `+0x38`, `0x00452a00` / `0x00452b70` | Decoded; widths rendered, heights decoded but unused |
| `+0x18` | `NALR` lanes | 109 | `+0x10`, `0x00452820` / `0x00452920` | Decoded |
| `+0x1c` | `DZHR` | 109 | `+0x1b8`, `0x00452060` / `0x00452230` | Not decoded |
| `+0x24` | `CESR` | 97 | `+0x5c`, `0x00452610` / `0x00452690`; `0x00452710` derives sub-slice flags | Not decoded |
- `HTPR`: 16-byte header, then two signed bytes per sample: **bend** then **rise**. Header `int16` at `+0xc` is the running elevation at the section start, `+0xe` at the end (`0x00451970`). In 97 of 109 sections, start + Σrise = end; the other 12 differ, so the header end value is not trusted.
- **Rise is a height delta** per sample: `0x004519f0` accumulates it once into elevation.
- **Bend is curvature**: the projector (`0x00443d70`) adds bend into a lateral slope, which then adds into lateral offset. Split tracking (`0x00453030`) likewise double-integrates the bend difference between branches.
- `NALR`: 20-byte records `{start, end, left0, right0, left1, right1 (bytes), Q16 left slope, Q16 right slope}`. Walked by `0x00452820`/`0x00452920`; absent table means width `0x200` (two lanes each side, `0x004528f0`). `0x00454240` converts lane units to world units as `trunc(q8 × 250 / 256)`: a lane is 250 world units.
- `DLSR` (sides): 16-byte header (count at `+8`), then 36-byte records:

  | Offset | Field |
  | --- | --- |
  | `+0`, `+4` | Start and end sample |
  | `+8`, `+9` | Pair A (left, right) at start |
  | `+10`, `+11` | Pair A at end |
  | `+12`, `+13` | Pair B (left, right) at start |
  | `+14`, `+15` | Pair B at end |
  | `+16`, `+17` | Two bytes copied to sub-slice `+0x14a`/`+0x14b` (observed 0, 64, 128, 192, plus a few odd values) |
  | `+20`…`+32` | Four Q16 per-sample slopes: A left, A right, B left, B right |

  Interpolation and stepping match `NALR`. Values are `byte << 16` and are read back `>> 16` (`0x00452b00`), so they are plain world units.
  - Pair A widens the road: `0x00454240` sets sub-slice `[0x3a] = left edge − A.left` and `[0x3b] = right edge + A.right`. `0x00444630` projects these to screen `+0x60`/`+0x68`. Pair A is therefore the shoulder width.
  - Pair B is a height: for sub-slice type 0, `0x00444630` sets rows `+0x54`/`+0x5c = surface row − B·S`.
  - Checked on all 109 sections (389 records): extent = 16 + 36·n everywhere; all 261 interpolating records' slopes equal `(end − start byte)·65536/(end − start sample)`; 274 of 280 records start where the previous ended.
  - Common widths: 224 (791 bytes), 255, 200, 250, 150, 112. Heights are mostly 0, then 20, 18, 25, 15, 10.
  - The meaning of the flag bytes at `+16`/`+17`, and how type ≠ 0 sub-slices use pair B, is not yet established.
  - Native reader: `course.cpp` walks `DLSR` into `PathSample::shoulder_*`, `height_*` and `side_flags`. `road.cpp` projects the outer shoulder edges (`RoadSurface::shoulder_left/right`), and the viewer draws them. Heights are carried but not drawn, since the original applies them only for type 0 sub-slices.
- **Interval tables (`NALR`, `DLSR`) are walked exactly as the iterators step them** (`walk_intervals` in `course.cpp`).
  - Values are seeded from record 0's start bytes at sample 0.
  - Stepping to sample `j`: `j < end` adds the slopes, `j == end` loads the end bytes, and past the end the next record is entered only when `j` equals its start, keeping the current values.
  - This replaced an earlier closed-form lookup that reset to a record's start bytes. The two differ on 6 samples (HIWAY section 6, whose first lane record starts at sample 1); the walker matches `0x00452920`.
- Terrain type chunk (row `+0x10`) → sub-slice `+0x3c` (`0x00451910`):
  - `RBLD` (building) → type 0 (`0x004517b0`). This is the only type that uses `DLSR` pair B heights, as rows `+0x54`/`+0x5c` in `0x00444630`.
  - `RMTN` and `RDWD` → type 1 (`0x00451710`), which builds wall/cliff profile fields `+0x14c`…`+0x160` from tables in the chunk.
  - `RHIL` → type 4 (`0x004517e0`), with three control points per side (`+0xf0`…) and 0–31 shading gradients.
  - Types 2/3 are assigned elsewhere (the split-road handling in `0x00454240` uses 3).
  - Each type copies two bytes per sample into `+0x148`/`+0x149`, probably colour or texture selectors; not yet confirmed.
- `NOD `: `DONR` header (size at `+4`, node count at `+8`, root at `+16`). Kinds 0 (road, section index + length), 1 (split), 2 (merge), 3/4 terminal (`0x00447b80`).

## Road projection recovered

Native port: `src/road.cpp`, tested by `tests/road_test.cpp`.

- Road samples live in a 128-slot ring (`0x0049e820`), 80 samples ahead and 48 behind the player, filled by `0x00454c80` → `0x00454240`.
- `0x00443a00` → `0x00443d70` projects `0x21` (33) slices ahead of the camera position `0x4bc7e4` (Q8 samples). Initial state for fraction `f`: `dx = trunc(bend·(256−f)/256)`, `y = −((f·rise) >> 8)`, `x = 0`, `z = 256 − f`.
- Per slice (`0x00444630`): `S = table[z/2]`, `screenY = (camY − y)·S + 90<<16`, `screenX = (x + edge − camX)·S + (320/2)<<16`; then `z += 256`, `x += dx`, `y += rise`; the caller adds the next sample's bend to `dx`.
- View space is fixed at 320×240 (`0x46667c`/`0x466678` are never written) with the horizon on row 90.
- `S` is a static, non-linear Q16 table at `0x0046f098`: 4,352 non-increasing entries, flat 163,840 up to depth 276. The port reads it from the user's `ROADRASH.EXE` and rejects other binaries.
- Camera: `0x00437d80` copies `0x4b7ee0` into `0x4bc7d0`. Height is bike `+0x24` plus a cinematic offset, minimum 150 (`0x96`), and race init (`0x0044b250`) uses 150. Position is one sample (`0x100`) behind the bike.

## Splits and merges recovered

Native port: `make_route` in `src/course.cpp` (traversal) and `project_road` in `src/road.cpp` (projection).

- **Node fields:** a split (kind 1) has outgoing roads at `+8` (half 0) and `+12` (half 1) and the incoming road at `+16`. A merge (kind 2) is the reverse. `+0x14` is `0x3c00` on disc, but `0x00453b60` rewrites it to `0x5100` (81 samples) at load, after walking 81 samples from the node.
- **Lookahead traversal** (`0x00453030`, instances `0x49ee30` front and `0x49ea20` back) carries two cursors (`+0x28`, `+0x21c`), a primary half `[1]`, and the separation of half 1 from half 0: slope `[5]` and lateral offset `[6]`.
  - Forward step in a dual stretch: move both cursors, `[6] += [5]`, then `[5] += bend₁ − bend₀` at the new samples. The backward step is its exact inverse.
- **At a split:** `[5] = bend₁ − bend₀`, `[6] = 0`. Half 0's centre shifts so its left edge matches the incoming road; half 1's so its right edge matches (`trunc(Δq8·250/256)`).
- **Collapse:** once the rider has committed (`0x00476cd4`, set by `0x00439390` when the bike leaves the overlap), the other road is dropped (`0x00452fd0`) when the primary passes local sample 81 or enters a new road node.
- **Merge snapshot:** `0x00453b60` walks 81 samples backward from each merge and stores the traversal at `node+0x1c`. When a single-road traversal is 81 samples short of a merge (`0x00453030`, event 6), it restores that snapshot, so the other road converges and the separation reaches zero at the merge. The kept edge is realigned when the roads join (event 1).
- **Projection** (`0x00443d70`):
  - Both halves integrate their own bend and rise.
  - When the camera starts inside a dual stretch, the other half starts from the slice's `[9]`/`[10]` (the traversal's `[6]`/`[5]`).
  - Ahead, a split's second road starts as a copy of the current road; a merge window's is offset by the stored separation.
  - Vertical separation is never tracked. The branches have independent height profiles (NAPA branches differ by up to 3,369 units within 40 samples of a merge), so the other road is drawn relative to the camera's road height.

Validated on all five courses (`rr-course`): 11 split/merge pairs per route pair, every merge window restored, separation zero one sample before every merge.

**Preview simplifications:**
- The rider is assumed already committed to the selected branch, so the second road is dropped 82 samples after a split. In the game this depends on the rider's lateral position.
- When a collapse keeps half 1, its next bend comes from the kept road. The original reads a stale ring sub-slice there.
- The merge snapshot uses the last sample's lane width where the original uses the terminal lane record.

**Not yet ported:**
- Shoulder heights (pair B) and shoulder blending at splits (`0x00454890`)
- Terrain profiles for buildings, mountains and hills (`0x00451710`, `0x004517b0`, `0x004517e0`)
- Road/verge colours and textures
- Horizon placement, `CEL ` scenery and sprites, occlusion clipping (`0x4c10c8`)
- Road types from the `+0x10` chunk (`0x00451910`), and the `BORR`, `MSRR`, `DZHR`, `CESR` chunks
- Bike-driven camera x and height

## Riders and bike physics recovered

Native port: `src/bike.cpp`, `src/race.cpp`.

- **Pair construction:** `0x00405600` creates the racers through `0x00405990`. Each is a **bike** object (table `0x00463ba8`, AI `0x00463ad8`) plus a **rider** object (`0x00463c10`, AI `0x00463b40`), linked through `bike+0x2fc` and `rider+0x1c4`.
  - `0x00438930`/`0x004387a0` copy table words into the object: word 0 → `+0xc0`, 1 → `+0xc4` (type, 4 = bike), 3–6 → `+0xdc`/`+0xe0`/`+0xe4`/`+0xe8`, 7.. → `+0x148` grip, `+0x14c` skid grip, `+0x150` aero drag, `+0x154` rolling drag, `+0x160` ground contact, and so on.
  - Bike: think `0x00407a90`, move `0x004060b0`, slice change `0x00406940`. Rider: think `0x00434660` (player) or `0x00407bd0` (AI), move `0x00406890`, slice change `0x00406b40`. `0x00405600` sets some riders' think to `0x00407d40`.
- **`SPEC`** (`BIKESPEC.RSC`, 15 × 356 bytes), applied by `0x00405ea0`:
  - byte 0 → `+0x1bc`; dwords 1–5 → `+0x168`, `+0x16c`, `+0x170`, `+0x1b4`, `+0x1b8`; dword 6 = mass; dwords 7–82 → `+0x1c4…+0x2f3`
  - the gear table is at `+0x1d0 + 16·g` (drive factor, upshift speed, downshift speed), 7 gears
  - derived values: maximum throttle `+0x260` and brake `+0x27c` scale with mass and the rider value; throttle/brake ramp rates `+0x264`/`+0x268`/`+0x280`/`+0x284`; steering limit `+0x29c = 0x2e00` and rates `+0x2a8`/`+0x2ac`
- **Controls:** `0x0044f800` combines `0x0044fa80` (keyboard via the `0x004c5d90` key → action table) and the joystick into `0x004b7ed0`. Bits in `0x00476c00` order: `0x1` accelerate, `0x2` brake, `0x4`/`0x200` steer, `0x8`/`0x400` lean, `0x10` nitro, `0x20`/`0x40`/`0x80` attacks, `0x100` dismount. `0x00434660` applies them through:
  - `0x0040acd0` (coast/throttle/brake), `0x0040adb0` throttle, `0x0040aea0` brake
  - `0x0040ad30` → `0x0040af50` steering: target from `sin(input)·speed/+0x2b0`, clamped to ±`+0x29c`, ramped
- **Integration, once per tick** (`0x0043a580`):
  - `0x00438c70` traction: grip from the surface table `0x0046dce8`, skid when demand exceeds grip, steering damping `0x0046dd08`/gain `0x0046dcf8`, rough-surface curve factors `0x0046dd28`
  - `0x004390c0` speed: `v += dt·(drive − drag − |lateral|/16)`, drag = aero·(v>>8)>>8 + surface drag `0x0046dce4` + rolling
  - `0x004391d0` gear shift
  - `0x004395e0` movement: distance `(2v − a·dt)·dt/2 >> 8`; curves push outward via the bend swept (`0x004394c0`, `×400/dt`); lateral `+0x28 += dt·+0x10c`
  - `0x0043a610` wall/terrain collision (not ported)
  - `0x0043a190` vertical motion over ground height `0x00439b10`: crests launch the bike when the ground drops faster than its vertical speed; landing rebound and wobble
- **Surface class** (`0x0043a0c0`): 0 road, 1 shoulder (type 0 terrain), 2 rough, 3 beyond the shoulder.
- **Units:**
  - speed `+0xec`: upshift points 3,000–21,504, top ≈ 11,800–16,600 in our runs
  - position `+0x1c`: Q8 samples
  - lateral `+0x28`: world units × 256
- **Timing:** 60 ticks/s from the performance counter (`0x004163d0`), capped at 20 steps per frame.
- **Camera** (`0x00437c30`): centred on the road with the rider offset ¾, clamped to ±200; height = rider height + crash lift (to 300), minimum 150; position = rider − crash pull-back (to 900) − `0x100`.
- **Grid** (`0x00464218`/`0x00464258`): 8 slots at samples 50/51 with lateral offsets 16384, −3616, −23616, −43616 (added to the road offset).
- **Math helpers:** `0x0044f5a0` is a piecewise-linear atan2 (256 per turn); `0x0044f4a0` is a quarter-wave sine table at `0x00475768` (1,024 entries, Q15, 4,096 per turn).

## Rider sprites, AI and combat recovered

- **Sprites:**
  - `0x00410160` loads `Data/Bikers/Template` (MIP, not decoded).
  - `0x00443610` loads, through `0x004436a0`, bob (326), cop (45), shadow (326) and shadow-C (45) frame sets from `Data/Bikers/Hi*` (falling back to `Lo*`). Frame layout: width byte, height byte, indices.
  - `0x00443820` copies a frame into a 256-wide sprite page, remapping each pixel through `0x0046e158 + rider*256`. Ids `0xde`/`0xdf`/`0xe0` select shadow frames.
  - Index 0 is transparent.
- **Animation (not ported):**
  - Each object has an anim struct at `+0x70`, with `+8` → a table of animations by code (`0x00474d78[type]`, filled at runtime by `0x00445a60` from `GLOBAL.RSC` `CANS` 1–2 and `ANIM` 1–4 through `0x0040b6a0`).
  - Frame records are 20 bytes: duration at `+0xc`, loop flags at `+0xd`. `0x0040ac70` sets the animation; `0x0043ae90` advances it.
- **AI personalities:** 10 records of `0x204` bytes at `0x00464558`, copied to `0x004c6b40`. Each holds per-level target speeds at `+0x1bc` (5 levels; e.g. rider 1: 99/115/130/150/185) and a lane preference at `+0x18c`. `0x004078f0` also makes 6 copies with ±6 random variation.
- **AI driving:**
  - Target point: `+0x194` lateral, `+0x198` = position + `0x3c0`, `+0x19c` speed.
  - Speed compare: `(v>>12)+(v>>7)+(v>>9)` against the target.
  - Curvature: `0x00449b40` averages sub-slice `+0xb4` (atan(bend) << 14).
  - Re-plan `0x004082f0`: resets `+0x1a4` from the level speed, then adjusts it by the rubber band (when the player is the next rider in race order) or by fixed factors.
- **Rider table** `0x00463c78` (`0x40` bytes per rider):
  - word 3 = rider value × 2
  - word 5 = bike/colour data
  - words 6–10 = weapon per level
  - words 12–15 = strength, base, maximum and current stamina
- **Contact:** `0x004368f0` runs from the scheduler. Pairs overlap when within the half-lengths `+0x178`, the half-widths `+0x174` and the heights `+0x17c`, and on the same road-half bits. `0x00436a40` separates them along the axis that was clear on the previous tick, scaled by mass `+0x194`, and records impulses `+0x184`/`+0x188`/`+0x18c` and contact flags `+0x19c`; then each object's `+0x1a4` handler runs (bike: `0x004049c0`).
- **Combat:**
  - Control bits `0x20` strike, `0x40` kick, `0x80` backhand.
  - Attack animation codes come from `0x004644f8`/`0x00464508`/`0x00464518`/`0x00464528` (weapon + variant × 4; left or right).
  - The hit lands on frames flagged `0x600000`.
  - Reach (`0x00409c10`): |dz| < `0x40`, |dx| < reach × `0x1000`, |dy| < `0x3000`.
  - Damage: max(maximum/base × strength, strength/2) × weapon multiplier / 256.
  - Stamina and bike condition: the maximum drops by damage×4 and the current by damage×64; kicks also wear the bike (`+0x2f0`) by damage/4 + 1. Either reaching zero knocks the rider off (`0x00403d60`).
  - Hit cooldown: (5 − level) × 6 ticks.

## Original image loading recovered

- `.RRI` files begin with JPEG/JFIF headers. Six menu files decode to 640×480.
- `0x0044c5d0` calls the raw indexed-image loader with `0xbe` × `0x40`: 190×64. `BIGMETER.BOB` is exactly 12,160 bytes.
- `0x0044c5f0` reads one byte per pixel and copies scanlines into locked DirectDraw surfaces. Low-resolution mode halves both dimensions.
- `0x0044c440` sets the high-resolution horizon row width to `0x2d0` (720) and splits each row into two equal halves. High-resolution horizon files are 230,400 bytes, giving 320 rows. The viewer displays the combined source image, not the original game's scrolling/cropping behavior.
- `0x0044c730` reads exactly 1,024 palette bytes. `0x00448f90` forces the first entry's RGB components to zero and constructs a half-brightness palette.
- `0x0041e710` converts the palette to BMP ordering by exchanging components 0 and 2, corroborating RGB-plus-flags ordering in the source palette.

## Function map

Names here describe observed behavior; they are not recovered original symbols.

| Address | Observed role | Native status |
| --- | --- | --- |
| `0x004583c0` | CRT startup calling `0x00448200` | Replaced by native entry |
| `0x00448200` | Windows startup, menus, race transition | Not ported |
| `0x004163d0` | Race loop, counter-based pacing, update/render calls | Not ported |
| `0x00442e10` | Update function called by race loop with tick count | Needs detailed analysis |
| `0x004439f0` | Frame routine called after update | Road projection path ported |
| `0x00443d70` | Build 33 projected road slices | Single and dual-road paths ported |
| `0x00453030` | Lookahead traversal, split/merge state | Replayed along route (`make_route`) |
| `0x00453b60` | Precompute 81-sample merge snapshots | Ported as backward walk |
| `0x00444630` | Project one road slice | Road surface/edges ported |
| `0x00454240` | Fill ring slice from path/lane iterators | Widths and bend/rise ported |
| `0x00452a00` / `0x00452b70` / `0x00452b00` | `DLSR` sides iterator | Ported (`walk_intervals`) |
| `0x00451910` | Terrain-type dispatch → sub-slice type | Mapping traced |
| `0x00452cb0` / `0x00452e10` | Initialise / step all section iterators of a traversal half | Mapped (row-chunk table) |
| `0x00416310` | Select and load horizon, meter, dashboard | Asset selection traced |
| `0x00441940` | Course resource load/fixup | Sections, lanes and node graph decoded |
| `0x00405e30` | Load 15 bike specs | Directory reader only |
| `0x00405ea0` | Apply bike spec to runtime bike fields | Ported (`apply_spec`) |
| `0x0043a580` | Per-tick bike physics | Ported except wall collision `0x0043a610` |
| `0x00437c30` | Chase camera | Ported (single road) |
| `0x00434660` | Player rider think (controls) | Riding part ported |
| `0x00414ca0` | Open resource container and load table | Native directory reader |
| `0x00415360` | Iterate table and optionally preload | Layout recovered |
| `0x004153a0` | Find/load/cache resource payload | Not ported |
| `0x00415470` | Search through resource collections | Not ported |
| `0x004154d0` | Binary search table by tag and ID | Not ported |
| `0x00415610` | Read payload from file/backing memory | Layout corroboration |

Some decompiled return types are visibly wrong (e.g. functions returning registers inferred as `void`), and imported runtime labels such as `__write` are not trustworthy. Check assembly and call sites before translating these functions.

## Next milestones

1. Correct signatures and export caller/callee references for the course loader, tick update and frame routines. Establish explicit fixed-width structures and signed-overflow behavior.
2. ~~Decode `SGS ` and `NOD `; render original road geometry~~ (done, single-road path). Split/merge roads also done. Shoulder widths also done. Remaining work: terrain profiles, shoulder heights and road colours; decode `CEL ` scenery; compare against Wine captures of the original at matching positions.
3. Reconstruct one bike's spec application, acceleration, steering and collision behavior. Add deterministic recorded-input checks before expanding to AI and combat.
4. Recover sprite compression/animation and compose bike, scenery and HUD layers with the native renderer.
5. Implement audio, input bindings, original menus, persistence and platform lifecycle behavior; package standalone macOS and Android builds.

No playable-game completion date is established. Asset viewing is the first validated milestone, not a substitute for recovering the simulation.

## Original frame oracle and course selector (October 2, 2026)

- `0x004439f0` calls road setup, display-list render and dashboard presentation with no arguments. Its four E8 call sites at `0x004163fa`, `0x00416406`, `0x0041656d`, `0x00443249` are verified against the reference binary before interception.
- After it returns, 33 projected entries at `0x004bc800` have 548-byte stride. Entry dwords 1/2 are kind/sample; dwords 5/6 point at projected road halves. Logged fields follow `0x00444630`, including depth/scale at `+0x18/+0x20`, centre/surface row at `+0x48/+0x4c`, road edges `+0x50/+0x58`, and shoulders `+0x60/+0x68`. The sub-slice pointer at `+0x0c` yields type byte `+0x3c`.
- The original course index (`0x00467a6d`) uses the string pointer table at `0x0046df58` in `0x00441940`: CITY, HIWAY, MEDLY, NAPA, CANYN. Horizon selection separately uses `0x00467aa0` in `0x00416310`: City, Sierra, Penin, Napa, PCH. These indexes must not be confused with the native CLI ordering.
- `analysis/oracle/napa-attract.tsv` contains 600 CITY frames; the scenario label predates the recovered ordering. Native comparison matches all measured numeric fields without tolerance. Coverage is limited to samples 49–74, so split/merge assumptions remain unverified.

## ISO data import (October 2, 2026)

The actual rerelease ISO uses 2048-byte blocks. Its primary volume descriptor at sector 16 reports 252,822 blocks; root directory is sector 24 and ROADRASH is sector 26. Both-endian extents/lengths, sector-contained directory records, `;1` version suffixes and directory flags decode natively. Files are checked against the existing 341-entry SHA-256 inventory before a staging directory is renamed into use. Synthetic tests cover invalid extents, truncated volumes, inconsistent endian fields and directory cycles. All 341 real files import and the viewer decodes successfully from the imported copy. SDL's Android picker supplies content URIs; random access through SDL_IOFromFile is built but not physically validated.
