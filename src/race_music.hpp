#pragma once
// Race music: RIFF MIDS songs + EMU SoundFont 1 banks (AWE32 path).
//
// Corrected spec (post plan 13): the race songs are RIFF `MIDS` files, not
// Standard MIDI Files. `GRUNGE1..5.MID` are the generic (non-AWE) mixes and
// `SBKGRNG1..5.MID` are the AWE mixes; `KLOWN.MID` overrides by player alias.
// Melodic banks are `GRUNGE1..5.SBK` (SoundFont 1, `ifil` 1.0) with
// bank-specific (non-GM) programs; `RRDRUMS.SBK` holds the channel-10 kit
// (preset 0 bank 128). Some notes fall on unmapped (program,key) zones;
// those notes are silent, exactly as on hardware missing factory presets.
//   // 0x00433bb0 (song/bank selection), 0x00434500 (AWE detection).
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rr {
class Assets;
class Mixer;

// A single channel voice event with an absolute tick.
struct MidsEvent {
    uint32_t tick = 0;  // accumulated delta ticks, division units
    uint8_t status = 0; // 0x80..0xEF channel voice status
    uint8_t data0 = 0;
    uint8_t data1 = 0;
};

struct MidsSong {
    uint32_t division = 480;       // ticks per quarter note (MIDS `fmt ` field 0)
    std::vector<MidsEvent> events; // tick order, all blocks concatenated
    // (tick, microseconds per quarter note); kind==1 tempo events.
    std::vector<std::pair<uint32_t, uint32_t>> tempo;
    uint32_t total_ticks = 0;
};

// Bounds-checked RIFF MIDS parser. Throws std::runtime_error on malformed,
// truncated or garbage input.
MidsSong read_mids(const std::vector<uint8_t>& bytes);

// One playable (program,key) sample zone of a SoundFont 1 bank.
struct Sf1Zone {
    uint8_t program = 0; // melodic preset program (bank 0), or 0 for drums
    uint8_t bank = 0;    // 0 melodic, 128 drums
    uint8_t low_key = 0, high_key = 127;
    uint8_t root_key = 60;
    int8_t coarse_tune = 0;
    int8_t fine_tune = 0;                      // semitones + cents/100
    int16_t volume_db_cb = 0;                  // initial attenuation, centibels (negative ok)
    uint32_t sample_start = 0, sample_end = 0; // sample indices into samples
    uint32_t loop_start = 0, loop_end = 0;     // sample indices; loop_end<=start = no loop
    uint16_t sample_rate = 22050;
};

struct Sf1Bank {
    std::vector<int16_t> samples; // 16-bit mono PCM from `sdta/smpl`
    std::vector<Sf1Zone> zones;
};

// Bounds-checked SoundFont 1 (`sfbk`, `ifil` major 1) parser. Only the zone
// fields the renderer consumes are extracted; unknown generator/modulator
// operators are skipped. Throws std::runtime_error on malformed input.
// `melodic=true` keeps bank-0 presets, `melodic=false` keeps the bank-128 kit.
std::shared_ptr<const Sf1Bank> read_sf1(const std::vector<uint8_t>& bytes, bool melodic);

// Factory presets for notes the disc banks cannot play (research music.md:
// "The disc banks are not a complete GM set"). SbkGrng1 ch5 sounds program
// 18, absent from GRUNGE1.SBK; Klown sounds programs 35/56/12/71/11/21
// against GRUNGE5.SBK (drums: channel 9 via RRDRUMS); GRUNGE1 ch1 also
// sounds program 33, absent from its GRUNGE1.SBK. The original AWE32 fell
// back to its factory ROM for these; the native bank is a small synthesized
// GM-timbre set (no disc bytes), used only when no disc zone covers the
// (program,key). `factory_covers(p)` reports the covered melodic programs.
const Sf1Bank& factory_bank();
bool factory_covers(uint8_t program);
// Count note-on events in `song` whose (program,key) is covered by neither
// `melodic`/`drums` nor the factory table (`factory_prog_key` lists the
// census-observed soundings as (bank,program) for the report).
size_t count_factory_notes(const MidsSong& song, const Sf1Bank& melodic, const Sf1Bank& drums);

// Offline render of a parsed song against melodic + drum banks (no audio
// hardware). Returns true when at least one sample was rendered. Unmapped
// notes fall back to `factory` (default: the factory table, so SbkGrng/Klown
// corpus renders are audible like the original AWE path); pass nullptr
// explicitly for the exact hardware-silence audit.
bool render_race_song(const MidsSong&, const Sf1Bank&, const Sf1Bank&, std::vector<int16_t>& mono,
                      const Sf1Bank* factory = &factory_bank());
// Live-registry pump for the sound.cpp hook region: SoundSystem::update
// (already called once per race tick) forwards here so the sequencer runs
// without touching game_main.cpp, which the live session owns.
void pump_race_music();
void set_race_music_level(int level_0_100);
class RaceMusicPlayer;
void register_race_player(RaceMusicPlayer* player);

// Song/bank selection (0x00433bb0): Grunge1..5 by original course 1..5 using
// the SbkGrng AWE mix; the Klown song when the player alias is "krusty" or
// "bozo" (comparison is the original 6/4-byte prefix match).
std::string race_song_name(int original_course_index, const std::string& alias);
bool race_song_is_klown(const std::string& alias);

class RaceMusicPlayer {
  public:
    RaceMusicPlayer(const Assets&, Mixer&);
    ~RaceMusicPlayer();
    // 0x00433bb0: load song+bank for the course; silent no-op when assets are
    // absent or unreadable. Never throws.
    void play_race(int original_course_index, const std::string& alias);
    void stop();
    void pause(bool);
    void update(); // game thread: render the next block into the mixer ring
    bool active() const { return playing_; }
    std::string song() const { return song_; }
    // Test hook: parse + render without audio hardware.
    bool loaded() const { return loaded_; }
    size_t event_count() const { return song_data_.events.size(); }
    // Live-sequencer hooks: buffered frame position, music level (0..100,
    // 0x004362e0 curve channel, default 80), and a synthetic-song entry
    // point for tests (bypasses asset loading, same render path).
    uint64_t position_frames() const { return position_frames_; }
    void set_music_level(int level) { music_level_ = level < 0 ? 0 : level > 100 ? 100 : level; }
    void test_play(const MidsSong& song, std::shared_ptr<const Sf1Bank> melodic,
                   std::shared_ptr<const Sf1Bank> drums);

  private:
    const Assets& assets_;
    Mixer& mixer_;
    bool playing_ = false, paused_ = false, loaded_ = false;
    std::string song_;
    MidsSong song_data_;
    std::shared_ptr<const Sf1Bank> melodic_, drums_;
    class MusicStream* stream_ = nullptr; // owned; mixer only borrows during play
    int stream_voice_ = -1;
    uint64_t position_frames_ = 0;
    uint32_t event_cursor_ = 0;
    uint32_t tick_ = 0;
    double tick_frac_ = 0.0;
    int music_level_ = 80; // 0x004362e0 music channel default
    struct LiveVoice {
        const Sf1Zone* zone = nullptr;
        double pos = 0.0, step = 1.0;
        float gain = 0.0f;
        uint32_t off_frame = 0;
        bool has_off = false;
        uint8_t channel = 0, key = 0;
    };
    std::vector<LiveVoice> voices_;
    uint8_t live_program_[16] = {};
    std::map<std::pair<uint8_t, uint8_t>, size_t> live_on_;
};
// Jukebox music videos (options-and-hud.md 59-video audit): table 0x00473f98
// holds JESSIE/DUEL, shown by the jukebox popup 0x00445730.
std::string jukebox_video_path(int slot);
int jukebox_video_count();
} // namespace rr
