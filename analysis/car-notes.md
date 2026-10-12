# Rider–vehicle contact (plan 11a Task 1 trace)

All addresses verified against the reference EXE
(SHA-256 `73d7f974…a60f18a2`); decompiles in `analysis/decompiled/`.

## Detection: `0x004368f0` (scheduler contact pass)

Called once per tick from the scheduler driver (`0x004383b6`, after the
`0x004383ab` ring resort; a second resort follows when any contact fired).
Outer loop walks the scheduler ring; a node participates as the *primary*
only when its schedule word `+0x10` equals the tick `DAT_004b7ecc` and
`(byte)+0xc0 & 6 == 6`. The `(pos & 0xff) - 0x80` pre-pass (`0x004373f0`,
walls/statics) runs when that value is `< +0x178`. Inner loop walks forward
from the primary's link while `(byte)+0xc0 & 5 == 5`, breaking at the first
pair whose longitudinal gap exceeds the summed half-lengths
(`+0x178` each); lateral must be within summed half-widths (`+0x174`
each), the slice-half bits (`+0x36` of the `+0x2c` node, masked with the
primary's, `& 3`) must share a roadway, and height within summed
half-heights (`+0x17c` each). On overlap both `+0x1a0` links are set to
each other, `0x00436a40` separates them, then each side's `+0x1a4`
handler runs (bike: `0x004049c0`; car: `0x0044efa0`, horn only).
REA second opinion (owner-supplied) matches this shape.

Split-roadway exclusion is the `+0x36 & 3` shared-half test: rider and car
on different roadways never collide. The native `Race::step` pair loop
already has this (`sa.dual && a.route != b.route`); the car pass must use
the same rule.

## Separation: `0x00436a40`

Masses `+0x194` (bike 256, car 1800), impulses `+0x184`/`+0x188`/`+0x18c`,
contact flags `+0x19c` (bit `0x40` = "other width contains ours", set in
the forward-overlap-prologue). Previous-position copies `+0x30..+0x44`
choose the separation axis. Longitudinal moves go through `0x00439270`
(course-ring traversal — no scratch equivalent, so the oracle fixture
pre-seeds impulses instead; documented approximation). DAT_004977b4 = 1
when any pair fired.

## Bike contact handler: `0x004049c0` (bike side, incl. vs cars)

Head: `+0xec` speed, `+0x180` elasticity, `+0x1a0` other, `+0x2fc` half;
impulse magnitude `|184|+|188|+|18c|`, `local_20 = magnitude >> 10`;
early-out when `+0x1a0 == +0x2fc` with speed < 10 and height 0;
crashed (`+0x2f4 != 0`) riders take only the `+0xf4 += 0x150` branch, the
`0x00404f4d` nudge and the tail (never a new crash — fixture cases
64..127/192..255 verify rejection).
Live branch (`+0x2f4 == 0`, other `+0xc4 != 2`): cars (`+0xc4 == 0x10`)
always pass this guard. Player-only (`param_1 == DAT_004642d8`) sounds
via `0x00436830`: impulse `< 0x1771` gives grunt `(rand&1)+3` plus crash
`rand&1` (silent when `< 0x7d1` with small `+0x188`); above gives grunt
`(rand&1)+4` and crash `(rand&1)+1` — two `0x00441750` draws, same as the
bike-bike path already ported in `Race::contact`.
Hit class off the car's height (`+0x17c = 8960 >= 0x2000`, always tall):
`+0x19c & 0x40` clear gives class 3, set gives class 2 (class 1 needs
car `+0x17c < 0x2800` with `+0xc4 & 0x10 == 0`, impossible for real
cars — not covered). Impulses scale by `+0x180 >> 8` (192/256).
Class 2/3 call the knock-off decision `0x004052c0` (class 2 scales
`local_20` by 7/4, class 3 by 1/8), which applies stamina damage
(`0x004050c0`: max `-4·p`, current `-64·p`, floor max at base/2) and bike
damage (`0x004051e0`: `+0x2f0 −= p`), then `0x00403d60` on knock-off.
Class 2 without knock-off transfers speed (`+0xec += +0x184`) when the
speed gap `< 0x1771`, else crashes with **reason 2** (rider animation
29 — fixture: 32/256 cases, all reason 2/anim 29). Class 3 shoves
laterally (`+0xf0 ± |184|/2` with zero-cross reset) and cuts speed
(`−|184|/4`). Both end with the `0x00404f4d` nudge
(`(w1+w2+sign)>>4 · +0x18` away from the other lateral) and the tail:
impulses/`+0x19c` zeroed, `+0xec` clamped to [−1000, 30000], `+0xf4` to
[−10000, 0x600], `+0x288` steer adjust via `+0x2d8`/`+0x29c`, then
`0x00407410` (sets `+0x1f8`).
Non-bike branches (`+0xc4 == 2` rider-vs-rider-half bookkeeping,
`0x20`-class scenery bump, `+0x298`/`+0x1e8` switch) do not apply to cars.

## Car dimensions (via `0x004387a0` from template `0x00475698` +0x40..+0x54)

`+0x170 = 128`, `+0x174 = 16384`, `+0x178 = 64`, `+0x17c = 8960`,
`+0x180 = 192`, `+0x194 = 1800`; `+0xc4 = 0x10`, `+0x1a4 = 0x0044efa0`,
think `0x0044e710`, move `0x0043adc0`. The per-kind `0x00475700` block
goes to `0x004756dc` scratch (kinds 0/2 vs 1/3 differ only in word 0:
16384 vs 40960 — lane-target scale, not dims; fixture kind bit covers
the nudge via the template width only). Bike for reference (from
`0x00463ba8` words 16..21): 24 / 7680 / 64 / 8960 / 192 / 256.

## Car reaction: `0x0044efa0` (horn only)

Sets `+0x1f4 = 1` with a `+0x1f8` timer (`0x3c`, or `0xf0` for chasers);
queues sound 18 only when the player (`DAT_004642d8`) is the look-ahead
target and `rand()%10 == 1`. No stop, swerve, or speed change — the car
keeps driving through the rider. Fixture: `snd_after44ef − snd_after4049
= 0` in all 256 cases (scratch bike is never the player; documents the
player-only gate).

## Crash entry reason for cars

`0x00403d60` reason **2** (animation 29; `begin_crash` already maps 2 →
29). No `crash.cpp` change needed — reason is a caller parameter.
`Race::crash` currently hard-codes reason 0; the car path must pass 2.

## Fixture: `analysis/oracle/napa-car-contact.tsv` (git-ignored)

256 isolated `0x004049c0` calls (scratch bike vs scratch car in the
`capture_crash_entry` style; impulses pre-seeded as if `0x00436a40` ran —
live-ring traversal has no scratch equivalent). Encoding: bit 0 head-on
(car ahead, speed −2000) vs from-behind (+1800); bits 1–4 lateral edge
sweep (edges 8–15 set the `0x40` containment bit → class 2, edges 0–7 →
class 3); bit 5 speed class (2500 vs 9000); bit 6 already-crashed;
bit 7 car kind (narrow/wide, nudge-only effect). Logged per case:
dir/edge/fast/crashed/kind/tick, full bike/car in/out words, half
stamina +0x38/+0x3c in/out, sound-queue depth around both handler calls,
`contact_flags/crash/reason/animation/frame`. Result: 0 faults;
32 new crashes (all fast-contained, reason 2, anim 29); 32 slow-contained
speed transfers; 64 side shoves; 128 crashed-rider rejections
(`+0x2f4` stays 1, reason untouched).
Capture DLL was a throwaway under /tmp (repo `aweman_proxy.c` untouched,
installed DLL restored; SingleAnchorReplay confirmed no conflict).

## Open questions

- Stamina words rise (+826/+832) in the 32 crash cases while falling
  (−4·p/−64·p) elsewhere; no writer in `0x00403d60`/`0x00407410`/anim
  path explains it. The golden captures it; the port must reproduce it
  (test will confirm or refute the trace).
- Per-kind word 0 (16384 vs 40960) does not enter dims; presumed
  lane-target scale in `0x0044e710` — untested.
- Impulse seeding is an approximation: with real `0x00436a40` masses
  (1800 vs 256) the live impulses differ from the seeded sweep. The
  handler golden is exact given its inputs; end-to-end parity needs the
  plan 10 `napa-crash-car` tick scenario (recorded as remaining work).
