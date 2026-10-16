# Race MIDI hook background (20261016f, read-only)

Task: Plan 13 background — race MIDI hook (`GRUNGE1..5.MID` + `.SBK` bank),
pump/teardown chain, siren/car-engine slots. READ-ONLY: no tracked file edited,
no builds, no captures. Baseline HEAD `0141e7f` (verified `git rev-parse HEAD`;
`git status --short` shows only `?? analysis/*` scratch dirs, no tracked
modifications at turn start).

Owned outputs (this turn only):
- `docs/agent-handoffs/midi-race-20261016f.md` (this file)
- `analysis/midi-20261016f/` (`notes.md`, `rea-433e30-pump.md`, `rea-433cf0-teardown.md`)

## REA spot-check (2 calls max, pinned env, `--format md`)

Env: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`rea-agents@6.4.0`. EXE `../cdrom/ROADRASH/ROADRASH.EXE` SHA-256 verified
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` pre-call.
Spot-check only per `docs/TOOLING.md`, not runtime proof. No captures run.

| evidence_id | addr | normalized vs `analysis/decompiled/<addr>.c` | verdict |
| --- | --- | --- | --- |
| `analysis/midi-20261016f/rea-433e30-pump.md` | `0x00433e30` stream pump | 1250 / 1250 chars | MATCH |
| `analysis/midi-20261016f/rea-433cf0-teardown.md` | `0x00433cf0` session teardown | 336 / 336 chars | MATCH |

Detail (callback/division/loop bits, unload ordinals): scratch `notes.md`.
Prior full-window counts (pump-tail 1643/1644, teardown 384/385) are window-size
variants of the same verdicts; see `midichain/midipump/miditeardown/sbkloader-rea-20261012.md`.

## Exact addresses (chain + slots)

- Starter/probe/parser: `0x00433bb0` (song/bank select) → `0x00434500` (AWE
  probe: Ordinal_2 checks, RRDRUMS→bank1 + `GRUNGE*.SBK`→bank2 via `0x00434620`,
  fallback generic on failure) → `0x00434230` (RIFF MIDS `fmt `+`data` parse,
  MIDIHDR chain, `0x00434420` compact-track expand).
- Feed/session/pump: `0x00434150` (MDSI block + file-map + parser) →
  `0x004340a0` (teardown-old + alloc + feed + open, 0/1/2 codes) → `0x00433e30`
  (MDSI gate, open-vs-restart, header feed, error teardown; loop bit
  `param_2 & 1`).
- Volume/teardown: `0x00433cb0` (two pct volumes → 0–255 DWORD) →
  `0x00433cf0` (header cleanup + stream teardown + free/null + AWE
  `0xcd/0xcd/0x66` unloads) → `0x00433f70` (midiOutReset + unprepare + close) /
  `0x00433dd0` (close-if-open + buffer free + magic stamp).
- Callers/tables: `0x00435da0` calls `0x00433bb0` with original-course-minus-1;
  `0x004160c0`→`0x00402f50` stops the RRA soundtrack at race start; song tables
  `0x0046d908/0x0046d920/0x0046d938`; SBK state `0x0046d900/0x0046d904`; AWE
  handle `0x00497654`; stream callback `0x00434060`. `0x00433380` is a
  serial-port ctor, NOT audio (sbkloader correction stands).
- Siren/car-engine: siren `0x00435ae0` (event 19, slots 30 `SIREN` / 31
  `SIRENFST`, 0x2b11 Hz dual-buffer, `0x259/0x79` phase, spatial `0x00435840`,
  consumer `0x004351f0` fast path); cars use slots 4/6 `Engcar11`, bikes 3/5
  (`0x004350e0` nearest-two); player slot 0 (`EngRat/Sport/Super`), nitro 1,
  tunnel 2; horn event 18; levels `0x004362e0` 101-entry curve.

## Trigger points + race-start conflict (plan 11 touches)

Plan 11 Task 7 Step 3 owns the race-start hook region (`src/sound.cpp`,
`src/sound.hpp`, `src/hud.cpp`, `src/hud.hpp`, `src/game_main.cpp`,
`tests/sound_test.cpp`, `tests/hud_test.cpp`): "call
`MusicPlayer::play_race(course_index, alias)` at race start and stop it at race
end (`0x004160c0` → `0x00402f50`, `0x00433bb0`)".
Native race-start already touches this point three ways: career
`src/game_main.cpp:356-360` (`music.stop()` + `race_music.play_race(ev.course,
fe.player_name())` + level), attract demo `:341-342`, quick-race `:715-718`
(original-index map `{4,1,2,5,3}`). A plan-13 edit to the same
`start_race` block would collide with the live plan-11 slice.

Hook plan that avoids the conflict: keep ALL MIDI pump work inside the
`sound.cpp` hook region — `SoundSystem::update` (`src/sound.cpp:236-238`)
already forwards to `::rr::pump_race_music()` every race tick even when effect
channels are off, and `set_levels` (`:144-148`) forwards the music channel via
`set_race_music_level`. The live registry (`src/race_music.cpp:753-759`,
`register_race_player`, exactly one player, no `game_main.cpp` change needed)
is the seam: plan 11 keeps owning `game_main.cpp` start/stop calls; plan 13
owns `race_music.*` + the `sound.cpp` pump/level forward only. No new
`game_main.cpp` hook required.

## Bank/program mapping

Per-course (`music.md`, RE notes): CITY→GRUNGE1/SBKGRNG1+GRUNGE1.SBK,
HIWAY→2, MEDLY→3, NAPA→4, CANYN→5; alias `krusty`/`bozo` (6/4-byte prefix,
`0x004573c0`) → `KLOWN.MID` + `GRUNGE5.SBK`; no `KLOWN.SBK`/`SBKGRNG*.SBK` on
disc. All `.MID` are RIFF MIDS (`fmt ` division 480, Klown 120; flags 0, maxbuf
4096); all `.SBK` are SoundFont 1 (`sfbk`, `ifil` 1.0): GRUNGE1 3 presets
(progs 87/30/29), GRUNGE2 2 (36/30), GRUNGE3 3 (27/29/34), GRUNGE4 5
(36/28/30/31/37), GRUNGE5 4 (36/28/29/37), RRDRUMS kit bank128. Non-GM,
bank-local programs; SbkGrng1 ch5 prog 18 and Klown progs 35/56/12/71/11/21
fall back to the native factory table (`race_music.hpp:67-76`); drums on
channel 9 (zero-based) via RRDRUMS. Plan 13 Task 1 Step 2 (AWE
channel/program/controller trace) remains open — proxy forwards Ordinal 2
without MIDI-voice logging.

## Timing source

Original: MIDS `fmt ` division via `MIDIPROP_TIMEDIV` (`0x00433e30`:
`param_1[1]`), tempo from type-1 stream events (usec/qn; e.g. GRUNGE2 521739
vs SBKGRNG2 545455 — do not substitute mixes), loop via pump `param_2 & 1`,
async callback `0x00434060`; starter/probe/parser draw no RNG (randomness in
stream timing). Native: `race_music.cpp:628-635,648-688`
`frames_per_tick = usec/1e6/division*22050`, 512-frame blocks, tempo re-scanned
per refill, `reset_loop` on end; ring backpressure caps advance
(`available() < 2048`, 8 refills).

## Fallback position (plan 00 risk: time-boxed)

Plan 13 Task 1 time box is one session; plan 00 rule: "If a trace overruns its
time box, stop and report rather than guess." Fallback is option C ONLY with
owner approval in chat: play race music from the RRA soundtrack as an explicit
documented deviation. Without that approval the position is the current
behavior: silent no-op on missing/corrupt bank/song (logged once), race
continues (`race_music.cpp:526-531,567-570`; plan 13 review-focus item 1).
Do NOT substitute `GRUNGE*.MID` for `SBKGRNG*.MID`, do NOT bundle AWE ROM data,
do NOT invent factory presets beyond the documented synthesized table.

## NOT-changed list

No tracked file created, modified, staged, or committed. Untouched in
particular: `src/sound.cpp`, `src/sound.hpp`, `src/race_music.*`,
`src/game_main.cpp`, `CMakeLists.txt`, `tests/*`, `docs/REVERSE_ENGINEERING.md`,
`docs/plans/*`, `oracle/*`, prefix/install, Ghidra project, build dirs,
`analysis/decompiled/`, `analysis/oracle/`. Only the two owned outputs above
were written. No builds, no captures, no processes left running, no locks held.

## Remaining work (not done here)

MIDI/siren/car-engine ports (plan 13 Task 1/2 owner scope); plan 13 Task 1
Step 2 AWE voice-message trace (needs proxy MIDI logging or AWE call dump);
Step 5 listening check; plan 11 Task 7 Step 3 hook ownership stays with its
worker. Integrator: file under plan 13 Task 1 background; keep
`analysis/midi-20261016f/` local inclu scratch REA md (disc-derived).
