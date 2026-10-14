# Handoff: plan 10 Task 5 tick-208 contact tail (October 14, 2026)

## Result

Ported the missing `0x004049c0` tail into `Race::contact` (`src/race.cpp`):
the `0x00404f93` steering kick (`+0x288 += +0x2d8*(new_f0-old_f0)*2>>8`
sar-rounded, clamped to `+-+0x29c`, skipped when the rider went down) and the
`0x00404feb` impulse/flag clear (`0x184/0x188/0x18c/0x19c = 0`).

Effect on `sim-napa-solo-human.tsv` (`--driver=demo`, single green anchor):
tick-208 `rider.0.steer` gap closed from 1888 (native 504) to 45 (native 2437
vs original 2392). Full gate: 94/94 pass. `tools/sim_known_divergences.json`
gains the human-golden entry (exact divergence line + output sha).

## Mechanism (evidence-backed)

- The handoff's "no knock" reading of end-of-tick `raw_188 == 0` was wrong:
  the `0x00404feb` tail zeroes the impulses every tick, so 0 means "cleared",
  not "no contact". Native instrumentation found exactly one tick-208 pair
  (slots 0 vs 2, `branch=full`, `lat=-80`).
- The original's +1723 steer jump comes from the lateral branch (`0xf0 +=
  hit/2`) plus the tail kick, both previously unported in bike-bike
  `contact()` (`contact_car` already had its kick). The 45-unit residual is
  the AI steer exit `0x288` (~46 units); the identical contact kick preserves
  it (458+1934 = 2392 vs 504+1934 = 2437 approx).
- Frozen-opponent volatile words (`0x108/0x10c`) are stale pool memory in the
  original (`0x004387a0`/`0x00405990` never write `0x108`; pool objects are
  reused) and absent from every golden (node rows carry only
  think/position/kind), so the harness cannot seed them. Orig frozen `0x108`
  ≈ 321/6417 vs native 0 (proven by hit-magnitude math: orig hit 2286 vs
  native 2526).

## Audit notes (no action)

- `move()` bend-push chain matches `004395e0.c` bit-for-bit; the boost prefix
  (multiplayer-only globals) and `0x324` ghost/teleport branches (solo
  `param_5` = 0/1, no ghost object) are correctly omitted. Negligible:
  `wrap_angle` threshold (`0x7fffff` vs `0x800000`, one value), `div256` vs
  sar-round on negative push (off-by-one), unsigned `local_4` shift
  (unreachable: brake returns 0 at standstill).
- `separate()` matches `00436a40.c` line-for-line. `contact_car` may apply
  its kick without the `local_21` (went-down) guard the assembly has —
  flagged, not changed (out of scope).

## Next

A current-era SIM capture with frozen bike-word coverage + working SIM
captures (blocked: `sim-fault-20261014.md`). Then: ai-exit `0x288` residual
and grid-setup fix order #1.
