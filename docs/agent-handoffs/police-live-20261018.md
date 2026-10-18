# Live police wiring (2026-10-18)

Task: plan 32 slice D / plan 24 Task 2. Baseline commit: b15ed31 (HEAD 919c339
sim state). Supersedes the "not recovered" claim in
`rebaseline-police-20261018.md`: `0x00407860` and `0x00407f30` were already
recovered (`analysis/oracle/rea-407860-copfind.md`, `rea-407f30-arrest.md`);
what was missing was the cop-only AI branches and the live integration.

## What changed

- `src/race.hpp`, `src/race.cpp`, `src/race_schedule.cpp`, `src/race_pedestrians.cpp`:
  `Rider::cop`; `Race::set_police`, `spawn_cop` (0x00406b80 + 0x00449910 slice
  walk), `find_cop` (0x00407860: backward 0xa00 then forward 0x500 over rider
  halves), `police_think` (0x00407f30: `police_arrest`, `police_spawn_due`),
  `remove_cop` (0x004073a0 on parking, single player), `busted()`.
  The cop is always the last rider, at most one; `riders_` is reserved so
  references stay valid when it is appended.
- Cop-only AI branches from the decompiles: 0x004082f0 (scan cap 0x5000, +0x1a4
  decay with reset below 0x32, no speed reset, early return), 0x00408cd0 (no edge
  test), 0x00409ca0 (window 500, downed-target speed caps), 0x0040a4f0 (always
  snaps), 0x004092e0 (weapon-1 and relationship gate), 0x00409460 (no kick).
- Excluded from the place table and the finish order (0x00407550 loops
  0x004c7fb0, 0x00437800 skips the cop); cop animation uses bank 1 and the cop
  palette (`ride_bank`, `Game::sync_actor_states`, `rider_nodes`).
- `src/race_setup.cpp` enables police for non-solo races; `src/race_screen.cpp`
  maps `busted()` to `RaceOutcome::busted` (career penalty and BUSTED video path
  already existed).
- `tests/police_test.cpp`: live section on Napa level 3 with a parked player.

## Evidence

- `police-test` (needs the disc): no cop before tick 4096; at 4096 the cop
  spawns on slice 11 (player 12800 - 39 slices = 2816, same as the original
  constructor capture `napa-police-spawn.tsv`); it chases in follow state 4
  against the player, slows on approach, and `busted()` is set 440 ticks later.
  That is native behavior, not a comparison with an original run.
- Receipt for the full suite: see `docs/VERIFICATION.md` entry of the same date.

## Open

- Siren audio: event 19 is still `default: break` in `src/sound.cpp:349`
  (0x00435ae0/0x00435840 dual-buffer siren, no RNG).
- No original-capture comparison of cop movement yet (`napa-cops`, `city-cops`,
  `napa-cops-crashed` in `police-spawn-20261016d.md`); until then cop pursuit is
  a port of the decompiled branches checked by the native test only.
- DAT_004c5d7c == 4 (cop drops a player target) is treated as never true offline;
  network branches (+0x324 == 1, 0x004c4fb0) are not ported.
- Cop on-foot/crash recovery (0x0040a600 exemption already in `crash.cpp`),
  cop rider-half name string (0x00446bf0 index 9 reads outside the table, left blank).
- Playtest of the arrest -> BUSTED video flow in `rr-game`.
