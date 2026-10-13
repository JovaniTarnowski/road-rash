# Simulation and course implementation context

Evidence: reference executable, `analysis/decompiled/<address>.c`, selected
`analysis/original-disassembly.txt` ranges, current source and the census JSON.
All recovered behavior below is static/corpus evidence, pending runtime goldens.

## Replay is the first dependency

`tests/oracle_sim_test.cpp` currently reads `solo` and unconditionally calls
`set_demo_hold(true, ...)` when it is set. It also passes zero controls to
`Race::step` for every solo tick. A solo race can be human-controlled; these are
independent axes. The comment describing human goldens does not implement this
distinction. Add `driver=human|demo`, read the original `DAT_004753c8` branch flag,
and record actual player-think entry/counter for `0x00434660`. Do not infer this
from a filename or nonzero recorded input.

The dump header reports bike and character as `-1`; replay hard-codes SPEC index
1, demo weight/stats and personalities. The game uses the requested/career bike,
weight and character. Replay creates a vehicle library without EXE descriptors,
does not attach traffic/crash configuration to Race, and uses an absolute Napa
road-base anchor absent in the game constructor. Use one factory, with explicit
configuration and no hidden replay-only setup.

The post-step dump is produced after `0x0044b3b0`. Replay's current green
heuristic chooses the first frame whose position/speed changed, anchors RNG to
that frame's **post-step** state, and then executes that frame's control word.
This can be off by one RNG transition. Capture pre-step RNG and exact tick/green
boundary (or use the preceding post-step state when it is truly equivalent).
Add a synthetic replay with exactly one random call at green and another later;
compare both expected calls and field states. Keep absolute original tick values
for animation deadlines; local tick zero is not automatically equivalent.

`RR_ORACLE_COURSE`, `SIM`, `SEED`, `INPUT`, `RECORD`, `SOLO`, `DRIVE`,
`SHOT_EVERY`, `FRAMES`, `POLICE`, `RANDCOUNT`, `RANDTRACE`, `RANDLOG` are read
by the current proxy. `RR_ORACLE_LEVEL`, `RR_ORACLE_BIKE` and `RR_ORACLE_START`
are not. Add and validate those selectors at actual race construction, before
writing level-5/nitro/near-finish capture commands. `DRIVE=2` alone is not proof
of the human branch. Input files contain one hexadecimal word per tick; EOF
sets controls to zero and schedules termination after 0xb4 ticks.

`tests/golden.hpp` requires magic `# rr-oracle 1`; simply bumping the version
as plan 16 suggests makes old captures fail to parse. Adding optional fields
can keep v1, or support explicitly enumerated versions. Parser absence should
stay absence, not zero. The current comparator iterates native fields and skips
ones absent in the original. New acceptance must require the intended field set,
actors and coverage, including car/police/animation fields. This prevents a
missing snapshot field from making a test green.

## Contact and knock-off (plan 16 Task 1)

`0x004052c0(victim, stamina_damage, bike_damage, reason)` is a mutating helper,
not a predicate over `(const Rider&, other bike, impulse)`. It calls
`0x004050c0` first, then `0x004051e0`, and returns whether `victim+0x2f4` is
nonzero. There is **no RNG call inside it**. Random grunt/crash sounds upstream
in `0x004049c0` still affect the shared stream.

| Reason | Transform both damage inputs before applying |
| --- | --- |
| 0 | Set both to zero |
| 1, 4, 5 | Keep both unchanged |
| 2 | Multiply by 7 then divide by 4, truncating toward zero |
| 3 | Divide by 8, truncating toward zero |
| Others, including silent-hit 6 | Set both to zero |

`0x004050c0` reduces maximum stamina at stats `+0x38` by damage×4, floors it at
base `+0x34`/2, reduces current `+0x3c` by damage×64 and floors it at zero.
`0x004051e0` subtracts bike damage from `bike+0x2f0`, floored at zero. Both have
owner/network bypass branches; retain them as documented scope rather than
flattening them into multiplayer behavior without a decision.

If either returns zero, crash entry `0x00403d60` receives two 3-word vectors:
for reason 4, first `{−80,0,0}`, second `{0,0,0}`; otherwise first
`{−256,0,1792}`, second `{0,0,1024}`. Preserve the assembly's argument order.
Reason 4 without a crash selects animation 9. Reason 5 multiplies `bike+0x188`
by the scaled stamina damage. If that field is nonzero and the victim is still
riding, steering `+0x288` gains trunc(`+0x2d8 × +0x188` / 256), then clamps to
±`+0x29c`. Perform original-width products/wrap before truncation.

11a's car branch already contains a local version of this damage logic. After
11a lands, extract/reuse one implementation rather than adding a second with
different silent-hit or steering behavior. Double-contact tests must compare
the original handler invocation order and side effects, not merely assert
one bool changes.

## Scenery collision (plan 16 Task 2)

The complete direct-call census of crash entry is:

| Call site | Function | Context/reason |
| --- | --- | --- |
| `0x00404f13` | `0x004049c0` | Strong contact, class-derived reason |
| `0x004053b2` | `0x004052c0` | Depleted stamina/condition, supplied reason |
| `0x00434825` | `0x00434660` | Manual dismount: input 0x100, speed <1000; reason 6 |
| `0x00438e34` | `0x00438c70` | Sustained excessive steer/grip loss; reason 6 |
| `0x0043a50b` | `0x0043a190` | Landing/crashed-rider motion; reason 6 |
| `0x0043ac62` | `0x0043a610` | Wall case, null vectors; reason 3 |
| `0x0043ad96` | `0x0043a610` | Wall case with supplied vectors; reason 6 |

This list alone does **not** prove scenery is non-solid. There is a positive
actor-construction path: `0x00454240` reads BORR positions built by `0x00451cb0`;
positions strictly inside road edges are instantiated with `0x00438930`, pool
`DAT_00476ce8`, template `0x00476cb8`, at sample fraction 0x80. Template class
at +4 is 1, which the contact handler treats separately (including halving
the object's +0x17c size). The pool is created by `0x004552e0` with object size
0xec and count 0x14. Therefore in-road roadside hazards are a concrete trace
target, distinct from ordinary billboard rendering.

Next trace `0x00438170` → broad-phase contacts → `0x004049c0`, pool initialization
`0x00438b70`/`0x00438a70`, and the resulting hazard dimensions. Dump one original
hazard actor's class, dimensions, sample and contact fields. Do not add collision
boxes to every sign/tree or infer solidity from the image. Building collision
belongs to terrain/wall `0x0043a610`; class 2 has a different contact branch
which sets skid/reaction timing from `object+0x124`.

## Weapons and attack timing (plans 11/16)

`0x004096f0` first checks the selected runtime descriptor's flags (0x600000)
and animation frame byte `rider+0x81` (frame 2 is a special event path). It then
finds and validates the target through `0x00409400`, `0x004095a0`, `0x00409c10`.
This is not a universal hit at tick 7 of an 18-tick attack.

Stealing the weapon has **all** of these gates before `rand()&3 == 3`:
target steal deadline `rider+0x1cc < tick`; target frame <2; attacker's current
attack weapon `+0x1d4 == 0`; target current attack weapon neither 0 nor kick 3;
target is actively attacking (`0x00409510`). Only then consume the steal RNG.
Transfer both held `+0x1c8` and current attack `+0x1d4`; clear both on the
target; set the attacker's steal deadline to tick+300; update relationships
via `0x0043c980`; reset/queue the original attack/riding animations.

Failed steal can still hit. Target hit deadline `+0x1d0` suppresses damage
until strictly expired; a landed hit sets it to `tick+(5−level)*6`. After damage
there is a `rand()%(6−level)` repeat-attack decision and possibly another AI
random decision. Preserve conditionally consumed calls and order.
`0x00409ad0` scales strength by stamina, floors at half strength, multiplies
by the per-weapon EXE table at `0x00465a00`, then calls knock-off. Kicks add
condition damage trunc(damage/4)+1 and use reason 5; others use reason 4.

`0x00403d60` has no held-weapon `+0x1c8` transfer/drop write in the inspected
body. This does not yet establish all-race persistence: finish setup/career
selection still need checking. Do not implement world pickups or weapon loss
without an actual constructor/caller. Start with transfer conservation under
the traced steal gates, plus a crash test that measures what happens.

## Surfaces, reverse and nitro

`0x0043a0c0(subslice, lateral_q8)` selects surface 0–3. It uses strict road-edge
tests, event/marking byte +0x37, terrain type +0x3c and shoulder edges +0xe8/+0xec.
The four drag/grip tables are already imported by `read_physics_tables`:
signed drag bytes `0x0046dce4`, grip `0x0046dce8`, steer gain `0x0046dcf8`, damping
`0x0046dd08`, skid loss `0x0046dd18`. Reuse the exact classifier before inventing
grass/dirt multipliers; synthetic `SliceEdges` alone does not cover all of it.

`0x00439390` uses unsigned position addition, compares sample boundaries as
signed values, and chooses a branch on backward transitions too. Native
`Race::cross_sample` currently clamps negative positions at zero and only
changes route forward from route 0. The soak plan's universal monotonicity
assertion is invalid for reverse, crashes/remounts and different-length branches.
Test graph identity and physically bounded deltas with explicit transition
exemptions instead. Contact's final clamp is speed [−1000,30000]; this is not
proof that every motion path applies that same reverse limit.

Nitro state is player-global, not initially a per-rider counter:

| State | Source |
| --- | --- |
| Recharge increment | `0x00465a28` |
| Throttle boost multiplier Q8 | `0x00465a2c` |
| Maximum charge/time | `0x00465a30` |
| Charges remaining | `0x00465a34` |
| Minimum activation charge/time | `0x00465a38` |
| Charge/time accumulator Q8 | `0x004c5f10` |

`0x0040b0e0` copies six words from the selected bike spec at +0x14c and starts
the accumulator at maximum<<8. `0x00434660` activates when **both** nitro bit
0x10 and accelerate bit 1 are held. `0x0040b110` requires boost flag +0xc0 bit
0x20 clear, charges>0, accumulator/256 >= minimum; it decrements one charge,
sets that flag and emits event 14. Releasing accelerate or finishing stops
boost (`0x0040b170`, event 21). `0x00407a90` calls `0x0040b1a0`: boost drains
256×tick delta; otherwise recharge increments until the maximum test fails.
`0x0040adb0` multiplies resulting throttle force by boost multiplier and shifts
8 **arithmetically**, unlike the truncating divides elsewhere. The HUD at
`0x004469d0` updates charges and recovery display; plan 17 must include these
fields in snapshots. A held-input test should test depletion/recharge/retrigger,
not only that a charge counter never becomes negative.

## Animation snapshot schema

`ActorAnimationState` already exists; do not create the old plan 11 struct.
The common block begins at object+0x70. `0x0043ae90` reads:

| Absolute object offset | Width/sign | Native field |
| --- | --- | --- |
| +0x80 | i8 | animation (−1 sentinel) |
| +0x81 | i8 | frame |
| +0x82 | i8 | queued (−1 sentinel) |
| +0x84 | u32 | deadline (0xffffffff initial sentinel) |
| +0x88 | u8 | loops |
| +0x7c | pointer | selected descriptor **before** advancement |

Record descriptor identity as animation/frame, not host pointer. The current
`selected` native field exists to retain this pre-advance selection. Deadlines
accumulate from the previous deadline and there is at most one transition per
call. Ordinary rider animation currently lives in `Game::actor_states`, advanced
by `Game::step`; it is absent in a pure Race replay. Move this state into the
simulation owner before adding Race snapshot comparisons. Car descriptors live
in `VehicleLibrary`; cops use bank 1; rider and crash bike/rider are separate
objects and cannot share one animation field set.

## Course census and finish events (plan 17)

The graph resource is one `NOD ` id 1 per course; sections are `SGS ` id 1.
Level changes finish events, not a choice of five graph resources.

| Course / original index | Horizon | Sections | Independent routes | Full-route sample range | Terrain tags | DZHR records | ANIM / CANS resources |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CITY / 1 | City | 15 | 2 | 5606 | DLBR, LIHR | 494 | 12 / 6 |
| HIWAY / 2 | Sierra | 26 | 4 | 5985–6095 | DLBR, LIHR, NTMR | 466 | 10 / 6 |
| MEDLY / 3 | Penin | 35 | 8 | 5598–5681 | DLBR, LIHR, NTMR | 1253 | 11 / 6 |
| NAPA / 4 | Napa | 15 | 8 | 6224–6319 | DLBR, LIHR | 306 | 11 / 6 |
| CANYN / 5 | PCH | 18 | 4 | 5423–5443 | DLBR, LIHR, NTMR | 194 | 11 / 6 |

Resource counts are not vehicle masks; resolve those through `VehicleLibrary`'s
SNAC inheritance. The JSON has every MSRR family id per section. Enumerating
all branch combinations matters even on Napa: current `make_route(course,bool)`
chooses the same branch at every split and misses six of its eight routes.
Plan 17 needs a sequence of choices keyed to split node identity, and Race needs
graph-safe branch transitions when flattened route lengths differ.

Finish recovery chain:

1. `0x004525c0` scans CESR entries (header20, stride12) for raw type 6,
   **signed** parameter byte +9 such that `parameter−level == 1`; it returns
   the first matching local start sample. `0x00453e60` stores it at section+0x2c.
2. `0x00453f40` computes graph-relative remaining distance, using preferred
   split successors, section lengths and that finish offset.
3. `0x00454240` marks runtime subslice byte +0x36 bit0x20 when active runtime
   event type is 7 and its parameter matches level+1. It also establishes
   global finish `DAT_004b6e58` from section-local finish and ring position.
4. `0x00406890` compares actor position with that nonzero global and calls
   `0x00437800`, which marks rider and bike +0xc0 bit0x10 and records finishing
   tick/order. Player control then coasts/brakes; `0x00416010` decides the race
   outcome and delay. It does not select course finish coordinates.

`0x0044b250` initializes camera/grid origin to 0x3200 (sample50), independent
of level; per-slot offsets are selected separately by `0x00405600`. The
`0x00468f58` table is read by menu function `0x00420810` for mileage text.
Do not force graph distances to equal it exactly: preferred-route display
distance and alternate physical routes differ.

Measured first qualifying finish **event-start** sample, relative to graph
root sample0, across all independently enumerated routes:

| Course | Level 1 | Level 2 | Level 3 | Level 4 | Level 5 |
| --- | --- | --- | --- | --- | --- |
| CITY | 1821 | 2840 | 3865 | 4510 | 5419 |
| HIWAY | 1790–1810 | 2630–2650 | 3275–3295 | 4185–4295 | 5546–5656 |
| MEDLY | 1802–1818 | 2703–2719 | 3626–3689 | 4927–5010 | 5496–5579 |
| NAPA | 1715–1740 | 2565–2590 | 3570–3665 | 4280–4375 | 6164–6259 |
| CANYN | 1862–1882 | 2719–2739 | 3685–3705 | 4215–4235 | 5361–5381 |

These are corpus coordinates, not captured finish ticks or an assertion about
the event end boundary. Validate whether exact crossing is start/end/latched
when integrating iterator flags. Repeated parameter1 and parameter0 events
exist later in Napa/CITY; use first matching semantics, not a uniqueness assert.

## Factory and field setup

Plan 17's `RaceSetup` ownership is sound if pointees remain stable: Race keeps
references/pointers to actor libraries and traffic and a terrain callback. Do
not capture a stack-local `RaceSetup&`/`this` that becomes invalid when the
factory return moves. Capture stable owned pointees and ensure destruction
retires Race before its dependencies. Seed/configure at construction explicitly.

`0x00405600` sets `DAT_004642d4 = 0x10` for offline play and loops slots
1 through `<count−1`: player plus fourteen other riders, fifteen total.
The current count≤7 API is an intentional incomplete field, not the exact
offline field. Recover `0x004317a0`, roster slot identities, demo/career mode
differences, ownership flags and per-slot RNG from constructor `0x00405990`.
Use observed original actor identity rather than discovery order in the dump.

Police numeric config/setup/arrest are already in `police.*`. There is no live
`Police` scheduler class yet. Recover think/move `0x004060b0`, `0x00406ee0`,
nearby lookup `0x00407860` and rider spawn decision in `0x00407f30`; do not
decide presence by only scanning an EXE per-course table. Add actual live
spawn/chase fields to the full-field golden before calling this accepted.
