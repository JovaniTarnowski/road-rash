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

## 20261026 follow-up: parked-scenario parity + original-driven save

- Capture `napa-cops-parked-20261026` (Wine, COURSE=4 LEVEL=3 SEED=1 HUMAN=1,
  FRAMES=4800): race ran 4731 contiguous ticks then ended (0xb4 timer after
  the 4552 arrest); truncated past the tick reset. `police_test`:
  "cop matches on 635 ticks; RNG matches to 4730, opponents to 4730" and
  "NAPA arrest on tick 4552 as the original" (one RNG anchor at tick 1;
  first-tick spare rows 9..14 feed `spare_levels`). `CopCapture` parked row
  renamed to the 20261026 TSV.
- `fe-career.RRS` (other worker's Wine frontend acceptance run
  `acceptance-20261026/34069bf...`: original Big Game Cydney/City race,
  restroom-UI save): payload equals dumped `fe-career-saved.state`;
  `career-test --save-corpus` now pins the original-driven round trip
  (character 3, level 0, bike 2, cash 651, byte identity). Skip-guarded if
  the acceptance dir is absent.
- Batch in progress (background): ORDER=1 re-runs of parked (mid-race FRAMES
  for direct `race.end_timer` evidence) plus KNOCK=4400 crashed/order2,
  KNOCK=4460 foot/foot2, and CITY parked; table arrest/span values to be set
  from measured ticks.
- Pixel acceptance (done after the above): `frontend_test` gains an
  item-rect-driven masked comparator; `fe-main` (60%/98%/0%) and `fe-street`
  (67%/96%/0%) pinned green. Characters/restroom/jukebox/graphics measured
  (~1% exact restroom-class; characters portrait block) and recorded as
  background/portrait art-mapping gaps, intentionally not pinned.

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
