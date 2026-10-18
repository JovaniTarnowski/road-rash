# Re-baseline at 919c339 and police live-wiring analysis (2026-10-18)

Plan: 32 slice D (live police), after the field-parity-20261018 handoff.
Baseline commit: 919c339. Owned files: this handoff only. No source changes.
Working tree has unrelated dirty `CMakeLists.txt` and `oracle/aweman_proxy.c`
(terrain-fixtures-20261015 claim); left untouched.

## Evidence
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel`: exit 0.
- `ctest --test-dir build --output-on-failure`: 102/104 pass (654 s). The two
  failures, `first-run-smoke` and `game-race-wiring`, are environmental: SDL
  reports "The video driver did not add any displays" in the sandbox.
  Rerun with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`: both pass. So the
  native suite is 104/104 at HEAD when a display driver is supplied.
- Not rerun: ASan/UBSan, `make verify-sim`, Android/package gates.

## Police wiring: findings for the next agent
- `police_spawn_due`/`police_arrest`/`make_police_actor` have no live caller
  (`rg police src` shows only `race_setup.cpp:138` reading the config).
- Original: spawn in 0x00407f30 when owner 0, rider flag 0x10 clear, no cop,
  `(tick & 0xfff)==0`. First spawn is therefore tick 4096, beyond every
  2400-tick capture, so adding it cannot disturb the existing parity receipts.
- The live game runs the interleaved scheduler (`set_live_schedule(true)`,
  `race_setup.cpp:218`). A cop is a rider (`REVERSE_ENGINEERING.md:847`), so
  it needs a scheduler node, slices, park/wake, `built_riders_` accounting and
  an actor-node/draw entry. Appending to `riders_` mid-race touches
  `race.cpp:1707,2488,2901-2968` and `race_schedule.cpp:46,76,305,463`.
- Missing recovered behavior: cop think/move (cop-hold braking, state-3/4
  follow caps, `ai_state_follow` says "Police-only caps not modelled"),
  siren (`sound.cpp:349`), 0x00407860 cop-find proximity.
- Outcome path: `race_screen.cpp:236-242` decides won/lost/wrecked only; add
  `RaceOutcome::busted` (career.cpp already prices it, `career.cpp:113-139`)
  and a `Race::busted()` set from `police_arrest`. Needs a test that a real
  arrest reaches the career penalty and busted screen.

## Next actions
1. Recover cop think/move and 0x00407860 (rea-agents; exports under analysis/oracle).
2. Add runtime rider insertion (scheduler node + slices) behind a flag off in replay.
3. Wire spawn/arrest in `Race::step`, `Race::busted()`, race_screen outcome.
4. Test through `finish_race`; update plan 32 D boxes only when done.
