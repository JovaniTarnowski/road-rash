# Audio and Video Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires plan 01. Tasks 5–6 hook into plans 04–06 via `World::events` and `Frontend`; everything else is standalone.

**Goal:** All original sound and video, natively:
- sound effects with the original's triggers, volumes and pitch (including engine sounds)
- the licensed soundtrack (`.RRA`)
- the optional MIDI soundtrack (`.MID` + `.SBK`)
- all 59 AVI cutscenes and bike showcase clips

**Architecture:**
- **Audio:** an SDL3 audio stream fed by a native mixer (`src/mixer.*`) running at 22,050 Hz stereo, the original's music rate. Effects are resampled from 11,025 Hz.
- **Video:** a native AVI demuxer, Cinepak decoder and IMA ADPCM decoder, all verified frame by frame against FFmpeg's output, which is available locally as a test oracle only and never linked.
- **MIDI:** plays through a SoundFont synthesiser (TinySoundFont, MIT). The `.SBK` banks are SoundFont 1.0 files that reference the Creative AWE32 General MIDI ROM (`irom` = `1MGM`), whose samples are not on the disc. Task 7 handles this with a documented fallback.

**Tech Stack:** C++17, SDL3 audio, TinySoundFont + TinyMidiLoader (single headers, MIT), FFmpeg CLI (tests only).

**Spec:** [00-INDEX.md](00-INDEX.md).

## Known facts

| Asset | Count | Format |
| --- | --- | --- |
| `AUDIO/EFFECTS/*.WAV` | 72 | RIFF PCM, mono, 11,025 Hz, 8-bit unsigned |
| `AUDIO/MUSIC/*.RRA` | 14 | RIFF WAVE PCM, stereo, 22,050 Hz, 8-bit unsigned (renamed WAV) |
| `AUDIO/MUSIC/*.MID` | 11 | Standard MIDI; `GRUNGE1..5`, `SBKGRNG1..5`, `KLOWN` |
| `AUDIO/MUSIC/*.SBK` | 6 | SoundFont 1.0 (`ifil` 1.0, `isng` `EMU8000`, `irom` `1MGM`); `GRUNGE1..5.SBK`, `RRDRUMS.SBK` |
| `VIDEO/*.AVI` + `VIDEO/{RAT,SPORT,SUPER}/*.AVI` | 44 + 15 | Cinepak (`cvid`): 44 cutscenes at 320×192 with IMA ADPCM (`0x11`) stereo 22,050 Hz audio; 15 silent bike showcases at 320×172 |

Relevant imports: `DSOUND!DirectSoundCreate`; `WINMM` midiStream*, mmio* (WAV parsing), `mciSendCommandA` (AVI playback via MCI, "AVI Window" strings at `0x004023e0`/`0x00402a80`, `avivideo` at `0x00402730`/`0x004029f0`); `MSACM32!acmMetrics`; `AWEMAN32` ordinal 2 (SBK loading).

## Global Constraints

- No audio or video data is embedded or bundled. Tests use synthetic buffers or the disc (skip 77 when absent).
- The mixer runs on SDL's audio thread and never locks the simulation. Game events cross over through a lock-free single-producer/single-consumer queue.
- Video decoding is exact: every decoded frame equals FFmpeg's decode bit for bit, compared in the palette/RGB space FFmpeg outputs for the stream.
- The MIDI path is optional and off by default unless a complete GM bank is available (Task 7).

## Review Focus

1. **Audio device loss** (Bluetooth headphones disconnect, Android audio focus loss): the mixer must reopen or pause without crashing. Task 1 tests `SDL_EVENT_AUDIO_DEVICE_REMOVED` handling with a fake device.
2. **Very many simultaneous effects** (a pile-up with grunts, impacts, engines): voice stealing must follow the original's limit and priority. Task 5 pins the rule found in code.
3. **Odd AVI files** (the last chunk truncated, `idx1` missing or wrong): the demuxer must play what is decodable and stop cleanly. Task 3 corrupts a copy of `INTRO.AVI` in a temp dir.
4. **Pausing and resuming the game mid-video or mid-track:** audio and video must stay in sync after resume. Task 4 tests a pause in the middle of `INTRO.AVI`.
5. **Language- or context-specific clips** (`BUSTED1..6`, `LOSE1..10`, `WIN1..6`, `START1..6`, `LEVEL1..6`, `WRECK1..6`): the selection rule must match the original, and a missing clip must not crash. Task 6 covers it.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/wav.hpp/.cpp` | RIFF WAVE parsing (PCM 8/16-bit, IMA ADPCM) |
| `src/mixer.hpp/.cpp` | Voices, resampling, volume/pan, music streaming, SDL stream |
| `src/sound_events.hpp/.cpp` | Map simulation/front-end events to effects (ported trigger rules) |
| `src/avi.hpp/.cpp` | AVI RIFF demuxer |
| `src/cinepak.hpp/.cpp` | Cinepak decoder |
| `src/ima_adpcm.hpp/.cpp` | IMA ADPCM (WAV variant) decoder |
| `src/video_player.hpp/.cpp` | Playback with A/V sync into the framebuffer or an RGB texture |
| `src/midi_music.hpp/.cpp` | Optional MIDI via TinySoundFont, SF1→SF2 conversion |
| `third_party/tsf.h`, `third_party/tml.h` | TinySoundFont, TinyMidiLoader (MIT) |
| Tests | `wav_test`, `mixer_test`, `avi_test`, `cinepak_test` (FFmpeg oracle), `ima_adpcm_test`, `sbk_test` |

---

### Task 1: WAV parsing and the mixer

**Files:**
- Create: `src/wav.hpp`, `src/wav.cpp`, `src/mixer.hpp`, `src/mixer.cpp`, `tests/wav_test.cpp`, `tests/mixer_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct PcmSound { int rate = 0; int channels = 0; std::vector<int16_t> samples; };   // interleaved, converted from 8-bit unsigned: (b - 128) << 8
  PcmSound read_wav(const std::vector<uint8_t>& riff);                                 // PCM 8/16 and IMA ADPCM (format 0x11)
  class Mixer {
  public:
      static constexpr int rate = 22050;
      explicit Mixer(int max_voices);
      int play(const PcmSound*, int volume /*0..127*/, int pan /*-64..63*/, int pitch_q8 /*256 = 1.0*/, bool loop);
      void set(int voice, int volume, int pan, int pitch_q8);
      void stop(int voice);
      void play_music(std::unique_ptr<struct MusicSource>);  // streamed
      void render(int16_t* out, int frames);                 // called from the SDL audio callback
  };
  }
  ```

- [ ] **Step 1: Failing tests**

`wav_test`:
- A synthetic 8-bit mono 11,025 Hz WAV with samples `{0x80, 0xFF, 0x00}` decodes to `{0, 32512, -32768}`.
- A missing `fmt ` or `data` chunk throws.
- An odd-sized chunk is padded correctly.
- A corpus test over all 72 effects and 14 `.RRA` files (skip 77) checks the rates and channels in the Known facts table.

`mixer_test`:
- One voice with a constant sample at volume 127, pan 0, rendered to 100 frames, equals the input in both channels.
- `pitch_q8 = 512` consumes the input twice as fast.
- More than `max_voices` plays steal per the policy documented in Task 5; until then the oldest voice is stolen.
- The output never overflows int16: two full-scale voices clip.

- [ ] **Step 2: Implement**

Use linear interpolation for resampling unless Task 5 finds the original uses DirectSound frequency changes only (then nearest). Wire the mixer into an `SDL_AudioStream` (`SDL_OpenAudioDeviceStream` with a callback).

- [ ] **Step 3: Device loss**

Handle `SDL_EVENT_AUDIO_DEVICE_REMOVED` by reopening the default device. Add a test that calls the handler with a fake removal and checks the mixer state is kept.

- [ ] **Step 4: Pass, commit**

Commit: `audio: WAV decoding and mixer`.

---

### Task 2: Soundtrack (`.RRA`) playback

**Files:**
- Modify: `src/mixer.cpp` (music source streaming from `PcmSound`), `src/sound_events.cpp`

**Interfaces:**
- Produces: `struct MusicSource { virtual int read(int16_t* out, int frames) = 0; virtual ~MusicSource() = default; };` and `std::unique_ptr<MusicSource> open_rra(const Assets&, const std::string& path);`

- [ ] **Step 1:** Trace the music selection: which track plays in menus and in races, per course, shuffle or cycle, and the fades. Start from references to `.RRA` names or directory strings (`grep -n "RRA\|Music" analysis/decompiled/string-references.tsv`), and from `Mixer` (`0x004020a0`/`0x00402370`, which read registry `Mixer`).
- [ ] **Step 2:** Test with a synthetic RRA in a temp dir: streaming returns exactly the PCM in order, then loops or ends per the traced rule.
- [ ] **Step 3:** Implement, commit: `audio: soundtrack playback`.

---

### Task 3: AVI demuxer and codecs

**Files:**
- Create: `src/avi.hpp`, `src/avi.cpp`, `src/cinepak.hpp`, `src/cinepak.cpp`, `src/ima_adpcm.hpp`, `src/ima_adpcm.cpp`
- Create: `tests/avi_test.cpp`, `tests/cinepak_test.cpp`, `tests/ima_adpcm_test.cpp`, `tools/ffmpeg_reference.sh`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct AviStreamInfo { uint32_t fourcc; int width, height; uint32_t rate, scale; int audio_rate = 0, audio_channels = 0, block_align = 0; };
  struct AviChunk { int stream; size_t offset, size; bool keyframe; };
  struct AviFile { std::vector<AviStreamInfo> streams; std::vector<AviChunk> chunks; std::vector<uint8_t> palette; };  // palette if 8-bit
  AviFile read_avi(const std::vector<uint8_t>& file);      // uses idx1 when valid, else scans movi
  class CinepakDecoder { public: CinepakDecoder(int w, int h); void decode(const uint8_t* data, size_t size); const std::vector<uint8_t>& rgb24() const; };
  std::vector<int16_t> decode_ima_adpcm(const uint8_t* data, size_t size, int channels, int block_align);
  }
  ```

- [ ] **Step 1: FFmpeg reference script**

`tools/ffmpeg_reference.sh` writes reference decodes of every video into a temp dir, for tests only:
```sh
#!/bin/sh
# Usage: tools/ffmpeg_reference.sh VIDEO_DIR OUT_DIR
set -eu
mkdir -p "$2"
find "$1" -name '*.AVI' | while read -r f; do
  n=$(echo "${f#$1/}" | tr '/' '_')
  ffmpeg -v error -y -i "$f" -map 0:v -f rawvideo -pix_fmt rgb24 "$2/$n.rgb"
  ffmpeg -v error -y -i "$f" -map 0:a -f s16le "$2/$n.pcm"
done
```

- [ ] **Step 2: Failing tests**

- `ima_adpcm_test`: the standard IMA step/index tables applied to a hand-made one-block stereo stream give known samples. Compute the expected values by hand in the test from the documented algorithm: the predictor and step index come from the block header per channel, then 4-bit nibbles, 8 samples per channel per 4-byte group.
- `cinepak_test`: for every AVI, decoding every frame gives bytes identical to the FFmpeg `.rgb` reference, and decoding the audio gives the `.pcm` reference. Run the script first; skip 77 if FFmpeg or the disc is missing.
- `avi_test`: a synthetic minimal AVI (`hdrl`, one video stream, two `00dc` chunks, `idx1`) parses. A copy of `INTRO.AVI` truncated at 60% still lists the complete chunks before the cut and does not throw.

- [ ] **Step 3: Implement**

- **Cinepak:** strips, V1/V4 codebooks (full and partial updates), intra/inter coding, 12-bit YUV codebook entries → RGB using the same integer conversion FFmpeg uses (`libavcodec/cinepak.c`, conversion in `cinepak_decode_codebook`); verify against the reference. If the streams are 8-bit palettised Cinepak, use the palette from the `strf` chunk.
- **IMA ADPCM:** standard WAV variant.

- [ ] **Step 4: Pass, commit**

Commit: `video: AVI demuxer, Cinepak and IMA ADPCM decoders`.

---

### Task 4: Video player

**Files:**
- Create: `src/video_player.hpp`, `src/video_player.cpp`

**Interfaces:**
- Produces: `class VideoPlayer { public: VideoPlayer(const Assets&, const std::string& path, Mixer&); bool update(uint64_t now_ns); void draw(SDL_Renderer*, SDL_Texture*) const; void pause(bool); bool finished() const; void skip(); };`
  - Frame timing is `scale/rate` from the stream header. Audio drives the clock.

- [ ] **Step 1:** Test with a virtual clock: after pausing at 40% for 5 s and resuming, the next frame shown is frame ⌈0.4·N⌉ (no jump), and audio and video drift stays below one frame.
- [ ] **Step 2:** Trace how the original presents videos: position and scaling of the 320×192 frame on the 640×480 screen (the MCI window rectangle), letterbox colour, and whether a key skips. Implement it.
- [ ] **Step 3:** Commit: `video: synchronised playback`.

---

### Task 5: Sound effect triggers and engine audio

**Files:**
- Create: `src/sound_events.hpp`, `src/sound_events.cpp`
- Modify: `src/world.hpp` (plan 04) if `World::events` does not yet exist:
  `struct GameEvent { uint16_t kind; uint16_t object; int32_t a, b; }; std::vector<GameEvent> events;` (cleared each frame after audio consumes it)

**Interfaces:**
- Consumes: `World::events` (from plans 04/05)
- Produces: `class SoundDirector { public: SoundDirector(const Assets&, Mixer&); void on_frame(const World&, const Camera&); };`, which plays effects for events and keeps engine loops updated.

- [ ] **Step 1: Trace**

Find every DirectSound play call site (`IDirectSoundBuffer::Play`, `SetFrequency`, `SetVolume`, `SetPan` via vtable offsets on the buffer objects) and the functions that decide which WAV to play:
- effect names such as `CRASHHAR`, `PUNCHHEA`, `ENGSPORT`, `BKM1_HEY`
- per-character voice sets (prefixes `BK`, `BS`, `CM`, `AF`, `AM`, …)
- the engine pitch formula from RPM/speed (`ENG*.WAV`, `ENGTUNNE` in tunnels)
- distance attenuation and pan
- the voice limit and priority

Record them as a table in `REVERSE_ENGINEERING.md`.

- [ ] **Step 2: Oracle**

Add an IAT-level hook in the oracle for DirectSound buffer calls (`DSOUND!DirectSoundCreate` returns an object; wrap the vtable) logging `sound.<n>.{buffer,op,value}` per tick in `napa-fight` and `napa-race-full`. Test that `SoundDirector`, fed the native `World` for those scenarios, issues the same sequence of play/frequency/volume/pan operations tick by tick.

- [ ] **Step 3: Implement, pass, commit**

Commit: `audio: original effect triggers and engine sound`.

---

### Task 6: Cutscene and music hooks in the game flow

**Files:**
- Modify: `src/frontend.cpp`, `src/game_main.cpp` (plans 04/06)

- [ ] **Step 1:** Trace which clip plays when:
  - intro
  - `START1..6` / `LEVEL1..6` before races or levels
  - `WIN1..6` / `LOSE1..10` / `WRECK1..6` / `BUSTED1..6` after races
  - `DUEL`, `JESSIE`, `FINALWIN`
  - bike showcase clips in the shop (`VIDEO/RAT|SPORT|SUPER/<bike>.AVI`)

  Record the rule, including any randomness through `Random` and in what order.
- [ ] **Step 2:** Test: for each outcome and level, the chosen clip name equals the traced table. A missing file logs and continues.
- [ ] **Step 3:** Implement, commit: `game: cutscenes and music in the game flow`.

---

### Task 7: Optional MIDI soundtrack

**Files:**
- Create: `src/midi_music.hpp`, `src/midi_music.cpp`, `tests/sbk_test.cpp`
- Add: `third_party/tsf.h`, `third_party/tml.h` (TinySoundFont, MIT) with licence

**Interfaces:**
- Produces: `std::vector<uint8_t> sf1_to_sf2(const std::vector<uint8_t>& sbk, const std::vector<uint8_t>* gm_bank /*nullable*/);` and `std::unique_ptr<MusicSource> open_midi(const Assets&, const std::string& mid, const std::string& sbk, const std::vector<uint8_t>* gm_bank);`

- [ ] **Step 1: Analyse the banks**

Parse all six SBKs (`phdr`/`pbag`/`pgen`/`inst`/`ibag`/`igen`/`shdr`) and list which sample headers point into ROM rather than the file's own `smpl` data. SF1 marks ROM samples by sample type or by offsets beyond the `smpl` size; confirm which. Document it.

- [ ] **Step 2: Decide**

- If every instrument used by the `.MID` files resolves to in-file samples, MIDI can be exact: convert SF1→SF2 (`ifil` 2.1, adapt generator enumerations that differ between 1.0 and 2.x) and play with TinySoundFont.
- Otherwise, ROM-backed instruments need a General MIDI SoundFont supplied by the user (a settings path; never bundled). Without one, MIDI is disabled and the `.RRA` soundtrack is used.

Record the decision in `PROGRESS.md` under deviations.

- [ ] **Step 3: Tests**

- `sbk_test`: the converted SF2 of each SBK parses with `tsf_load_memory`.
- The preset count equals the SF1 `phdr` count − 1 (terminal record).
- A synthetic SF1 with one ROM sample and no GM bank reports that sample as unavailable, not as garbage.

- [ ] **Step 4: Implement, pass, commit**

Commit: `audio: optional MIDI music from SBK banks`.

Implementation update (October 2): PCM WAV/RRA and WAV IMA decoding, queued mixer, AVI physical scanning and RGB Cinepak decoding are implemented. Every audio sample and RGB frame in all 59 AVIs matches FFmpeg in the corpus comparison. The scanner intentionally trusts physical complete packets instead of idx1, so missing or corrupt indexes do not prevent sequential playback. A truncated INTRO retains complete packets. Runtime video playback is being integrated; original presentation rectangle, cutscene selection, streamed soundtrack, original sound triggers/voice priorities and optional MIDI remain open.
