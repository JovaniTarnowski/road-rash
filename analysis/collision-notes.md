# Contact and knock-off unification (plan 16 Task 1)

`0x004052c0(victim, stamina_damage, bike_damage, reason)` is one shared
mutating helper (`Race::knock_off`, `src/race.cpp`), not per-caller logic.
It performs **no RNG**; the grunt/crash draws stay upstream in the
`0x004049c0` caller (`Race::contact_car`), preserving the shared-stream
position (two `random()` draws before the helper on the player path).

## Reason table (inputs scaled before applying)

| Reason | Transform of both damage inputs |
| --- | --- |
| 0 | Zero both |
| 1, 4, 5 | Keep both |
| 2 | x7/4, sar-rounded (`(p*7 + ((p*7>>31)&3)) >> 2`; trunc-toward-zero for non-`INT32_MIN` products) |
| 3 | /8, sar-rounded (`(p + ((p>>31)&7)) >> 3`) |
| Others incl silent-hit 6 | Zero both |

## Damage application (offline/local branch)

- `0x004050c0`: max `-= 4*p`, floored at base/2; current `-= 64*p`, floored
  at 0. Owner/network bypasses retained as documented scope: flag `-1`
  (remote) or flag `1` without local ownership skips both applications and
  returns the untouched `+0x2f4` state; offline play (flag 0) always applies.
- `0x004051e0`: bike `+0x2f0 -= p`, floored at 0, same bypasses.

## Crash entry and tail

When either result is zero, crash entry `0x00403d60` runs with the
assembly's argument order (rider impulse first, bike impulse second):
reason 4 passes `{-80,0,0}`/`{0,0,0}`, every other reason passes
`{-256,0,1792}`/`{0,0,1024}` (`Race::crash_reason`; `crash_car` is the
reason-2 car path through the same entry). Reason 4 without a crash selects
riding animation 9 (recorded on `Rider::contact_animation` for the render
owner; the `Rider` object carries no animation block of its own). Reason 5
multiplies `bike+0x188` by the scaled stamina damage in original 32-bit
wraparound; when that field is nonzero and the victim still rides, steer
`+0x288` gains `(wide + ((wide>>31)&0xff)) >> 8` (32-bit `+0x2d8 * +0x188`
first), clamped to `+-+0x29c`.

## Car branch

11a's `contact_car` local `knock_off` lambda now calls the shared helper:
class 3 passes stamina `magnitude`, bike damage
`(magnitude + ((full>>31)&15)) >> 4` (the `+0xc4 & 0xc` caller slot);
class 2 passes `magnitude`/`magnitude` and scales inside. Behavior
identical; the existing `traffic-test` car-contact synthetics pass
unchanged. Double-contact order/side effects are the handler's, not the
helper's: this change asserts no single-bool shortcut, verified by the
unchanged boundary/nudge/crash-rejection probes plus the new reason-table
synthetics in `tests/crash_test.cpp`.

## Deltas vs prior notes

- `analysis/car-notes.md` "Crash entry reason for cars" (`begin_crash`
  needs no change, `Race::crash` hard-codes 0): superseded — `crash_reason`
  now carries reason + vector pair; `crash_car` delegates with the default
  pair.
- `analysis/car-notes.md` open question on stamina words rising in crash
  cases: still open; the shared helper applies `-4*p`/`-64*p` before entry
  and entry itself writes no stamina words.
