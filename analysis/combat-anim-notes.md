# Combat and roadside animation (CombatAnim slice, 2026-10-12)

Static/corpus evidence; no new Wine captures were run for this slice (oracle
prefix/proxy shared with active sessions). Baseline: `eefab89` + siblings'
uncommitted edits (SimReplay road_base/finish, MusicRace, RenderInfra).

## 1. knock_off form (verified — no change)

`Race::knock_off` (`src/race.cpp:486`) already matches `0x004052c0`
(`analysis/decompiled/004052c0.c`):

- Signature `(victim, stamina_damage, bike_damage, reason)` — two damage
  inputs + reason, mutating the victim, returning `+0x2f4 != 0`. No `random()`
  inside; grunt/crash draws stay upstream in the `0x004049c0` callers
  (`contact` player branch, `contact_car` player branch), preserving the
  shared-stream position.
- Reason table: 0 zeroes both; 1/4/5 keep both; 2 scales both by 7/4 with
  sar-bias `(p*7+((p*7>>31)&3))>>2`; 3 divides both by 8 with sar-bias
  `(p+((p>>31)&7))>>3`; all others (incl. silent-hit 6) zero both.
- `0x004050c0` (max `-=4p` floor base/2, current `-=64p` floor 0) and
  `0x004051e0` (`+0x2f0 -= q` floor 0) run with the owner/network bypasses
  (flag −1, or flag 1 without local ownership) as documented scope.
- Crash entry `0x00403d60` via `crash_reason` in assembly argument order
  (rider impulse, then bike): reason 4 `{-80,0,0}`/`{0,0,0}`, otherwise
  `{-256,0,1792}`/`{0,0,1024}` (`004052c0.c:36-49`, `00403d60.c:98-106`).
- Reason 4 without a crash selects riding animation 9 on the victim's `+0x70`
  block (native: pending `Rider::contact_animation`, consumed by
  `advance_ride_animation` — the existing `ActorAnimationState` interface,
  README row 11; no parallel struct added). Reason 5 multiplies `+0x188` by
  the scaled stamina damage in 32-bit wrap; the shared `+0x288` steering tail
  (all reasons) uses original-width products with sar-bias `/256` and the
  exact `±+0x29c` clamp form (`004052c0.c:57-68`).
- Call sites route through it: `contact` contained bike hits (reason 2),
  `contact_car` class 2 (magnitude/magnitude) and class 3 (magnitude + the
  `+0xc4&0xc` `(m+((full>>31)&15))>>4` bike-damage slot, `004049c0.c:95-102`),
  `resolve_hit` (reason 4, or reason 5 + `trunc(d/4)+1` for kicks).

Two comment-only fixes in this slice (stale text said the bike branch never
called `knock_off` / the reason-1 branch was live for bike-bike; class 1 is
dead for bike-bike since bikes are `+0x17c = 0x2300` tall, `004049c0.c:83`).

Failing-native-test-first per task: the reason-table synthetics in
`tests/crash_test.cpp` (`test_knock_off_reasons`) already cover rows
0/1/2/3/6, bypasses, reason-4 animation 9, and reason-5 impulse/steer/clamp
before this slice. This slice adds the missing negative-damage sar-bias
cases (reason 2 `p=-5 -> -8`, reason 3 `p=-9 -> -1`, matching the assembly's
sign-bias exactly where C++ truncation differs), plus a crash-impulse
argument-order case (reason 4 vs default vector pairs with a crash library
attached). Written to fail against a truncating-divide implementation; they
pass against the landed sar-bias form.

## 2. Roadside collision: in-road BORR actor path (new)

Traced `0x00454240` → `0x00451cb0` → `0x00438930` + `0x004373f0`/`0x00437660`
+ `0x00436a40`/`0x004049c0`:

- `0x00454240` builds runtime subslices per course sample. `0x00451cb0`
  writes the BORR lateral slots `[0x6c..]` (count byte `+0x60`) from the
  section's BORR record at `param_2+0x84`. A second emitter
  (`0x00437750`, DZHR-placed objects, byte `+0x3d`) feeds the same dispatch.
- `0x004373f0` tests each BORR slot against the rider's swept box
  (`lateral ± +0x174`, `height ± +0x17c`, sample fraction `0x80`) and calls
  `0x00437660`, which builds the static `DAT_004c3e90` hazard actor (class
  from template `0x00476cb8` word 1 = **1**, `hazard_template` census
  `[0,1,20,0,0,0,0]`) and runs the shared separator `0x00436a40` + the
  rider's contact handler (`+0x1a4`) against it. Only BORR positions strictly
  inside the road edges are instantiated (`0x00454240`: `iVar6 < iVar4 &&
  iVar4 < iVar8`); pictured trees outside the edges are never actors. Do NOT
  box every sign/tree.
- Hazard class-1 routing in `0x004049c0` (`param_1+0x2f4==0`,
  `other+0xc4==1`, `other+0x17c/2 <= self+0x17c`, i.e. the halved template
  `+0x17c`): reason 1 through the shared `knock_off`; small speed gap
  transfers impulse to speed, big gap enters `0x00403d60` reason 1. Class 2/3
  cannot fire for hazards (containment bit `0x40` is set but the
  `+0x17c < 0x2800 && +0xc4&0x10==0` test takes class 1 first).

Implementation (`src/race.*`, no tick-path allocation):

- `Race::set_hazards(std::vector<RoadHazard>)`: per-route-sample lateral
  spans in Q8, precomputed at course load from BORR profiles via
  `rr::road_hazard_spans(section, sample_base, random)` (`src/race.*`; the
  same BORR word walk as `roadside_objects` but emitting spans at sample
  fraction `0x80` instead of billboard rows). Stored per route sample;
  empty by default (synthetic routes unaffected).
- `Race::step` calls `collide_hazards(r)` for every rider at the traced
  point (after `collide_traffic`, before `advance_ride_animation`): swept
  lateral box vs the sample's spans, then the shared `0x00436a40`-equivalent
  lateral separation (mass-weighted, hazard treated as immovable) and
  `contact_hazard` — the class-1 branch of `0x004049c0` through the shared
  `knock_off` (reason 1), with the same small-gap/big-gap follow-ups.
- Failing test first: `tests/race_test.cpp` asserts the
  strictly-inside-edges rule picks the expected spans on a hand-made section
  (inside spans kept, outside spans dropped); `tests/race_test.cpp`
  asserts a rider crossing an in-road span takes reason-1 stamina damage and
  a lethal hit enters crash reason 1, while an outside-edge span never fires.

## 3. Animation in the Race tick (verified — no change)

`advance_ride_animation` is already called in `Race::step` for every rider
after contacts and before `update_camera`/snapshots, and `snapshot()` already
carries `anim/anim_frame/anim_queued/anim_loops/anim_deadline`
(`tests/sim_fields.hpp` offsets `+0x80/81/82/88/84`) plus
`attack_weapon/steal_until/hit_until` and nitro fields. It adapts the
existing `ActorAnimationState` + `advance_riding_animation` /
`advance_animation` (`0x00407f30` selection + `0x0043ae90` at most one
transition per call, deadlines accumulating) and the police numeric helpers
(`police_arrest`, `make_police_actor`) — README row 11, no parallel
interfaces added.

This slice adds a regression test (`tests/race_test.cpp`): with a ride
library attached, a `contact_animation = 9` selection plus an attack-swing
start both land in the same tick's snapshot (`anim == 9` resp. swing code,
`selected` set pre-advance), proving animation advances in the tick before
snapshots without a render owner.

## Acceptance

- `race-test`, `crash-test`, `scenery-test`, `traffic-test`,
  `sim_fields-test` pass under the lock protocol; full `ctest` shows no new
  failures (sibling slices' suites untouched).
- Oracle: no new goldens captured (shared proxy); `napa-contact`,
  `napa-contact-double`, `napa-weapon-*` captures remain open work.

## Open questions

1. Hazard template `+0x174/+0x178/+0x17c` dims: `0x00476cb8` reads as data,
   not the behaviour table — the pool object at `DAT_00476ce8` carries the
   real dims at runtime. A hazard-actor dump (class/dims/sample/contact
   fields) from a live capture would pin them.
2. DZHR-placed (`0x00437750`) hazards share the dispatch; their solidity byte
   (`param_2+2`: 1/2 = solid via `0x00437660`, 3+ = scenery-marking path) is
   ported as a span flag but unverified against a live capture.
3. `napa-crash-scenery` oracle capture (steer into the first in-road BORR
   span; `build/rr-course` prints sample/lateral) still needed for golden
   acceptance.
