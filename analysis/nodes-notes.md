# BikeShadowNodes slice notes (11a Task 3) — working trace, merged by owner at commit time

## 0x00446250 (bike+shadow dispatch) vs 0x004463e0 (bike+shadow+weapon)

Native `ActorNodes::rider` follows the 0x00446250 shape: shadow first
(clip 0 / `clip_ground=false`, ground without vertical), then body
(clip 1, ground + vertical>>8). Both call `0x00443af60`-equivalent view
selection with re-selected view after restoring `+0x90`.

Disassembly (`analysis/original-disassembly.txt`):
- `0x00446250–0x00446341`: bike path. `+0x90` save/swap on a
  `DAT_004642dc`/`+0x2fc` match (`0x004b94e4` vs `0x004b94ec`), then
  `0x0043af60`; `+0xbc = 0`; if `+0xa8` nonzero: `0x00446350` (shadow
  specialize), `0x0044ba40(..., size 0x200, lateral (+0x50 rounded), ground
  +0x4c, position +0x44, clip 0)`; restore `+0x90`; second `0x0043af60`;
  `+0xbc = 0`; if still nonzero: copy `+0xac` into node `+0xc` when
  nonzero, `0x004463c0` (body specialize), `0x0044ba40(..., lateral,
  (+0x48>>8) + +0x4c, +0x44, clip 1)`.
- `0x004463e0+`: same plus weapon overlay — yaw clamp `|yaw| > 0xe0000`
  clamps roll `+0xbc` to ±`0x50000`, then a third `0x0044ba40` with the
  weapon frame (`0x0040b750` selection off bike `+0x1c4`/`+0x54` state).
  Native `game_main.cpp rider_nodes` has NO weapon overlay: any third node
  in the original dump vs two native nodes = this gap. Also note the body
  lateral uses `(yaw-clamped?)` — no, lateral identical; only roll clamped.
- `0x00446350`: `0x00446380` (ensure `0x004460c0` colour, fallback
  `0x00446190`) then `0x004463a0(colour == 0xde ? 0xdf : 0xe0)` — i.e.
  shadow colour is 0xdf only when the body colour is cop (0xde), else 0xe0.
  Native `ActorNodes::texture` maps bank→palette but `rider()` passes the
  caller's `colour` for the shadow bank too: for a cop-coloured rider this
  picks the wrong sheet. Player/opponent colours are 0..14 so both give
  0xe0 — no live-race effect, but `--native-actors` must assert it.
- `0x004463a0`: node resource = `0x00466688` (Bob marker), node `+0x38` =
  colour id. `0x00443820` at draw time (`0x0044d300` dispatch, flag 0x2000)
  copies frame `+0x34` remapped (colours 0..14) or raw (0xde/0xdf/0xe0).
  Native `rider_texture` ports this; `render_nodes` throws on 0x2000
  (correct: specialization must happen before render).

## 0x0044d300 dispatch: no shadow raster mode

`analysis/decompiled/0044d300.c:84-156`: per node, quality-mask test
(`flags & 0x00475648` → flat fill `0x0044d620`, else texture path
`0x0044ce60` + `0x00443820` if flag 0x2000 + `0x004180b0`). Shadows take
the same textured path with plain index-0 transparency (CelTexture
flags 2). **No darkening/translucency table.** Candidate cause #4 is
closed by code reading; pixel confirmation still needs the Step-4 frame
match. No `world_renderer.cpp`/`texture.cpp` change expected.

## 0x00413ae0 vs native actor_angles — DIFFERENCE FOUND (to verify)

Original (`analysis/decompiled/00413ae0.c`):
- rider (`+0xc4 == 8`): `depth = slice_depth*2 + (position&0xff)*2`
- non-rider: `depth = slice_depth + (position&0xff)`
- `lateral = slice+0x10 + ((actor+0x50 + sign-rounding) >> 8) - camera_x`
  where slice+0x10 is the projected-slice road x (native uses
  `orientation.road_x`, which `game_main` sets to `a.road_x + camera.x`
  — i.e. DOUBLE-ADDS camera.x since actor_angles subtracts camera_x;
  check `rider_nodes`: `orientation.road_x=a.road_x+camera.x` then
  `actor_angles` does `road_x + lateral_q8/256 - camera_x`. Net =
  `a.road_x + lateral_q8/256`. If `a.road_x` is already world-x the net is
  right; needs `road.hpp screen_x` semantics check.)
- `view = approx_atan2(depth, lateral) * 0x10000`; yaw = road_heading
  (wrapped) − view (wrapped) + actor heading `+0x68`. roll = `+0x6c`.

Native `actor_angles` (`src/actor_render.cpp:11-19`):
- `depth = actor.depth + (position & 255)`, then `depth *= 2` if rider.
  **ORDER DIFFERS**: native doubles the fraction too
  (`(slice+frac)*2` vs original `slice*2 + frac*2` — same thing!
  `(a+b)*2 == a*2+b*2`. NOT a bug. ✓).
- lateral: `road_x + lateral_q8/256 − camera_x`. Matches IF road_x carries
  the same slice value. ✓-pending (see above).
- `mul(approx_atan2(depth,lateral),65536)` — original `iVar2 * 0x10000`
  is also a multiply (wrapping). ✓ same.
- heading wrap: native wraps heading AND view; original wraps slice
  heading (`param_3+0x1c`) and view. Same role. ✓

So orientation looks faithful; the yaw-sign / depth*2 candidates look
clean on reading. The live `orientation.rider` flag: native sets
`!crash_active || item.body` — crashed bike node gets rider=false →
halved depth → wrong view AND wrong scale. Original `+0xc4` is the
scheduler node type (8 = rider?), not crash state. To check against dump.

## Scene-0 evidence (napa-drive frame 0, 10 nodes)

- Bob nodes 2/3 (far pair) and 8/9 (near pair), all frame 47. Shadow
  (0xe0) then body (0x9 / 0x1) — order matches native.
- Shadow quad is SMALLER and HIGHER than the body (far: shadow w 877878
  h 390168 top 7393884 vs body w 942906 h 2178438 top 5703156). Native
  shadow uses the same cell frame via `library_.graphics[bank+2]` — the
  shadow RRAN bank, correct per dispatch (shadow re-selects view with the
  shadow graphics context `+0x90` swap, same frame index).
- Frame 750: pairs share frame (23/23, 22/22, 3/3) — body-vs-shadow bank
  mismatch candidate looks clean at rest. Mid-jump (vertical ≠ 0) still
  untested — needs the jump frame.
- Colours: player body 0x1? node 9 colour 1, node 3 colour 9 — opponents.
  Player node pair at frame 0 is nodes 8/9 (near, colour 1??). Hmm — the
  player colour: `game_main` passes `rd.colour`. Which pair is the player?
  Needs rider object state — NOT IN DUMP (see below).

## Dump-v3 gap (blocks --native-actors): CONFIRMED, recapture needed

- Rider list pointer `0x004642d8 = 0x1403c10` is a heap address; the v3
  heap closure roots are only `{0x0049f250, 0x004bc800, 0x004ba190}` —
  the scheduler ring / rider objects are NOT walked. No pre/post region
  contains `0x1403c10`. So dumped object state (position/lateral/height/
  animation selection `+0x98`/`+0x90`) is absent.
- What's available without recapture: Bob node bytes themselves
  (frame +0x34, colour +0x38, 4 view points, flip bits 0x64/0x65) +
  `0x004b9220`/`0x004b9270` (actor graphics pairs, captured in .data) +
  `0x004742d4`-family counts. I.e. we can verify texture/palette
  selection and geometry self-consistency, but NOT rebuild nodes from
  object state.
- Recapture recipe: extend `closure()` roots with the scheduler ring
  (`0x0046dcc8` → S, walk links, or simpler: add `0x004642d8` target +
  each rider's bike half `+0x2fc` and animation state). Simplest robust
  fix: add a 4th root `{0x004642d8, 4}` + walk rider structs? closure()
  only follows 0x200-byte heap blocks two levels. Rider/bike objects are
  bigger than 0x200? With size-0x200 snapshots, fields past +0x200
  (e.g. +0x2fc rider-half ptr, +0x324) would truncate. Better: explicit
  `dump_rider_objects()` writing full bike (0x340) + rider-half bytes per
  scheduler node into the TSV (like `dump_sim_tick` fields) rather than
  growing the closure. Flag to owner: **recapture required; propose TSV
  rider-object extension, not closure growth**.
- napa-drive capture command (from SIM_SCENARIOS pattern + plan 11):
  `RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_PIXELS=1
  RR_ORACLE_SHOT_EVERY=150 RR_ORACLE_FRAMES=900 oracle/run.sh napa-drive`
  (gives frames 0/150/300/450/600/750/900; current dumps are every-150 to
  750 — frame 900 missing, and no jump frame guaranteed).

## Weapon check (club without attacking)

- `0x004460c0` (colour select): cop → 0xde; `0x004c5b00/0x004c5af9`
  nonzero → `+0x1c4/+0x328` byte; player (`0x004c7fb0`) → player
  rider-half `+0x1e0+5`; else grid lookup. Starting weapon therefore
  comes from rider-half `+0x1e0+5` / `+0x328` — native `Rider.weapon`
  init (`race.cpp:180 riders_[0].weapon = weapon`) source to verify
  against `0x00463c78` row (game_main: `rider_value` reads +0xc/2;
  weapon byte offset in that row?). Read-only so far (race.cpp held).
- Riding-animation selection `advance_riding_animation` already has 1,024
  captured steps matching (REVERSE_ENGINEERING.md:683). The club-at-2400
  report is most likely the missing weapon OVERLAY node (third node),
  not a wrong base frame: native draws 2 nodes where original draws 3.
  Confirm from a dump frame with weapon ≠ 0 (frame 750 pairs show only
  2 nodes each — check `+0x1e0` state when recaptured).

## Page-cache staleness (candidate #5)

`ActorNodes::texture` keys `pages_` by (bank, frame, palette) and caches
`std::optional<CelTexture>` by value in a std::map — insertion never
invalidates (test asserts stability). BUT `bank+2` shadow with a
rider colour 0..14 computes `palette = colour` (bank==1?0xde:bank==2?
0xe0:bank==3?0xdf:colour → bank 2 gives 0xe0 ✓; bank 3 gives 0xdf ✗ for
player shadows — bank+2 of bank 1 is 3, and cop shadows must be 0xdf
only when body is 0xde... wait: bank 1 = cop bank, shadow bank 3 =
cop-shadow sheet; native passes colour → palette=colour for bank 3?
No: `bank==3?0xdf:colour` — bank 3 forces 0xdf. But per 0x00446350 the
shadow colour should be 0xdf only if body colour is 0xde, else 0xe0 —
and bank 3's SHEET is shadoc (cop shadows) regardless. Hmm: native bank
semantics: graphics[bank+2] with bank<2 → bank 2 (bike shadow) for bank
0, bank 3 (cop shadow) for bank 1. `rider()` is only ever called with
bank 0 (game_main passes 0). So shadow always bank 2 → 0xe0 ✓. Cop path
(bank 1) is dead code live. No live staleness; key includes palette so
remap swaps can't alias. Candidate #5 looks clean on reading.
- Real staleness question: `library_.graphics[bank].frames.at(cell.frame)`
  texture vs `rider_texture` page — `texture()` prefers
  `graphic.texture` (imported RRAN page) when present; when is it
  present for Bob nodes? Bob nodes specialize at draw time (0x2000
  flag), so graphic.texture should be null for rider banks... untested.
  `--native-actors` should assert which source each node uses.

## Next actions (needs tree clearance / lock)

1. Owner decision: TSV rider-object extension + napa-drive recapture
   (frames incl. a jump frame). I can write the proxy patch but NOT
   while race.hpp/race.cpp are held? — proxy file `oracle/aweman_proxy.c`
   is ALSO touched by the sibling AI session (uncommitted). Coordinate.
2. Phase 2 (under build lock): add `--dump-nodes` to rr-game, capture
   native t=300/900/1500/2400 + jump screenshots, write
   `--native-actors` mode, confirm failure, fix, re-verify.
