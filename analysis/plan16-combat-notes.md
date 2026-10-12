# Plan 16 combat remainder (2026-10-12)

Traced from `analysis/decompiled/004052c0.c`, `004049c0.c`, `004096f0.c`,
`00409ad0.c`, `00409c10.c`, `004095a0.c`, `00409bd0.c`, `0040b0e0.c`,
`0040b110.c`, `0040b170.c`, `0040b1a0.c`, `0040adb0.c`, `00434660.c`,
`0043ae90.c`, `00407a90.c`, and the EXE tables below. Static/corpus
evidence; no new runtime goldens were captured (oracle captures were not
run for this slice).

## What changed

- **Knock-off routing on bike contact** (`src/race.cpp` `contact()`):
  contained bike hits now run the shared `knock_off` helper with the
  reason-2 two-damage form (magnitude, magnitude), matching the `0x004049c0`
  class-2 path already used by cars. Small-gap hits transfer speed;
  big-gap hits enter `crash_reason` reason 2. Glancing (class-3) hits keep
  the lateral branch. Previously the bike branch never called `knock_off`.
- **Weapon hits at descriptor frames** (`fight`/`fight_hit_frame`/
  `fight_reach`/`resolve_hit`): the tick-7 stand-in now fires only with no
  ride bank. With a bank (`set_ride_library`), hits fire on the riding
  animation frame-2 event once per swing (`attack.fired`). Reach is the
  `0x00409c10` rule via `fight_reach`. Steal enforces every traced gate
  before exactly one `rand()&3` draw (target `steal_until` expired, target
  frame < 2, attacker fists, target armed non-kick, target mid-attack);
  transfer moves held `+0x1c8` and attack `+0x1d4`, clears the target, sets
  attacker `steal_until = tick+300`. Hit deadline `hit_until`/`invulnerable_until`
  = `tick+(5-level)*6`; repeat-attack `rand()%(6-level)` and non-player
  `rand()&3==2` draws preserved in order. Damage is `0x00409ad0` through
  `knock_off` (reason 4, or reason 5 + `trunc(damage/4)+1` for kicks).
- **Nitro** (`nitro_think`/`nitro_charge_tick`/`nitro_throttle_force`,
  `NitroConfig`, `set_nitro`): activate needs nitro+accelerate held, boost
  flag clear, charges > 0, accumulator/256 >= minimum (event 14, one charge
  spent); release/finish stops boost (event 21); per-tick drain/recharge
  `0x0040b1a0`; throttle force × boost multiplier arithmetic `>>8`
  (`0x0040adb0`). Setup copies the player spec +0x14c words with EXE
  defaults `{0x11,0x180,0x78,0x64,0x1e,0xf}` from `0x00465a28` (measured);
  the player bike SPEC tail carries the same six words. Player-only, wired
  in `step_rider` at the `0x00434660` point (nitro, then drive, then steer;
  the old `apply_controls` path is inlined there for the player so boost
  applies to the throttle force).
- **Animation in the Race tick** (`advance_ride_animation`, called in
  `Race::step` before snapshots): contact animation 9 and attack swing
  codes select on the per-rider `ride_animation` block, then
  `advance_riding_animation` runs every tick. Snapshot carries `anim`,
  `anim_frame`, `anim_queued`, `anim_loops`, `anim_deadline`
  (`tests/sim_fields.hpp` + `oracle/sim_fields.h`, offsets `+0x80/81/82/88/84`)
  plus `attack_weapon`/`steal_until`/`hit_until` and
  `nitro_charge`/`nitro_throttle`/`nitro_charges`. `oracle-sim-test`
  passes the new keys through when the golden dumps them, else counts
  them uncovered (never failing); it also attaches the ride bank, attack
  codes, and nitro setup to the replay race.
- Compat: `attack_hit_tick` kept as an alias (game_main is owned by the
  playtest-visual-fixes session and was not touched).

## Deltas from the research note

- `knock_off` reason-2/3 scaling uses sar-with-bias (`(p*7+((p*7>>31)&3))>>2`,
  `(p+((p>>31)&7))>>3`), matching the decompile exactly; the "truncation
  caveat" in the old comment is resolved by the bias form (kept both paths
  documented in code).
- `0x004096f0` flag gate (`0x600000` on the runtime descriptor via
  rider+0x7c): NOT enforced — no native bank carries that word
  (`ActorGraphicsDescriptor` has no flags word). The native gate is the
  frame-2 event only; flag-gating is open until the word is traced.
- `0x00408d50` relationship updates and `0x0043c980` (network-only branch)
  are not ported (no-ops noted in code); the former needs the AI brain
  rating layout the Race does not carry.
- `0x004095d0` attack-start animation selection is approximated by the
  existing attack-code table + side latch; `0x00409bd0` shove keeps the
  backhand approximation.
- Nitro accumulator/throttle are per-rider copies of player-global state
  (offline single-player equivalent); `DAT_004c5f10`/`DAT_00465a34` globals
  are `nitro_charge` on the player + `nitro_charges_` on the race.
- `0x0040adb0`/`0x0040acd0`/`0x0040ad10`/`0x0040ad30` full drive path is not
  ported — only the boost-multiplier branch; AI nitro is not wired.

## Evidence

- `race-test` (0 failures): knock-off double-contact bound, 1000-seed
  weapon conservation, nitro deplete/stop/recharge, snapshot anim presence.
- `sim_fields-test`: green-anchor steal replay updated for the real steal
  gates (neighbour must be mid-attack with a nonzero timer that never
  crosses zero inside the window; `-8`); absent/present new-field checks.
- `crash-test`: knock-off reason table still passes (shared helper
  untouched). `bike`, `traffic`, `police`, animation/actor suites pass.
- Full fast suite: 60/60 pass (before this slice's final edits; touched
  suites re-run after: race, sim_fields, crash, bike pass; oracle-sim
  replay unchanged).
- Oracle: `sim-napa-solo --driver=demo` diverges identically before and
  after this change (frame 74 tick 75 lateral, rng equal) — pre-existing,
  not a regression. No combat/contact/nitro goldens exist yet
  (`napa-contact`, `napa-weapon-steal`, nitro captures are still missing).

## Open questions

1. Which bank carries the `0x004096f0` flags word (`rider+0x7c → +0x10 → +4`)?
2. Where does the original call `0x00407a90`/`0x00407f30` relative to
   `0x0044b3b0` — is per-tick `advance_ride_animation` at the right point?
3. New captures needed: `napa-contact`, `napa-contact-double`,
   `napa-weapon-steal`, `napa-weapon-crash`, nitro deplete/recharge, and a
   proxy dump of the new anim/combat/nitro fields.
4. `attack_ticks = 18` swing cap without descriptor completion is still a
   stand-in value (kept, documented).
