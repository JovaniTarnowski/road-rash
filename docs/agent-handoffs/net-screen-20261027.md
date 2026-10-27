# Plan 34 Task 6: online race presentation — October 26, 2026

Baseline `ac341a9`; existing `plan34-impl` checkout
`/Users/archit/Dev/rr/.plan34-impl-20261026`. Session suffix uses UTC; user-facing
date is October 26 (Los Angeles). Serialized implementation, no subagents.
The active main-checkout frontend work and its CMakeLists were untouched.

## Implemented

- `RaceOptions::online` injects an already-started, stable-address `net::Client`
  into RaceScreen. `iterate` polls it once each frame and renders its latest
  predicted tick. It does not call `Game::tick` on the authoritative instance.
  Polling/stepping continue during quit prompts, focus loss and backgrounding;
  background rendering/audio can still sleep/mute. Escape opens original string
  316 and a second Escape leaves the room. Pause/restart actions cannot freeze
  or rebuild a network race locally.
- Rendering/audio use a detached presentation copy, allocated through
  `race_setup.cpp` (`copy_race_presentation`). The selected human becomes row
  zero only in this copy; physics, RNG and canonical roster indices stay on the
  client. Per-seat headings/back-off derive from shared state. Local dashboard,
  aliases and original AI names follow the selected rider. Source data stays
  unchanged even when SoundSystem updates engine rpm and nearby vehicle sounds.
- Rollback has bounded per-tick effect metadata and two observers. Predicted
  effects can be seen repeatedly during re-simulation; confirmed effects publish
  once, in order, after every preceding input agrees. This covers correct
  predictions promoted by incoming seals, not only corrected ticks. Sound queues
  drain once per simulated tick so old events cannot fill the simulation queue.
- `SoundEventRecord::source` identifies riders and pedestrian pool entries.
  `SoundHistory` deduplicates `(tick, source, event)` outside snapshots, retains
  at most window+1 ticks and caps per-tick keys. Removed prediction sounds may
  finish. Newly introduced sounds after a correction still play. Client playback
  batching is capped at 256 and outcome announcements at 32.
- Automatic Result reports are emitted from confirmed terminal metadata, using
  its exact per-tick state hash, not the latest predicted state. Terminal rule
  follows the preceding design handoff: all humans done (departed humans excluded)
  or 3600 ticks after the first finish. Every seat reports the same tick/hash.
  Synthetic-cutoff harnesses explicitly disable this and report their cutoff.
- Only server Official results decide the RaceScreen result and fade; the full
  table is returned in `RaceEnd::official`. Individual predicted outcomes never
  trigger career/results/cutscene transitions. Task 7 must route these results
  to mano screens with no career effects.
- Original strings 427/428/429 supply confirmed finish/wreck/bust announcements.
  F1–F8 send caller-supplied chat slots; relays use aliases from Start. The race
  log holds six persistent lines without an expiry timer, with the newest line
  drawn in the normal 0x15-pixel top strip (MULTIPLAYER.md traces 0x00446c30).
- `smooth_remote=true` applies 100 ms correction-only smoothing to remote human
  poses; it never changes local pose/camera, AI, inputs or simulation. Route
  changes and large teleports snap through the existing blend rules.
  `network_hud=false` shows RTT, correction depth and substituted-input count
  when enabled. Both keys persist in Settings; menu controls/launch are Task 7.
  Overlay textures cache unchanged text and invalidate on renderer reset.
- `net::race_config(Start)` builds compact simulation slots in roster order.
  StartHuman.seat remains a room identity, which can be sparse after leaving.
  Bots and tests use the shared builder. Client is explicitly non-movable because
  its rollback observers capture its address. Spectators do not submit Input.

## API ownership for Task 7

1. Create a Client with a factory returning `make_race_setup(assets, exe,
   net::race_config(start))`. Provide the actual production Hello gate (source
   fingerprint plus self-test and verified data digest; still to wire at startup).
2. Consume lobby events until Start. Keep the room client alive across races.
3. Call run_race/RaceScreen with `online`, local `assets`, Settings and chat slots.
   Screen moves immutable course assets from Client's setup into Game; heap
   allocations retain addresses. **Game must retain those assets until the
   client's race is replaced or destroyed.** Game is persistent in game_main.
   Its RaceInstance is presentation-only during the network screen.
4. On return, use `RaceEnd::official` and mano result videos/screens, with no
   career change. Keep the Client for room/rematch messages. A new Start builds
   fresh assets before the next screen. Room client errors still need frontend
   messaging and reconnect controls in Task 7/10.

The Host/Join UI is not enabled by this slice. game_main's existing run_race
accepts the extended RaceOptions; Task 7 provides the online caller.

## Verification

- Release configure/build: `cmake -S . -B build-p34-rel -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; `cmake --build build-p34-rel --parallel 8`.
  `ctest --test-dir build-p34-rel --output-on-failure -j 4`: **114/114**, zero skips,
  35.96 s on the final code. Native transport OFF in this existing build cache.
- Debug configure/full build and CTest in build-p34 (transport ON): **115/115**,
  zero skips, 419.46 s. Final focused rebuild/checks after the shared builder and
  local HUD/string refinements: presentation+tooling 2/2; correction presentation
  fixture 1/1 with full AI/cars and active sound, 5.01 s. Final stable-address
  declaration is compile-time only; Release full gate includes it. Later cache
  invalidation at the asset handoff is also covered by the final focused Debug
  test and the final Release gate.
- `net_client`: 84 cutoff chaos rooms (2–8 players × four RTT settings × three
  loss settings), plus two real-Server initial-wreck rooms (2 and 8 humans) with
  automatic Result reports. These terminal fixtures are not full-course finishes.
- New `net_presentation`: sound dedup/new correction sounds/bounded history,
  local/AI/camera exclusion from smoothing, branch snap, settings round-trip,
  six-line log, confirmed-effect ordering/promotion with/without rollback,
  predicted terminal suppression. The rendered fixture uses sparse room seats
  2 and 5, full AI/cars, scripted changing remote inputs causing actual rollbacks,
  local audio and a headless comparison peer. Authoritative hashes match every
  frame across chat, quit and focus events. Official table alone ends the screen.
  This uses scripted protocol fixtures, distinct from the real Server chaos test.
- Local rr-netd on 18080, health 18081; final two-bot cutoff smoke at 480: both
  hash `5741817043016304586`, official/agreed. Server seed varies by race, so
  hashes compare within a run. No VPS changes or network deployment.
- Native scene screenshot inspected (commercial assets, local only):
  `/Users/archit/Dev/rr/.plan34-logs/net-screen-20261027-final.png`.
- No live C++ LSP service exposed. agent_lsp.py clangd fallback checks on
  race_screen.cpp/rollback.cpp pass with `--tweaks=none`. Default Apple clangd
  --check returned exit 3 from its ExtractFunction refactoring self-test
  (break/continue extraction), not a compile diagnostic; disabling tweak tests
  gives zero errors. clang-format 22 dry-run/Werror and diff
  whitespace checks pass on changed C++ files.
- An initial full gate caught the central-factory guard rejecting the first
  presentation allocation in game_types.cpp. Allocation moved into race_setup;
  tooling passed afterward. No test/guard weakened, no golden updated.

Receipts copied to `/Users/archit/Dev/rr/.plan34-logs/net-screen-20261027/`.
Original offline oracle/golden tests pass in the native suites. No claim of
new original-game multiplayer equivalence, device cost, browser/Windows/Android
cross-play, full-course result acceptance or production soak. Existing Task 3
hardware/platform blockers remain unchanged.

- Focused ASan/UBSan: existing Homebrew LLVM 22 `build-p34-fuzz`, RR_FUZZ=ON,
  transport OFF; build `net_presentation-test`, run with
  `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`: pass. Address and
  undefined-behavior checking enabled; leak detection explicitly not covered.
- The plan's Superpowers skills are unavailable in this client. Equivalent
  serialized source ownership, implementation, review, compiler/test gates and
  resumable handoff workflow was used.

## Files, resources and remaining work

Owned: client/rollback/effects, Race sound/view metadata, game_types, RaceScreen,
view_state, race_overlay, settings, race_setup, sound_event, client/presentation
tests, bots, cmake/net_tests and dated progress/verification/plan handoffs. No
unrelated changes or commercial files staged.

Claims (owner PID 37010): `773a9846-6a3d-4a4b-9699-63f87496712f`,
`981c6dd9-30c4-4045-93c3-491cae54a788`,
`6e89c8e4-89a7-4cf4-bfdb-c4c427ad1c6a`. Reused predecessor build dirs with no
competing owner. Owned server PID 44200 stopped cleanly after the bot smoke; no network process
remains. Claims are released after the completion commit/push.

Next: Task 7 room/frontend controller, production compatibility gate, original
Host/Join/vote/chat screens, invite links, official mano results and rematch.
Then Task 8/9 transports/deployment and later platform/soak acceptance. Do not
mark the complete online game or plan 34 accepted from these component gates.
