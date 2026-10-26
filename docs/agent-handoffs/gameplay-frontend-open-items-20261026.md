# gameplay-frontend-open-items-20261026

Task: README open items — live police, wall/crash stand-ins, car/pedestrian
branches, race MIDI, sirens/car engines; jukebox/mix/controls, mano-a-mano,
seeded + driven saves, menu pixel/layout acceptance.

Baseline: `28dc959` (docs: trace original mano-a-mano netcode; plan 34).

## Owned / changed files

- `src/race.hpp`: `Race::traffic()` accessor (car-engine select needs the pool).
- `src/sound.cpp`: nearest-two select walks bikes + traffic cars
  (`0x004350e0`); bikes slots 3/5 (`0x004349c0`/`0x00435030`), cars slots 4/6
  (`0x00435090`: freq doppler*0x3b, vol cc*0x4fff/0x6400). Includes
  `traffic.hpp`.
- `src/race.cpp`: ped-ped comment corrected to the `0x004368f0` `& 3` gate
  (no-contact is faithful, not unported).
- `tests/sound_test.cpp`: car-engine pins (slot names, traffic accessor,
  near-car audible / far silent in others channel).
- `tests/career_test.cpp`: `--corpus` now drives a real-table career (Cydney,
  win City, affordable upgrade, save) and pins reload equality + byte identity.
- `tests/frontend_test.cpp`: full layout sweep over all 223 original items
  (2 scrollable, 28 hidden placeholders); vertical overflow allowed, horizontal
  fit hard.
- `README.md`: Status rewritten for the above; mano-a-mano notes plan 34 in
  scope per Oct 26 owner decision (not yet built).
- `docs/PROGRESS.md`: timeline entry for this session.
- `docs/VERIFICATION.md`: gate entry with commands/outcomes.
- `docs/REVERSE_ENGINEERING.md`: `0x00434fd0`/`0x00435090` car route +
  `0x004368f0` ped-pair gate.
- `docs/CURRENT_STATUS.md`: Frontend row updated (native driven round trip,
  layout sweep, car-engine pins).

Build directory: `build-agent-open` (owned Debug).

## Evidence

- `sound-test ../cdrom/ROADRASH`: 0 failures.
- `career-test`, `--corpus`, `--save-corpus`: 0 failures.
- `frontend-test ../cdrom/ROADRASH`: 0 failures (layout sweep + menu-09
  60% masked / 98% exact / 0% large-diff).
- `police-test`, `crash-test`, `traffic-test`, `pedestrian-test`,
  `car_contact-test`, `music-test`, `race-test`: 0 failures (police live
  spawn/chase/arrest + on-foot arrest pass; cop oracle fixtures absent,
  skipped).

## Remaining work

- Original-capture parity for the cop pursuit (needs `napa-cops-*` /
  `city-cops-*` fixtures; absent in this tree).
- Original-driven save capture via `tools/frontend_acceptance.py` (Wine);
  native-driven round trip is pinned, original-driven is not.
- Full unmasked menu pixel equality (blocked on GDI-hinting text shapes;
  masked capture + layout sweep are the current acceptance).
- Plan 34 online multiplayer (in scope, not built).
- Android hardware validation; Linux/Windows ports (out of this task's scope).

## Live processes / resources

- No Wine/oracle captures run. No Ghidra writes. `build-agent-open` only.
