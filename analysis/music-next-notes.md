# Music remainder deltas (plan 13, 2026-10-12)

Prior slice: `fcaf29b` (MIDS parser + SF1 banks + race hooks, parse-only player).
This slice remainder per `docs/research/post-11a-2026-10-12/music.md` +
`options-and-hud.md` 59-video audit + README row 13.

## What changed
- Factory presets (`src/race_music.*`): synthesized `factory_bank()` for census
  missing melodic programs {11,12,18,21,33,35,56,71}; `render_race_song`
  defaults to factory (pass nullptr for hardware-silence audit);
  `count_factory_notes()` counts disc-unmapped note-ons.
- Corpus audit (`tests/music_test.cpp`): all 11 MID files parse; Klown
  division 120/112 tempos; per-AWE-song factory counts: SBKGRNG1 190,
  SBKGRNG2 0, SBKGRNG3 1, SBKGRNG4 0, SBKGRNG5 14, KLOWN 2850.
- Mixer streaming ring (`src/mixer.*`): new `MusicStream` bounded SPSC stereo
  ring (reserve-once, lock-free pull, silence on underrun, stop() drains +
  bumps generation); `Mixer::play_stream` voice mixed centre-panned in
  `render()`; retire/consume/stop handle stream voices.
- Live sequencer (`RaceMusicPlayer::update/test_play/position_frames/
  set_music_level`): game-thread 512-frame blocks into the ring, tempo map,
  program tracking, factory fallback, song loop (0x00433e30), pause holds,
  stop = mixer stop + ring drain + generation handshake.
- Sound hook region only (`src/sound.*`): `set_race_music(void*)`,
  `set_race_music_level()`, music channel forward in `set_levels`,
  `pump_race_music()` once per `SoundSystem::update` via
  `register_race_player/pump_race_music/set_race_music_level` registry.
  No `game_main.cpp` changes (live-session owned).
- Jukebox triggers: `jukebox_video_path/count` = VIDEO/JESSIE.AVI,
  VIDEO/DUEL.AVI (table 0x00473f98); UI wiring stays with frontend owner.

## Evidence
- `build/music-test` exit 0 (incl. corpus + streaming + live + jukebox).
- `build/mixer-test` 0 failures; `build/sound-test` 0 failures (tail output;
  exit code not separately captured — main agent should rerun under gate).
- SBKGRNG4 render 3758090 frames audible; play_race(4) streams audible.

## Open questions / deviations
- Factory timbres are plain synthesized markers, NOT AWE ROM reproductions;
  generator-48 cB scaling still `10^(-cb/200)` guess (prior open Q1).
- Klown merged-bank + factory is best-effort; SBKGRNG1's 190 factory notes
  are key-range (not program) misses — parser keeps strict key ranges.
- `update()` voices_ grows on note-on (bounded by polyphony); no listening
  check performed. `sound-test` exit code + full ctest gate left to Main.
