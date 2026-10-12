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
- **Grid:** `0x00464218`/`0x00464258` hold 8 (lateral, sub-slice) slots at samples 50/51 with lateral offsets 16384, −3616, −23616, −43616 (added to the road offset); used when the field is small. Level < 4 full-field construction (`0x00464088`, read from the EXE) places 15 pairs: the race field at consecutive samples 50–57 (slots 0, 9, 4, 10, 2, 11, 3, 12 by position) plus 7 extra bikes parked ahead at 21248–31488. Solo oracle captures freeze all 14 non-player pairs on the first step (`sim-kink-probe7`). Native two-row placement does not match; see the October 12 VERIFICATION.md entries.
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
  - Re-plan `0x004082f0` (full body recovered October 12, 2026; native port still the old approximation): while `+0x188` > 0x3b, ease the 10-entry brain table (`+0x0..+0x78` toward `+0x78..+0xf0` by steps `+300/+0x130/+0x134`) and decrement 0x3c; pick neighbours `+0x16c/+0x170` via `0x00408cd0` ring scans (self-exclusion only when sharing the player's rider-half); rate overtake steps `+0x178/+0x17c` 0–3 from the rating table against `(0x10−iVar7)·1000/2000 − level·0x5dc/3000`; keep the nearer target in `+0x168/+0x174`; clear the target when it is a live chaser (`0x00432500` = move-nonnull, unless network state matches); reset `+0x1a4` from `+0x1bc[level]` and `+0x1a0` from `+0x1a8[level]`; speed cases on neighbour nullness vs the player (rubber-band table `0x004659d4` only when a neighbour IS the player; fixed ±fractions otherwise); halve (or ×3/2 at level 3) when the player is crashed and >0xa00 away (`0x00449c20` = absolute gap). Helpers: `0x00449ce0`/`0x00449e60` walk ring links `[0]/[1]` filtered by slice/think-flags, `0x00449e90` scores by closing rate, `0x00449da0` picks nearest by slice each side, `0x00441750` is CRT rand.
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
| `0x00434660` | Player rider think (controls) | Riding part ported; demo/mode AI branch ported (`Race::set_demo_hold`) |

Demo-player finding (October 11, 2026, commit `ca57525`): oracle captures launched
through menu course entry run the `0x00434660` head's AI branch (hold or
race-not-started mode), so the recorded control words are never read. Evidence:
the spare rider-half node keeps think `0x00434660` while bike `+0x288`/`+0xf0`
sit at 0 for 120 ticks under controls=5. The demo rider is character 1's rider 8
(bike SPEC id 2 by condition 142, value 91, weapon 2, stats 7/3000, base speed 95
= personality 8 level-0). Lane shortlist {0,1} (lane 2 diverges at frame 72);
bias = slot-0 grid word 16384. Remaining: lane final fit, and the tick-184 AI
state transition (now traced to the state 1→5 switch below).

AI-brain finding (October 12, 2026): the rider half (`*(bike+0x2fc)`) is not the
AI state. Word `rider_half+0x1d8` points at a separate brain struct that
`0x00407bd0` steps. Offsets confirmed by live dumps (`rider.<slot>.brain_<off>`
in the sim golden, `sim-kink-probe3/4`): `+0x160` state, `+0x164` skid,
`+0x168` target object, `+0x16c/+0x170` neighbours, `+0x174` overtake step,
`+0x178` per-state timer, `+0x188` replan accumulator, `+0x18c` lane, `+0x190`
bias, `+0x194/+0x198/+0x19c` target x/position/speed, `+0x1a0/+0x1a4` current
speed, `+0x1bc` base speed, `+0x1dc/+0x1e0/+0x1e4/+0x1e8` lookahead
(`0x0040a100`). `rider_half+0x1e0` is the stats-table pointer (`0x00463c78` rows).
The state dispatch table `0x00465980` maps 0→`0x00408e30`, 1→`0x00409000`
(racing), 2→`0x004092d0`, 3→`0x00409460`, 4→`0x00409ca0`, 5→`0x0040a4f0`,
6→`0x0040a560` (read from the reference EXE `.data` at file offset `0x63f80`).
Solo-capture evidence: the demo rider runs state 1 with `target_x` 18688 until
tick 182, then `0x00408ad0` moves it to state 5 and `0x0040a4f0`→`0x00408e40`
writes `target_x` 43264; bike `+0x288` ramps +165/tick from tick 183 and
`+0x10c` flips −34→+86 at tick 184. Native `ai_think` has no state machine and
holds `+0x288` 0, diverging on `lateral` at tick 184 (17073 vs 16953).
The transition needs the frozen opponent nodes: solo captures still construct
7 opponents (think/move nulled), and replan `0x00408cd0` picks the target
(`+0x168`) among them. Native solo builds zero opponents, so it can never take
this transition; the replay harness must build frozen opponents (open).
Steer rates verified: bike `+0x2a8/+0x2ac` read 165/141 live, matching native
`apply_spec` output for SPEC id 2 exactly — no divergence there.
Target identification (same session, `sim-kink-probe6`): the `+0x168` value
`0x1402f60` is scheduler-ring node 28, think 0, position 13312 — a frozen
opponent node the demo rider is pulling away from. The player bike is ring
node 32. So `0x0040a4a0`'s ahead-window (`0xf0`) trips on a receding frozen
node, and `0x00408e40` then steers off it. Solo replay must mirror the frozen
opponent construction exactly.
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

## Audio/video and pixel capture follow-up (October 2, 2026)

Original high-resolution capture: draw call `0x004439f5` invokes `0x00450420`. Locked state `0x00475620`, base `0x004b7ec0`, pitch `0x004b7ec4`, logical dimensions `0x0047539c`/`0x004753a0`, palette `0x0049e138`. Captured active shift `0x00481fa8` is 15, converting Q16 320×240 projection coordinates to a 640×480 surface. Low mode remains unmeasured. Capture is before dashboard drawing.

`0x0040ff40` builds transformed linked nodes (next at +0x68); `0x0040ffc0` converts coordinates; `0x0044d300` locks and traverses the display list; polygon fill/span routines are `0x0044d620`/`0x0044db30`. Earlier plan filler addresses were incorrect and have been corrected.

`0x004349c0` computes engine frequency from speed times the active gear ratio (+0x1cc+gear*0x10), divided by 256, plus 0x2000. Its smoothing limits changes to ±0x2000 on the ground, increases by 0x1000 airborne (cap 0xffff), and drops 0x1000 when crashed with prior frequency >0x2000. `0x00434ce0` multiplies this by Doppler (+0xc8), divided by 512, and chooses normal/nitro/tunnel buffers. This rule is traced, not yet connected or oracle-validated.

`0x004141c0` converts effect volume to DirectSound attenuation `clamp(volume*10000/16384-10000,-10000,0)`, clips pan to ±10000 (optionally reversed), frequency to 100..100000, and stops looping playback below -8999 attenuation. Audible gain must use the DirectSound logarithmic scale; the generic linear mixer alone does not reproduce this wrapper.

AVI corpus: 59 Cinepak files, 44 stereo WAV IMA audio streams and 15 silent bike showcases. Sequential RIFF LIST/movi scanning supplies complete packets without relying on idx1. IMA predictors/indexes restart per block; stereo payload alternates four bytes per channel, low nibbles first. Standard shift/add IMA rounding matches every original audio sample. Cinepak state retains separate 256-entry V1/V4 codebooks per strip and the previous RGB frame, with shared/independent strip books, partial update masks and skip/vector flags.

Codec comparison algorithm reference: [FFmpeg Cinepak decoder](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/cinepak.c) and [FFmpeg ADPCM decoder](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/adpcm.c). Native implementation is independently structured C++; FFmpeg CLI is used only for byte comparisons. No FFmpeg runtime dependency or original assets are embedded.

Original MCI destination setup `0x00402820`: native-size x=160 and y=240-height/2 when flag 1 is absent and screen mode is not 2; doubled width/height at x=0, y=240-height otherwise; explicit flag 1 changes y to 20. Native CLI playback currently uses the normal rectangle. Frontend call-site flags still require mapping.

## Packaging evidence (October 2, 2026)

No additional original-game behavior is inferred by packaging. The standalone macOS build uses the [official SDL 3.4.8 release](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.8), verifies the published source archive digest, links it statically, and is ARM64/macOS 13 with Apple system dependencies only. Android still uses the existing verified SDL AAR. Both packages contain licence notices and exclude original disc assets. Hardware and full game fidelity remain separate acceptance items.

## Flat polygon raster recovered (October 2, 2026)

`0x0044d620` receives four integer surface vertices and one palette index, with bbox globals prepared by `0x0044d300`. It calls `0x0044ce60` to normalize vertex topology/orientation, then builds up to four directed edges. Horizontal edges are inactive. Each lower-to-upper edge starts at `x<<16`; slope is `(dx%dy<<16)/dy + (dx/dy)*65536`, using signed divisions and 32-bit wrap. An upper endpoint below the next vertex in that edge's direction has its final row decremented. Edge slots are acquired in original edge order at each start row, stepped with wrapped addition and released after their inclusive final row.

Each row floors signed Q16 edge positions, clips the lesser x to the left bound and greater x to the right bound, and `0x0044db30` writes exactly right-left bytes (exclusive right). The bbox bottom is inclusive and capped by the clip bottom. Fully horizontal polygons draw one span. The original's degenerate adjacency tables `0x0047564c..0x00475687` correspond to skipping duplicate neighboring vertices; the native filler computes that relation without embedding table bytes.

The native `fill_quad` accepts normalized polygons, bounds coordinates to the supported original Q16 edge range and rejects unnormalized shapes requiring more than two active edges. This is a partial primitive port: arbitrary topology normalization/texture handling are not implemented. All 64 captured normalized fixture images match exactly; this does not establish a full-frame world renderer match.

Oracle loader mode 2 calls the original pure filler on synthetic points into a separately allocated 640×480 buffer, restoring pixel pointer, pitch, clip and bbox globals afterward. Original EXE bytes on disk remain unchanged. Original function normalization is captured in the recorded points. Capture outputs contain synthetic geometry rather than disc graphics. Live mode 1 additionally permits component capture after the first locked draw.

## View coordinate conversion (October 2, 2026)

`0x00410160` sets shift/mask globals `0x00481fa8` / `0x00481ff4`: low uses 16 / `0xffffffff`, high uses 15 / `0xfffffffe`. `0x0040ffc0` applies arithmetic shift then mask to eight Q16 node coordinates. High mode therefore computes twice the low-mode floor, snapping all vertices to even pixels. Negative values require signed floor, not division truncation. The earlier description of shift 15 alone was incomplete.

`0x00448870` sets logical view dimensions to 640×480 or 320×240. Low inset mode sets display size 640×480 and view origin (160,120); high mode uses origin zero. `0x004457f0` maps settings 1, 2, 3 to low fullscreen, low inset, high fullscreen. `view_mode` implements these fields; the existing game scene does not yet consume this abstraction. Twenty-four original conversion-helper fixtures match including both signed extremes.

## Terrain iterator recovery (October 2, 2026)

Section row +0x10 points to terrain. `DLBR`: 16-byte header, +8 sample count, then two selector bytes per sample. `NTMR` (and code-supported `DWDR`): 24-byte header, +8 sample count, +12/+16 left/right record counts, then 16-byte wall records for each side and selector pairs. Records contain unsigned sample start +0, eight parameter bytes +4..+11 and signed base height +12.

`0x004510c0` initializes wall width at byte +6, height jitter at byte +11×2, lateral jitter at byte +8×2. `0x00451220` changes each once when integer sample changes, using original random wrapper values modulo (2×variation+1) minus variation. Variations are +5/+10/+7, bounds are [0,2×byte6], [0,4×byte11], [0,4×byte8]. `0x00451710` exports width, lateral jitter+(byte6+2×byte9)×2, base height+height jitter. Final road-relative wall transforms are applied later by `0x00454240`, not by the iterator.

`LIHR`: 20-byte header, +8 record count, 76-byte records {signed start,end, two selector bytes, two reserved bytes, eight 8-byte point definitions}. Each definition is {unsigned min/max x, x variation, signed min/max height, height variation, min/max interval length}. `0x00450fc0` draws initial x in 32-unit bounds and height in 16-unit bounds. `0x00451380` chooses interval lengths and x/height slopes bounded by the profile; `0x004515a0` preserves forward/backward transition and last-sample behavior. The fourth point per side is stateful but only three per side export. `0x004517e0` exports absolute edge-relative x/height and six 0..31 gradient indices using signed division.

The real disc contains unsorted mountain starts and negative/inverted CITY hill intervals; these are accepted in physical order, with the original single record transition per call. Structural bounds remain strict. The native API takes an external random source so it can later share the game's original stream. Fixed-random component comparisons cover 864 original slices with exact equality. This does not yet validate live course-cache initialization or the shared random stream in natural races.

## Hill projection recovery (October 2, 2026)

`0x0041d6d0` receives a projected road record with source sub-slice pointer +0x0c, world centre x +0x10, road elevation +0x14, scale +0x20 and surface row +0x4c. It projects six source points +0xf0..+0x11c into Q16 output +0x70..+0x9c. X uses `(world_x-camera_x+point_x)*scale + (view_width & ~1)*32768`; y uses `(camera_height-road_y-point_height)*scale + 90*65536`. Every arithmetic operation wraps as signed 32-bit. EAX returns the maximum y including the initial surface row (confirmed by disassembly, despite the decompiler's void signature).

At scale ≥65,536, a negative x with a nonnegative wrapped product yields −0x40000000. A nonnegative x whose product-plus-centre is negative yields +0x40000000. Other x values use the wrapped projection. At lower scales the sentinel checks are omitted. Native `project_hills` reproduces this behavior; 96 isolated original calls compare exactly. This verifies the helper, not live world visibility/rendering.

## Polygon topology/orientation recovery (October 2, 2026)

`0x0044cd70` computes four signed 32-bit wrapped corner cross products, marks positive and zero corners, then propagates orientation through collinear corners three times. Entirely collinear returns 255. `0x0044ce60` repairs one concave corner with a wrapped/truncated neighbor midpoint, swaps crossed edges, or rejects orientation masks 5/10. It then orders by the lowest two y values (default), x values (flag 8), or texture orientation (flag 4). Ties follow the original strict/non-strict comparisons. The default lookup tables encode a shared cyclic edge of the lowest two vertices; opposing vertices have no preferred start. Flag 4 toggles a provided texture-flip state when reversing.

Native `normalize_quad` matches 2,048 original calls across modes 0/8/4, including collinear, duplicate and wrapped-cross fixtures. `draw_flat_quad` uses the default normalization and preserves the original horizontal bypass, then calls `fill_quad`. All 64 indexed reference images also match from their captured pre-normalization points. The existing filler still rejects pathological shapes with more than two active edges; behavior of original rejected polygons and texture drawing is not established by this milestone.

## Indexed texture raster recovery (October 2, 2026)

`0x00418910..0x00419e10` are opaque spans for strides 1,2,4,..256; `0x0041a0b0..0x0041bc20` preserve destination where the sampled palette index is zero. A generic native loop replaces alignment/unrolled variants with equal bytes. Sampling uses unsigned wrapped `(((row_q16 & 0xffff0000) << stride_shift) + column_q16) >> 16`; row/column increments wrap per pixel. Bounds checks prevent malformed UVs reading outside imported texture storage.

`0x004184a0` writes inclusive edge rows into one of two 480-entry {Q16 x,U,V} tables. Its reciprocal is `2^32 / ((y1-y0)<<16)` with signed division; signed 64-bit slope products retain bits 16..47. Negative rows advance values without writing; rows cap at 479. Horizontal segments do nothing. `0x004180b0` walks the two chains from the first minimum-y corner, skips horizontal top edges, and later segments replace shared rows.

`0x00418760` floors x, tests the initial right edge >0, swaps edge triples if needed, uses reciprocal floor(65536/span_width), advances clipped negative-x UVs and clips span right. The static reciprocal table at `0x00467ebc` agrees with this division. Disassembly shows ECX=count, EDX=V, stack=U,dU,dV: texture spans use V as row and U as column, despite the ambiguous decompiled call expression. `draw_textured_quad` ports normalized convex scan conversion with origin-zero left clip, caller-supplied UVs, mip pixels and transparency.

Mip selection compares inclusive projected bbox dimensions with inclusive source rectangle dimensions, reduces each axis while destination < source/2 and another level exists, then selects the smaller axis level. Original UV shorts shift right by that level and resource mip entries are twelve bytes apart from +0x38. `texture_mip_level` and UV shift match the original in three-level synthetic quad captures. Resource header/atlas/display-list preparation is still pending.

All 1,008 span fixtures and 64 full textured quad images compare exactly. These are synthetic original-function fixtures; they do not establish a full race or sprite render match.

## CEL wrapper and PC indexed PDAT (October 2, 2026)

Original `0x0040b800` walks size-delimited chunks until pixel data: optional `OFST`, `CCB `, `PLUT`, `XTRA`, optional `HSPT`, then `PDAT` (code also supports `RPDT`/`RRPD`). On-disk tags are reversed little-endian bytes. CCB is 80 bytes with its body at chunk+12: flags there, width/height encodings at chunk+72/+76. `0x0044fb60` interprets sign-bit dimensions as powers of two and other dimensions by low nine bits, then replaces them with its lookup encoding. The native reader preserves numeric dimensions/flags rather than 32-bit runtime pointers. PLUT has count +8 and little-endian 16-bit entries from +12; its use for nonindexed legacy drawing is still pending.

PC pixel bodies start at PDAT+8 with magic `0x00bc614e`. Header: flags +12, name +16 (16 bytes), width +32, height +36, mip count +40, flat palette index +44. Per-level pointer word at +56+12×level is relative to pixel-body+28. `0x0040b7b0` relocates these words in place. Native bounds-checks them and copies only indexed mip storage, retaining caller ownership. Width is a power-of-two stride ≤256; reduced height bottoms at one storage row. Mip data contains direct PC palette indices, requiring no PLUT-to-RGB conversion.

Seventeen GLOBAL textures contain 117 indexed mip blocks. Twenty-five other GLOBAL/course CEL resources contain only four legacy pixel bytes; the reader retains those words and does not pretend they are decoded images. Course selectors additionally reference external/local CEL tables (`0x0044ac00` / `0x0044aca0`), whose preparation is not yet ported.

The oracle calls original `0x0044fb10` to initialize a saved dimension table, runs `0x0040b800` on copied disc resource payloads, captures numeric fields and writes each original-relocated mip byte range, then restores the table. All 42 dimensions/flags and all 117 byte ranges match native decoding. This establishes resource interpretation, not original scene selection or full sprite drawing.

### Ordered node renderer recovered (October 2, 2026)

`0x0040ff40` transforms the source linked list in place into 64-byte nodes at source +0x68, stopping on the +0x0c sentinel 0x40000000. `0x0040ffc0` maps eight Q16 coordinates at +0x44..+0x60. `0x00410080` consumes the source +8 pixel-resource pointer directly, accepts magic 0xbc614e via `0x0040feb0`, and copies signed bounds at resource +4/+6/+8/+10 through `0x0040fe50`. The source is not an extra CCB pointer layer. Rejected resources with source +0x66 set become flat-template nodes colored from +0x67.

`0x0044d300` walks in list order; quality-mask intersection with resource flags selects flat fill (resource +44 color, or node +60 for the flat template). Texture paths normalize points, update horizontal flip, inset each UV endpoint by one, and call `0x004180b0`. Mip choice uses the original pre-normalization integer bounding box and inclusive signed UV extents. Quality masks observed in settings are 16, 48 and 112. Rider flag 0x2000 specializes the resource with `0x00443820`; that path remains unported.

The native generic renderer matches 32 synthetic original lists (384 nodes, both 320×240 and 640×480 modes), comparing all 6,144,000 pixels exactly. CEL signed UV bounds also match the original relocated loader on all 17 real indexed textures. This verifies generic dispatch/composition, not natural scene construction or whole race frames.

### Lane-panel construction and road material table (October 2, 2026)

`0x0043c170` builds one quad per lane using projected lane spacing (+0x24) of the neighboring/current slice, starts (+0x30/+0x40), and rows (+0x28/+0x38) masked with 0xffffc000. It expands the first and last panels by 0x40000 in logical Q16. Marking flag 0x10 reverses the two ends. Counts come from source bytes +0x34/+0x35; the source Q8 lane values at +0x30/+0x2c determine boundary/interior material roles via `0x0043c0b0`. Surface flag 0x20 selects the plain template. Marking flags 8/16 select marked variants, 0x20 selects the alternate material, and 0x40 suppresses special selection.

`0x00450720` emits quads in first-left, first-right, second-right, second-left order, with U/V reversals when end ordering is inverted. `0x0043c170` uses PRE0 0x18 for consecutive crossover terrain (type 3), otherwise 0x1f001f00. PRE0 is captured for structural verification; indexed texture drawing uses resource mip data.

`0x00441770` reads 20-byte load descriptors at `0x0046dfb0`: destination, resource tag, zero-based ID, loader state fields. CEL IDs are incremented before lookup. Destination slots 0x4c3cc0/0x4c3cb0 and the 0x4c3cd0/0x4c3ce0 arrays identify plain, alternate, marked and normal panels. The native loader reads these IDs from the original executable, resolving imported CELs without embedding the table. `0x00444e30` aliases marked slot 1 to marked slot 0.

All 343 panels emitted by 128 original calls match coordinates/material/PRE0/flips. Substituting actual original-loaded road CEL pixel resources into those original lists and drawing through `0x0040ff40` yields 24,576,000 pixels identical to native construction + imported material binding + generic renderer. Natural frame visibility/cache/order is not covered.

## Language string tables (October 2, 2026, session 20)

- `0x0042f340` picks `Text\<LOCALE_SABBREVLANGNAME>.DLL` (via `0x0042f410`/`0x00443380`). If that fails it tries the primary language's default sublanguage (`0x400 | primary`), then `0x409` (ENU). Only ENU/DEU/FRA/ESP/ITA exist on the disc. The native rule maps the first SDL preferred locale's ISO 639 code (`de`, `fr`, `es`, `it`) and otherwise uses English.
- `0x0042f5d0` builds the global string table at `0x00475378`. String 0 is `"%d %d"` (buffer bytes, count). Strings `count..1` are then loaded in descending order with `LoadStringA` into one shared buffer. When a loaded string's last ANSI byte is `0xff`, the byte before it is a signed index: string `3000 + index` overwrites those two bytes, and this repeats. Slot 0 is null. `LoadStringA` truncates to the remaining buffer.
- All five DLLs yield 985 non-empty slots (1,360–1,379 raw `RT_STRING` entries, including the 3000+ continuation pool). Strings are Windows-1252, as the game saw them.
- `TEXT/BADLOC.DLL`, `FUTB.DLL`, `FUTD.DLL` and `FUTR.DLL` are TrueType fonts, not PEs: Badloc and Futura Bold/Demi/Regular. `0x0042f720` installs them with `AddFontResourceA`. `0x00442990` creates Badloc at heights 0x12/0x48 (low resolution) or 0x20/0x80 (high resolution), plus Wingdings, and Arial/Small Fonts/New Courier for debug overlays. `0x00448790` draws captions in bold Arial Narrow with a 2-pixel black drop shadow. Arial Narrow, Arial and Wingdings are Windows system fonts, not game data.

## Career rules and save files (October 2, 2026, session 20)

**Game state block `0x004b8a10`** (32 bytes, initialised from `0x004754b0`, saved verbatim):

| Offset | Meaning | Evidence |
| --- | --- | --- |
| +0 | network player slot; cleared at race start outside mano-a-mano | `0x004160c0` |
| +1 | character 1..8 (Axle, Bose, Cydney, Jon, Pearl, Rhonda, Jim, Mike), 0 = none | `0x0041f5a0`, `0x0041c060` |
| +2 | level 0..4 | `0x0041f6d0`, `0x004453f0` |
| +3 | qualified-course bits (bit n−1 for course n; `0x1f` = level complete) | `0x00449150` |
| +4 | bike id 0..14 | `0x0041f630`, `0x004215e0` |
| +5 | course 1..5 of the race started (1 City, 2 Sierra Nevada, 3 Peninsula, 4 Napa, 5 Pacific Highway) | `0x004160c0`, menu items 16..20 |
| +6 | 0-based finishing place (`0x004c5d78`) | `0x00416120`, `0x00437860` |
| +7 | unknown; initial value 1 | `0x004754b0` |
| +8 | cash (u32) | `0x00421290`, `0x004214e0` |
| +0xc | alias (NUL-terminated) | `0x0041c060` |

**Game mode** `0x004753b0`: 0 attract, 1 Thrash, 2 Big Game, 3 mano-a-mano. `0x00445090` resets level, qualifications and character when the mode changes.

**Characters:** `0x00415fc0` maps character 1..8 to personality 8, 7, 2, 5, 4, 6, 3, 1. Starting cash comes from u16 table `0x00469000` by character. The default bike is `0x004641c8[level + personality*5]` (`0x0041f630`). Choosing a character in Big Game restarts the career (level 0, no qualifications, starting cash).

**Race outcome** `0x004c5d7c` (`0x00416010`): code `0x7f` (finished) becomes 2 (won) when the 0-based place is < 3 in single player, else 3 (lost). 4 is busted, 5 wrecked. `0x00416120` maps 2/3/4/other to results screens `0x1e`/`0x1f`/`0x1d`/`0x1c`, or to the main menu in attract mode.

**Results** (`0x00449150`, `0x00421290`, `0x004214e0`, `0x0041f6d0`):
- A win ORs the course bit into the qualifications (except in mano-a-mano).
- Big Game pays every finisher (won or lost) `prize[place] × (level+1)`. `prize` is the static first dword of the 0x4c-byte standings rows at `0x0046ab48`: 1000, 750, 500, 400, 300, 250, 200, 160, 130, 100, 70, 50, 30, 20, 10.
- Big Game busted: the fine is `(level*5+5)*0x50` = 400 × (level+1). Wrecked: repairs cost `bike_price/10` (u32 table `0x00468fc0`). If cash ≥ cost it is paid (messages 197/194). Otherwise message 198 ("outta money") is shown and `0x0041f630` restarts the career.
- In Thrash, wrecked/busted clears the qualifications (messages 193/196). There is no money.
- After the results, all five qualified sets the save-pending flag and clears the qualifications. Then either the level increments (screen `0x20`) or, at level 4, the finale (`0x23`). Otherwise the next screen is Thrash course select (`0x0a`), The Street (`0x0f`) or character select (`0x0e`).
- Videos: `Wreck`/`Busted`/`Win`/`Level` + random 1..6, `Lose` + 1..10, `FinalWin`.

**Bike shop** `0x004215e0`: showroom slots `0x80..0x8e` map to bike ids via `0x0041ff00`. Owning the bike already gives "WAKE UP!" (187/188). If `price[current]/2 + cash < price[new]` (unsigned), the result is "IN YOUR DREAMS!" (185/186). Otherwise cash += `price[current]/2 − price[new]` and the bike changes.

**Save file** (`0x0042f000` writer, `0x0042f110` reader), 0xa0 bytes:
- +0: checksum `0x0042efc0` over the following 0x9c bytes. Over bytes b with countdown n, `sum += b`, `weighted += n*b`, `alt += (n odd ? b : −b)`; the checksum is `(alt<<16 | sum&0xffff) ^ weighted`.
- +4: magic `0xcafedead`.
- +8: the 32-byte game state.
- +0x28: 10 × 3 dwords. Each is the triple at `+personality*12` in each runtime personality copy (`0x004c6b40`, 0x204 bytes each; defaults are the first 0x78 bytes of `0x00464558` records). These are the relationship values Schmooze reads (`0x00420bb0`).
- The reader checks magic (error string slot 369), then checksum (slot 370). It then restores the personalities, copies the state and relations, and forces Big Game mode.
- The native reader additionally rejects out-of-range state bytes; the original performs no range checks.

**String slots used:** 192/195/199/200 are the results captions; 201+place is "%s finished in …"; 197/194 are the fine/repair formats; 193/196 the Thrash penalty texts; 198 game over.

## Front end (October 2, 2026, session 20)

The menus are a Win32 window with child "RashButton" windows over two 640×480 15/16-bit DirectDraw surfaces. One surface holds the picture; the other is the same picture with every 5-bit channel halved (`0x0041e6e0`) and sits behind text boxes and buttons. Menus therefore do not use the 8-bit race surface; the native front end draws into a 32-bit image quantised to RGB555.

**Screen table** `0x0046a390`, 0x24 entries × 0x14 bytes: item count, item array, title string slot, title DrawText format, last focus index. At start-up `0x00423881` sets the counts of screens `0x10`/`0x11`/`0x12`/`0x13`/`0x0b`/`0x0d` from `0x00474668`/`0x0047466c`/`0x00465fb0`/`0x00474670`/`0x0046ca88`/`0x0046ce80` (13, 9, 22, 8, 6, 18). A screen with no items is a race-start or quit code: `0x00421df0(n)` posts `WM_QUIT(n)`, and the main loop (`0x00448200`) starts course n (1..5) or quits (6).

**Items** (0x38 bytes):

| Offset | Field |
| --- | --- |
| +4 | target screen |
| +8 | caption slot |
| +0xc..+0x18 | x, y, w, h |
| +0x1c | navigation bytes: left, right, up, down, tab, shift-tab (`0x004227d0` follows a chain until an enabled item without flag `0x40000`) |
| +0x24 | flags (see below) |
| +0x28 | enabled |
| +0x2c | handler; activation `0x00423200` calls it, else enters +4 |
| +0x30 | description slot |
| +0x34 | box format |

Flags: 1 title box shows the caption; 2 title box shows the description; 4 description box; 0x10 popup item; 0x20 the button keeps the picture behind it (otherwise darkened); 0x40 light colour when disabled; 0x2000 price box; 0x4000 course length; 0x8000 Escape target; 0x40000 skipped; 0x80000 dimmed under a popup.

**Screens:**
- 7 Credits (1 s timer)
- 8 "GAME LOADING / Get ready to race!"; its item's target is the course
- 9 main menu
- 0x0a Thrash courses
- 0x0b/0x0c/0x0d mano-a-mano
- 0x0e characters
- 0x0f The Street
- 0x10 Restroom
- 0x11 Jukebox
- 0x12 Configure Input
- 0x13 Graphics Options
- 0x14 Olley's
- 0x15/0x16/0x17 Rat/Sport/Super bikes
- 0x18 showroom
- 0x19 Der Panzer Klub
- 0x1a Schmooze
- 0x1b Big Game bulletin board
- 0x1c..0x1f wrecked/busted/won/lost
- 0x20 level progression
- 0x21 fine/repair/penalty
- 0x22 race results
- 0x23 finale (2 s)

The course screens share one item array; the sixth item is copied from `0x00469130` (Thrash: Restroom), `0x00469168` (mano: Disconnect) or `0x004690f8` (Big Game: Der Panzer Klub).

**Pictures:**
- `0x0041d970` picks the screen picture: Credits, Main, Postcard, Street, Olleys, Panzer, `Bikes/<class>/<name>`, `Restroom/Rest1..6`, `Results/Wreck|Bust|Win|Lose 1..6`, `Results/Level<level>`, `Results/Finale`, and `Chars/<name>-F` behind the loading screen.
- `0x0041dbc0` swaps the picture while an item has focus: `Postcard/[Q]City|Sierra[R]|Penin|Napa|PCH` (Q = qualified, R = alternate artwork toggled with A), `Chars/<name>-N`, `Showroom/<class>/<name>`, Street/Klub/Olley/Home.

**Fonts** (`0x00423881`): title boxes Badloc 72; buttons Futura Condensed Bold 24; full-width buttons and descriptions Futura Condensed Regular 32 (Demi 24 when flag 0x80; Regular 36 in Schmooze); "QUALIFIED" Badloc 42 at (452,272); standings New Courier Bold 18.

**Colours** (`0x0041fc90`): normal `#DEFFF0`, focused `#E1E180` with a Wingdings arrow, disabled/dimmed `#A0A0A0`, skipped `#FF6432`, titles `#FF6432`. All text has a 2-pixel black shadow.

**Box geometry:**
- Title box (`0x0041f770`): left (text + 0x31 by text + 0x2a), right-aligned (min 0x13c), centred at 3/4 height, or 640×300.
- Description box (`0x00420470`): ≤300 wide at x = 8 or 316, y = 122; or full width at y = 380.
- Restroom/jukebox items without a width take the title box width, clamped to 0xe6/0xa0..0x118 (`0x004459f0`).

**Popups** `0x004224b0`: header plus choices for mode `0x004735c0`, level `0x004736a0`, name `0x004737f0`, resolution `0x00474240`, biker `0x00474320` and videos `0x004743c8`. Choosing calls the stored setter (`0x0049e0a8`) with the item target: `0x00445090` mode, `0x004453f0` level, `0x004457f0` resolution, `0x00445900` biker detail, `0x00445990` videos.

**Restroom enabling** `0x00444fb0`: Y/N strings `0x00473598..0x004735b4` by mode, character and save file.

**Course length** (flag 0x4000): u32 tenths of miles at `0x00468f58[course*5 + level]`; metric ×1.609 truncated (`0x00462008`), units string 27/28.

**Character text:** `"Height/Weight/Bike/Cash"` uses language strings 4000..4003 (feet/inches or metres; lbs or kg ×0.454) and rider heights/weights at `0x00463c78` +8/+0xc. The bike is the level default.

**Standings rows:** `"%2d)  $%4d  %*s  %s"` (Big Game) or `"%2d)  %*s  %s"`, time `"%d:%02d<decimal>%d"` in tenths, `"------------"` for none. Three rows are shown, plus the player's row when placed below third (`0x00421290`).

**Schmooze** `0x00420bb0`:
- Pick a random rider 0..7 not yet seen this visit; the player's own bit is set on entry.
- Compute `value = rel[r][character][1] − rel[r][character][0] + rel[r][character][2]`.
- Attitude is `0x0046ab44[value < 16000 − 1500L ? 1 : value ≥ 32000 − 3000L ? 3 : 2]`.
- Two to four candidate lines come from `0x0046a6e0[level*8 + r]` (shorts; extra lines when +24/+26 name the player's character). The picture is `Chars/<name>-<"FNH"[attitude]>`.
- A visit shows at most five riders (`0x00420b50`).

**Not ported or replaced natively:**
- Mano-a-mano is disabled (plan 09).
- Jukebox, Sound Mix and Configure Input items do nothing yet.
- The 60-second attract-demo timer on the main menu is not implemented.
- Windows common file dialogs are replaced by numbered `<Character> N.RRS` saves in the preference directory and a list of the newest eight.
- MessageBox prompts are replaced by an in-picture dialog.
- Wingdings and New Courier are substituted.
- Title text placement within its box is approximate.

**Credits** (`0x00424140`, `0x00424230`, `0x00424340`):
- Screens 7 and `0x23` scroll string tables 1000 (opening, from y 0x30, left aligned with DRAWTEXTPARAMS tab length 35 and left margin 40) and 2000 (finale, from y 0x1c8, centred). Both are loaded with the `0x0042f5d0` rules over the normal picture.
- Twenty 24-pixel lines are drawn in Futura Condensed Bold 24 with a shadow. The first character of each line picks its colour: G grey, O orange, R red, Y yellow, others pale.
- The first timer tick re-arms for 1.5 s, then the roll scrolls `0x00468f50` (1) pixels every 15 ms. '+'/'-' double or halve the speed. When the table ends, the screen's item (handler `0x004241e0`) leads to `0x0041f6d0`. Escape skips.

**Race end** (`0x00416010`): finishing (`0x00434660`, code `0x7f`), wrecking (`0x0040a600`/`0x0043a190`, code 5) and being busted (`0x00407f30`, code 4) set the outcome once. The race then keeps running for 0xb4 ticks (`0x00442e10`). Confirming the in-race Escape prompt (string 316) while still racing sets outcome 5, so quitting counts as wrecked (`0x00423408`).

## Sound effects, engines and soundtrack (October 2, 2026, session 20)

**Effect slots** (`0x00435e60`): 0x45 slots, one DirectSound buffer each, loaded from `Audio/Effects/<name>.WAV`.
- Slot 0 is the player's engine, chosen by bike id: ids 0–4 EngRat, 5–9 EngSport, 10–14 EngSuper.
- Slots 1–6 are EngNitro, EngTunne, EngOther, Engcar11, EngOther, Engcar11.
- Slots 7/8 are M_YEAH/F_YEAH; 9/10 CHAINIMP/CLUBIMPA; 11 BOUNCE; 12–14 CRASHHAR/MED/LIT.
- Slots 15–20 are GRUNTM1–3 and GRUNTF1–3; 21 KICK_MW4; 22 PUNCHHEA; 23/24 SWISHCHA/SWISHGEN; 25/26 SKIDONNR/SKIDOFFR; 27 FOOTSTEP; 28 RIDERSLI; 29 BMWLONG; 30/31 SIREN/SIRENFST.
- Slots 32–68 are rider and pedestrian voices.
- A retrigger restarts only that slot's buffer, so there is no voice stealing.

**Event queue** (`0x00436830` → `0x004367f0`): 10 records of {code, lateral, height, position, speed}. Codes and their call sites:
- 0/1/2 crash light/medium/hard, and 3/4 grunts, from contact `0x004049c0` (player only). Below 0x1771 impulse it is a grunt (rand&1)+3 plus crash rand&1; above, grunt +4 and crash +1, and nothing when below 0x7d1 with a small +0x188.
- 5 wall grunt (`0x0043a610`).
- 6/7 chain/club swish (`0x004096f0`).
- 8/9/10/11 impacts chain/club/kick/punch; kick alternates 10/11 on the tick parity (`0x00409ad0`).
- 12 footstep, 13/17 skid on/off road (`0x00434bd0`, +0x164 > 0x32 and surface class ≤ byte `0x0046dce5`), 16 slide (`0x00434a80`).
- 14/21 nitro on/off (`0x0040b110`/`0x0040b170`), 18 car horn (`0x0044efa0`), 19 siren (`0x00435ae0`), 20 landing bounce (`0x0043a190`), 22 speech.

**Event consumer** (`0x004351f0`): most effects play once at volume `(rand%0x1001+0x3fff)/100 × effects` and frequency `rand%0xbb9 + 0x214d` Hz. Footsteps, skids, slides and speech use their own constants. The finish yell (M_YEAH or F_YEAH) plays once when the player's bike gets flag 0x10. Grunts use the female set (+3) for characters 3, 5 and 6 (`0x00435da0`).

**DirectSound mapping** (`0x004141c0`/`0x004142c0`):
- Volume in hundredths of dB is `(v×10000 >> 14) − 10000`, clamped to −10000..0; a looping buffer stops below −9000.
- Pan is ±10000 hundredths of dB, negated when speakers are swapped.
- Frequency is clamped to 100..100000 Hz.

**Engines** (`0x00434ce0`, `0x00434db0`, `0x004349c0`):
- The listener is the camera (`0x00437c30`: position Q8, lateral, position change × 256).
- Distance is `|dp| − min(|dp|,|dl|)/2 + |dl|` within 0x1e00; volume is `((0x1e00−d)×8/256)²/256`.
- Doppler is `0x13b0000/(0x13b00 − relative speed)`; pan is the `0x0044f5a0` octant angle × 0x62.
- RPM is `gear_ratio[+0x23c]×speed/256 + 0x2000`, moving at most ±0x2000 per update, +0x1000 while airborne, −0x1000 after a crash.
- Engine frequency is `doppler × rpm / 512` Hz; volume is `(+0xcc × 0x9ffe / 0xef10) × level`.
- The two vehicles nearest the camera (`0x004350e0`) use slots 3/5 (bikes) or 4/6 (cars).

**Levels** (`0x004362e0`): each channel uses `curve[level × master / 100]` from a 101-entry table built in code. The native reader decodes that code's stores from the user's executable. The Mixer block (`0x00463030`) holds master 100 and interface/player engine/effects/music/other engines/siren at 80, plus mute and swap.

**Music:**
- The soundtrack thread `0x00403040` streams `Audio/Music/<track>.RRA` from the saved track index and advances 0..13 when a track ends (HB_Trip, HB_Pass, MM_Dino, P_Pansy, P_Jessie, P_Bridge, SG_Rusty, SG_Outsh, SG_Kicks, SG_Super, SD_Train, SD_Duel, T_Teeth, T_Auto).
- Race start (`0x004160c0` → `0x00402f50`) stops it. Races play MIDI `Grunge1..5` by course (`SbkGrng` with AWE32 SBKs; Klown when the alias is "krusty" or "bozo"), via `0x00433bb0`.
- Showroom clips (`0x004023e0` flag 1) only signal the soundtrack thread; other videos stop it.

## Resource packing, cel groups and dynamic rider textures (October 2, 2026, session 20)

**Packing** (`0x004157b0`): a resource payload read through the resource stream starts with 16 bytes {unpacked size, stored size, CRC, packed flag}.
- Packed data uses the PKWARE Data Compression Library "implode" format, exploded by `0x004555e0` (`0x0040e4d0` / `0x0040e530`, "Error in compressed data").
- `0x0040e440` keeps a reflected CRC-32 (0xedb88320) of the output, which must equal header word 2.
- All 163 non-empty `FAM ` records in FAMILIES.RSC are packed this way. The native `unpack_resource` explodes them and checks size and CRC; all pass.

**Family containers** (`0x0044aa80`): a dword count followed by that many offsets, relative to the container, to nested chunks.
- Leaf chunks are `CCB `, `PLUT`, `RRAN` (animation), `CANS`, `OFSS`/`OFST` (offset tables), `FORM`, and `CLGP` (cel groups).
- `CLGP` (`0x0044a840`):
  - +8 (relative to +0xc) is the shared CCB body; +0xc (relative to +0xc) is the PLUT.
  - Byte +0x14 is the mode (bits 0–1) plus a power-of-two flag (0x40). Byte +0x15 is the dword index of the block table; bytes +0x18/+0x19 are counts; +0x1a/+0x1b are shifts.
  - Mode 0 (`0x0044a660`): table entry j points (relative to the chunk) at a list of +0x18+1 pixel-block offsets, each relative to +8.
  - Mode 1 (`0x0044a760`): the table holds the pixel-block offsets directly, with sizes in byte pairs at index +0x16.
- Every pixel block is an indexed texture with magic `0xbc614e` and mip offsets relative to +28 (`0x0040b7b0`), the same body as a CEL `PDAT`.

**Dynamic rider textures** (`0x00410080`, `0x00443820`):
- A display node whose resource is the "Bob" marker (`0x00466684`, stored at `0x00466688`) carries a frame at source +0x34 and a colour at +0x38.
- Colours 0..14 select bike frames remapped through `0x0046e158[colour]`. 0xde selects cop frames, 0xdf cop shadows, 0xe0 bike shadows (sheets loaded by `0x00443610` from `Data/Bikers/<Hi|Lo>{Bob,Cop,Shadow,ShadoC}.DAT`).
- Out-of-range pairs are dropped. At draw time the frame is copied into a 256-stride page with bounds (0,0,w,h).

**Display list** (`0x00450370`/`0x00450420`): nodes are 0xa8 bytes, bump-allocated from `0x0049e66c` up to a capacity of 0xfa (250) and flushed through `0x0040ff40` → `0x0044d300` at the end of the frame (or when full). A natural CITY frame holds 90–99 nodes.

## Simulation step and object lists (October 11, 2026, plan 10 task 1)

**Step boundary.** `0x004163d0` is the frame loop: each pass calls `0x00442e10(steps)` with the step count in `ecx` (capped at 0x14 = 20 per frame, `analysis/decompiled/004163d0.c` lines 101–107). `0x00442e10` polls controls once per frame (`DAT_004b7ed0 = 0x0044f800()`, already hooked at site `0x00442ea8`) and then calls the batch stepper `0x0044b2e0(steps)`. `0x0044b2e0` holds the inner per-tick loop (`decl esi; jne 0x0044b30c`, disassembly `0x0044b396`): each iteration increments the tick counters `DAT_004b7ecc`/`DAT_004c3990` first, then runs one step. The per-step hook site is the `E8` at **`0x0044b324`** (`e8 87 00 00 00`), expected target **`0x0044b3b0`**. The hook runs the original call and then dumps: at that point `DAT_004b7ecc` is the new tick and `DAT_004b7ed0` is the control word for the tick (polled once per frame, so multi-step frames reuse it; record at the step, not at the poll). Post-loop tail calls `0x00438450` once.

**Per-step driver** (`0x0044b3b0`): alternating timers `0x00475610/0x00475614`, rider/object pass `0x0044b0e0`, object scheduler `0x00438170(0)`, chase camera `0x00437c30` (gated on the player slice stamp equalling the tick), ring advance `0x00454120`, and traffic timers `0x0044dd90(distance_moved)` — so traffic `rand()` calls sit inside the tick at a fixed position after the rider pass.

**RNG.** `0x00441740` is `srand` (`movl %ecx, 0x49bce8`, fastcall). `0x00441750` is the game `rand` wrapper: if `DAT_004753b0 == 3` it returns `DAT_0049bce8` unchanged (the fixed-random mode the terrain/scenery oracles already use); otherwise it jumps to CRT `0x00455eb0`. `0x00455eb0` reads its seed from `getptd()+0x14` (`0x004592b0` is MSVC `_getptd` via TLS imports) and the arithmetic was verified by hand against the disassembly (`0x00455eb5..0x00455edb`): `seed*214013 + 2531011` (`0x343fd`/`0x269ec3`), returning `(seed >> 16) & 0x7fff` — the exact MSVC stream `Race::random()` reproduces (`src/race.cpp:531`). Consequence for the oracle: the live stream state is **not** `0x49bce8` (that global is only live in mode 3). The seed hook must write `*(getptd()+0x14)` by calling `0x004592b0` in the hook, and the per-tick `rng` field must read the same word after the step. `srand` call sites: `0x0043097c` (reseeds from its own `rand()` output) and `0x00431385` (seeds from `*param_1` via `0x00431380`; its caller is not yet identified, likely race setup through a behaviour table — open).

**Object lists.** `0x00438170(0)` walks the scheduler ring: `S = [0x0046dcc8]`, end marker `[S]`, first node `[[S+4]+4]`, links at `+0x4`, dispatch through `0x00437e60(node, tick, …)` (verified in disassembly `0x0043833a..0x00438365`; Ghidra's `DAT_0046dcc8[1]` prints as an address-relative index but is sentinel-relative `*(S+4)` — reading address `0x46dcc8+4` instead finds an unrelated flag, cleared at `0x00438379`). `0x00437e60` calls think at `+0xe0` and move at `+0xe4`. Node words: `+0xc`/`+0x10` schedule ticks, `+0x1c` position (the `0x00438aa0` insert sort key), `+0x28` lateral, `+0x2c` flag, `+0xc4` flags, `+0xec` speed. Riders and cars share this list (traffic despawn parks the object back to the free pool, matching `TrafficSystem` pool size 16 from `0x00438b70(0x210, 0x10)`). Bikes and rider-halves are separate nodes: every bike (player included) thinks with `0x00407a90`, rider-halves with `0x00434660` (the player's half mirrors the player bike's position/lateral). Non-rider nodes are dumped raw. Rider identity in the hook: node pointer equality with the player pointer `DAT_004642d8` marks the player bike; opponent grid-slot mapping is still open — discovery order shifts as cars spawn/despawn and riders reorder by position, so a stable cross-run opponent key is plan 10 task 6 work (never assume list order matches). Slice pointers must be converted to ring indexes in the hook (process-specific addresses); the current hook dumps them raw as `slice_ptr` for debugging only. Stamina/weapon/AI words in the plan's field table live in the rider half of the bike+rider pair (`*(bike+0x2fc)`; the hook currently omits them (native test skips absent fields).

**Field selection.** Bike words come from the existing `Bike::i32/u8` accessors (`src/bike.hpp:37`): `+0x1c` position, `+0x28` lateral, `+0xec` speed, `+0x23c` gear, `+0x2f0` condition, `+0x2f4` crash, `+0x140` slice, `+0xc0` flags, plus vertical height/velocity. Rider members map from `Rider` (`src/race.hpp:62`): stamina current/max, weapon `+0x1c8`, AI state words. Car fields map from `TrafficCar` (`src/traffic.hpp`): `+0x1c`, `+0x28`, `+0xec`, `+0x73` kind, `+0x7a` lane, `+0x7f` vehicle, `+0x71` phase.

## Free traffic (October 11, 2026, plan 05 task 2)

**Vehicle resources** (`0x004465f0`, see `docs/ACTORS.md` for the census):
`DATA/CARS/<CourseName>.CAR` holds `ANIM` (RRAN) and `CANS` (SNAC wrapper)
entries. `CANS` is **not** a flat cel: payload magic `SNAC`, size word +4,
type word +8, an embedded sub-container at +size whose own size word leads
(`0x0040b5f0` with index 0) to the cel block at `base + sub[+4] + 0xc`; the
vehicle mask `0x004b8fe0[i]` is that block's CCB flags word at +4
(`0x0040b620`). Mask bits `0x1c0000` are speed classes: `0x40000` slow,
`0x80000` oncoming, `0x100000` same-direction. The RRAN frames carry their
own indexed textures, so drawing does not need the CANS pixels.

**Ring cursors** (`0x004552e0`): `DAT_004b6e50` = front (base + 0x5000),
`DAT_0049f240` = back (base − 0x3000), `DAT_004b6e5c` = camera; both scroll
by the camera delta in `0x00454120`. The `0x0044dc10` window test
`[back, front)` applies only to objects with the knock-off flag `+0x1cc`
(riders); free cars always think, and `0x00437e60`'s tail parks them once
outside the window.

**Spawn timers** (`0x0044dd00`/`0x0044dd90`, driven per step by
`0x0044b3b0`): rate 2500 = 0x19·100, density default 6, reloads
`(rate·density·60) >> 8` toward zero. Oncoming spawns kind 2 at front−1
sample with lane `−1 − rand % left_lanes` (sub-slice +0x30); "behind"
spawns kind 0 at front−1 with right lanes (+0x2c); "ahead" spawns kind 0 at
back+1 while the rider is slow. Lane counts use the signed-char semantics
`(int8_t)((value + (value < 0 ? 0xff : 0)) >> 8)`.

**Placement** (`0x0044df60`): lateral `(road centre + lane·250 + 125)·256`;
vehicle type filtered by mask (loop excludes the last vehicle) and picked
with `rand() % accepted`; four random bytes (phase `+0x71` is a full int
slot 192..319 — a byte truncation freezes the car); serial `DAT_004b7e98`.

**Car think/move** (`0x0044e710`/`0x0043adc0`): speed accumulates through
`0x004498c0` (`+0xec += dt·rate`, clamped) toward the phase-scaled cruise
`((DAT_004b7e8c = 25) · phase >> 8) · 100` — note 25, not the timer rate
2500; rate `phase·0x12 >> 8`; obstacle following/lane changes
(`0x0044a070`/`0x00449e60`) not ported. Horn `0x0044efa0` → `0x00436830`
(0x12) when the look-ahead target is the player and `rand()%10 == 1`.
Native integration: `Race::set_traffic` steps the timers after the camera
update with the per-step camera advance; drawing reuses the verified
scenery projection with the depth-table scale at each car's Q8 depth.
Frame cycling and car CCB world-size scaling are native stand-ins.

Actor animation playback and descriptor recovery (October 11 continuation):

- REA (`rea-agents` 6.4.0, Ghidra provider) agrees with the exported
  `0x0043ae90` pseudocode. Original-function fixtures verify 2,048 playback
  steps independently. The selected draw-frame pointer is resolved before
  advancing; keeping only the post-advance frame changes the displayed frame.
- Duration is a signed byte, deadline comparisons are unsigned, and one call
  takes at most one transition. Deadlines accumulate from the old deadline.
- CANS runtime descriptors are 20 bytes, separate from RRAN graphics. The
  bounded `actor_descriptors` decoder reproduces all 194 descriptors built by
  `0x00445a60` (59 bindings). Metadata comes from CANS and 8-byte imported EXE
  source descriptors; only the initializer's relocation schema is native code.
- Rider/cop banks at `0x00474d78` each have 52 code slots, with 59 populated
  sequences total. Rider codes 20/21 refer to BSS records not initialized by
  the loader; direct selection rejects their zero draw pointers.

Owned actor graphics selection: CANS SNAC group records use signed high-16-bit one-based RRAN indices in their first word. The following attribute block supplies per-cell flags. `0x0040b6f0` combines `0x0040b730` and `0x0040b620`, then selects the RRAN frame through `0x0040b750`. All 672 cells match runtime helper calls. `select_actor_view` implements the angle-dependent portion of `0x0043af60`, including its unusual 0xff0000 wrap, signed-byte counts, yaw reversal flag 8, low-resolution span and full-circle formulas, elevation clamp, and roll correction limited to ±7. Across 9,312 original calls the selected RRAN index, flags, roll and imported hotspots match. Orientation calculation `0x00413ae0`, live animation triggers, shadow remaps and depth dispatch are separate remaining work.

Actor billboard dimensions: the RRAN frame width/height are logical view units. HI Bob images commonly double those dimensions, with odd dimensions rounded down (e.g. logical 14×33 maps to HI 27×65). `actor_node` therefore projects RRAN dimensions and samples the owned HI texture, preserving its own bounds. Empty shadow frames may have RRAN 1×1 dimensions but zero-size Bob pages, so they contribute no pixels. `ActorNodes` preserves shadow/body ordering and ground versus vertical Q8 displacement. Normal player riding animation `0x00407f30` has 1,024 matching captured steps; distance-based AI selection accepts a caller-supplied original distance, while crash-specific dispatch remains separate.

Napa vehicle CANS inheritance verified: all ten masks, all 207 view cells and all 30 decoded descriptors match original `0x004465f0` resources and `0x0040b200`/`0x0040b6f0` calls. The last four vehicles inherit the sixth CANS resource when their own sequential CANS lookup fails. Live car frame selection and actor projection can now use the same bounded CANS/EXE decoding as riders.

Crash actor behavior: `0x00403d60` copies current and previous motion from the bike to a separate rider, zeroes rider roll and several forces, sets drag 95 on the bike/70 on the rider, applies separate optional velocity impulses, raises the rider by 0x4b00 Q8 and bike by its contact-height constant, sets crash reason/state and collision dimensions, then chooses rider animation 29 for reason 2, 35 for reasons 4/5, otherwise 33 with queued -1. The numerical offline branch matches 64 captures across 49 fields per actor; route/network ownership and landmark traversal are deliberately separate. REA independently reproduces the existing crash-entry decompile (`analysis/oracle/rea-crash-entry.json`). The rider template at 0x463c10 has class 8 and update callback 0x406890; it uses `0x0043adc0` for independent crash motion. Crash animation branches `0x00407a90` (bike) and `0x00407f30` (rider) match 1,024 and 2,048 runtime steps respectively. `0x0040a600` handles walking, pickup, remount and wreck outcomes after stopping; the existing native crash timer remains a stand-in until those are ported.

Crash motion/recovery: `advance_actor_motion` implements the offline numeric portion of `0x00439920`, leaving `0x00439270` road advancement to a callback. Private flat-ring probes cover 512 inputs. `recover_crash_actor` implements offline decisions from `0x0040a600`; 256 cases compare bike/rider fields and animation state. Its walking speed is the constant 1200 returned by `0x0040a9d0`; proximity is longitudinal absolute distance <=9 and lateral absolute distance <6001 after subtracting 6000. Preserve `0x00449c20`'s logical sign shift for the wreck proximity test, including its unusual negative-difference result. Scope excludes directed player walking and route bounds, network/police ownership, and the separate `0x00404560` reset.

`remount_crash_actor` implements the local numeric reset from `0x00404560` and its copy from `0x004067d0`, including the detail that +0x104 is NOT copied by the latter. Original probes restore the linked rider's stamina from +0x38 to +0x3c; Race maps that to `RiderStats::max/current`. Linked route/landmark and network calls remain caller responsibilities. The fixture's private bike/rider/stamina/ring links avoid the live object heap.

Police scope for opening Napa: `0x00407f30` invokes `0x00406b80` when owner is 0, rider flag 0x10 is clear, no cop exists and `(tick & 0xfff)==0`; no level guard appears. `0x00406b80` constructs the cop 0x2700 Q8 units behind the player using templates 0x464310/0x464378 and rider row 15 at 0x464038. Personality 9 is copied by `0x004078f0` to runtime 0x4c7d64. It cycles speech IDs 350..357, chooses bike from 0x4641c8+15*5+level, resets current relationship columns to their defaults and replaces player-target relationships using place/difficulty. Original level-1 constructor evidence lives in `analysis/oracle/napa-police-spawn.tsv`. This is an explicitly induced constructor capture, separate from an unmodified 4096-tick spawn trace. REA export for 0x406b80 exactly matches the existing decompile.

Local arrest after `0x00407860` locates a non-crashed nearby cop: owner 0, rider/bike flag 0x10 clear, bike speed <300 normally or absolute bike speed <1800 in crash state 2, absolute cop-to-rider longitudinal difference <512 and absolute relative bike speed <5000. Owner -1 runs proximity logic but skips deciding local outcome. Original outcome is 4 with delay 180; native threshold tests cover the strict boundaries. Full cop lookup, AI pursuit and tick integration remain separate.

Cop actor allocation: `0x00405470` allocates 0x32c-byte bikes; `0x00405490` allocates 0x208-byte riders. Constructor capture must not apply bike-only offsets >=0x208 to the rider. Native `make_police_actor` retains imported virtual callback addresses only as opaque numeric metadata; native code never executes them. It imports cop-bank playback (bank 1), applies per-level bike selection/throttle percentages and reduced AI gears, initializes condition/stamina/weapon and pursuit relations, with ground/road target supplied by the caller. Captured level-1 numeric fields match; route/scheduler ownership, dynamic AI replanning and display-list integration remain open.
