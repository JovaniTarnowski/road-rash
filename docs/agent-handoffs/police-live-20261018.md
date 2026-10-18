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

## Follow-up (same day)

- Siren: `SoundSystem::siren` ports 0x00435ae0 (slots 30/31, SIREN loops 0x259
  updates then SIRENFST 0x79, each let finish its pass before the other starts;
  volume = linear^2 * 0x4fff >> 16 scaled by the siren channel, cap 0x4000, own
  mute gate). `Mixer::set_loop` stands for DirectSound `Play` on a playing buffer;
  `play_loop` now turns a playing one-shot into a loop as 0x004141c0 does. The cop
  posts event 19 from its think (0x00407bd0 head), a null record when it has no
  target (0x00409ca0) or is removed (0x004073a0). `sound_test` covers audibility,
  the alternation and silencing.
- Arrest while on foot: 0x00407f30 runs for the crashed player too
  (`police_think` called from the crash early-returns); a crashed rider's half is
  the body, so the arrest position and the scan list use `crash_actor.motion`;
  the 1,200-tick walk timer exemption (0x0040a9e0) applies to the cop itself
  (`police_owner = r.cop`). `police_test`: a knocked-off player is arrested while
  walking.
- Regression note: the first commit of this slice left `src/race.cpp` broken at
  HEAD 0e9e79d (zero-context hunk staging misplaced three blocks); 074e3bc fixed
  it. Verified in an isolated worktree at the fixed content (build + oracle-sim,
  field, schema2, police).

## Follow-up 2 (same day, clean worktree at 2dcd4e6)

- Arrest -> BUSTED screen link: `tests/race_screen_test.cpp` now drives the
  police_test scenario through `Game`/`RaceScreen` (Napa level 3, parked
  player, live cop arrest) and checks the career screen surfaces
  `RaceOutcome::busted`, keeps it through the 0xb4-tick run-on and fade-out,
  and ends. `frontend_test` already covers the last hop (`race_finished`
  queues the BUSTED video) with a synthetic outcome; a visual playtest of
  the video in `rr-game` is still not done.
- Clean-worktree gate: build + `ctest -j4` 104/104 (the git-ignored
  `analysis/oracle/sim-napa-solo.tsv` fixture copied in for `oracle-sim`;
  without it that one test fails on the missing file only).

## Open

- Original-capture comparison (`napa-cops`, `city-cops`, `napa-cops-crashed`):
  blocked on the shared oracle proxy. `tools/agent_workspace.py list` still
  shows three claims, but two name proxy copies in other worktrees; the
  main-tree claim is sim-l5-20261018, whose uncommitted proxy change
  (`RR_ORACLE_LEVEL`, calling the original level setter 0x004453f0) is in
  fact a prerequisite for a level-3 `napa-cops` run. No peer session is
  live. Do not overwrite that change or install a new DLL until sim-l5
  lands; a capture also needs a >4096-tick Wine run and new proxy field
  dumps for the cop. Until then cop movement is checked by the native tests
  and the recovered pseudocode only. Capture spec: `police-spawn-20261016d.md`.
- 0x00434f40 height term in the siren for a cop on the other branch of a split.
- DAT_004c5d7c == 4 and the network branches (+0x324 == 1, 0x004c4fb0).
- Cop rider-half name (0x00446bf0 index 9 reads outside the table, left blank).
- Playtest of arrest -> BUSTED video in `rr-game`.
