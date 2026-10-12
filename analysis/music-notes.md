# Race music notes (plan 13 slice, 2026-10-12)

Spec: `tools/research_after_11a.py` (`mids()` + `census()` music tables) over the
unmodified disc EXE (SHA-256 `73d7f97…a60f18a2`); song/bank selection `0x00433bb0`
in `analysis/decompiled/00433bb0.c`. The post-11a music.md brief itself is absent
from this checkout (`docs/research/post-11a-2026-10-12/` holds only the README and
simulation notes), so this slice implements directly from the census script plus
the decompile. Deviations from plan 13's original assumptions are marked (P13).

## Format corrections vs plan 13 (P13)

- Race songs are RIFF `MIDS`, not Standard MIDI Files. `read_midi` (SMF) was never
  built; `read_mids()` parses `MIDS/fmt ` (division, max_buffer, flags) and
  `MIDS/data` (block count, then per-block timestamp/size + stride events).
  Stride is 12 bytes here (flags 0); flags&1 selects the 8-byte form.
- Event packing: `delta u32 | middle u32 | event u32`; short events have
  `kind = (event>>24) & 191` with kind 0 = channel voice
  (`status = event&0xff`), kind 1 = tempo (`event&0xffffff` usec/qn).
  Long events set bit 31 with a 24-bit length, 4-byte padded. All 11 corpus
  songs use only kinds 0/1 and no long events.
- AWE songs are `SBKGRNG1..5.MID` (always used; no GM fallback on modern
  hardware), generic mixes `GRUNGE1..5.MID`, override song `KLOWN.MID`.
  `GRUNGE*.MID` are parsed by the same reader but never played.
- Klown is a song, not a bank: no `KLOWN.SBK` exists on disc. It was written
  against AWE factory ROM presets; the player merges all five GRUNGE banks and
  unmapped notes stay silent.
- `.SBK` is SoundFont 1 (`sfbk`, `ifil` 1.0, `isng = EMU8000`) but NOT SF2
  layout: sample headers are a compact 16 bytes (start, end, loopStart,
  loopEnd) with no name/rate/pitch; the census `shdr` u32 quads confirm.
  Sample rate is fixed 22050 Hz; root pitch comes from generator 58
  (default 60). Key ranges pack as generator 43 = low+(high<<8) (single op,
  not 43/44 split). Banks map one preset per bank file; drums live in
  `RRDRUMS.SBK` preset 0 bank 128 (19 zones, all with keyRange+sampleID).
- Attenuation: generator 48 values up to 127 with `note_gain = vel * 10^(-cb/200)`;
  corpus zones carry att 47..127 yet render audibly (gain ~4e-4..-3dB after the
  velocity factor). The exact AWE cB scaling is unconfirmed; treat as open.

## Selection rule (0x00433bb0)

- `SbkGrng<N>` by original course 1..5 (table order CITY/HIWAY/MEDLY/NAPA/CANYN);
  `music_awe` table `0x0046d920`. Klown when the alias matches `krusty` (6 bytes)
  or `bozo` (4 bytes), case-insensitive prefix.
- game_main hook: plan 11 race-start point (`music.stop()` at `0x004160c0→0x00402f50`)
  calls `race_music.play_race(ev.course, fe.player_name())`; race end calls
  `race_music.stop()`. `ev.course` is already the original 1..5 index.
- RRA soundtrack path untouched (`MusicPlayer::play/stop/track` as before).

## Validation

- `music-test`: synthetic MIDS (event ticks/status) + every-truncation-throws +
  garbage/wrong-tag sweeps; synthetic SF1 (zone/program/samples) + truncation +
  garbage; offline render audible for synthetic and corpus SBKGRNG4 (3,758,090
  frames); `play_race(4)` loads SBKGRNG4 with the parser's event count;
  krusty→KLOWN; missing-assets root is a silent no-op.
- Corpus (disc present): 6 AWE/Klown songs parse (6293/6186/9238/6948/8366/9265
  events; KLOWN division 120, 112 tempos); 5 melodic banks load (3/3/3/5/5
  zones); drum kit 19 zones; first-200-song-note mapping check 200/200.

## Open questions

1. Generator 48 (initialAttenuation) scaling on the EMU8000: linear-shift cB or
   96 dB curve? Current `10^(-cb/200)` is a guess that sounds; needs an AWE
   recording to pin down.
2. `SbkGrng` vs `Grunge` mix choice (`0x00434500` AWE detection): we always play
   the AWE mix. Whether the game falls back to `Grunge` without an AWE32 is
   unconfirmed from the decompile (both tables are referenced).
3. Live audio path: the player currently loads/parses only (`loaded_` state);
   game-thread sequencing into `Mixer` (SPSC queue, music level curve
   `0x004362e0`) is the remaining work, plus pause/resume-in-place.
4. `max_buffer` violations and `flags&~1` are rejected; no corpus file exercises
   the 8-byte stride form.
