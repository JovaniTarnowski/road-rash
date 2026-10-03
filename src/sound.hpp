#pragma once
#include "mixer.hpp"
#include "sound_event.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rr {
class Assets;
class Race;

// The Mixer block (registry "Mixer", 0x00463030): levels 0..100 and mutes, applied through the
// 101-entry volume curve built by 0x004362e0.
struct SoundLevels {
    int master = 100, interface = 80, engine = 80, effects = 80, music = 80, others = 80, siren = 80;
    bool mute = false, swap_speakers = false;
};
std::array<int32_t, 101> read_volume_curve(const std::vector<uint8_t>& exe);   // parsed from 0x004362e0's code

// DirectSound conversions (0x004141c0 / 0x004142c0): volume 0..16384 -> hundredths of dB
// (linear in dB, -10000..0, silent below -9000), pan in hundredths of dB, frequency 100..100000 Hz.
int32_t ds_attenuation(int32_t volume);
int mixer_volume(int32_t attenuation);            // 0..127 amplitude
int mixer_pan(int32_t pan_hundredths, bool swap);  // -64..63
int pitch_q8(int32_t frequency, int source_rate);

// Effects and engine sounds of a race: the 0x45 effect slots of 0x00435e60, one DirectSound
// buffer each (a retrigger restarts that slot only), the per-tick engine and nearest-vehicle
// update of 0x00434980 and the event consumer 0x004351f0.
class SoundSystem {
public:
    // skid_limit: byte 0x0046dce5 (surface classes counted as road for the skid sound).
    SoundSystem(const Assets&, Mixer&, std::array<int32_t, 101> curve, int skid_limit);
    ~SoundSystem();
    void set_levels(const SoundLevels&);
    // 0x00435da0: load slot 0 for the bike, pick male/female grunts for the character.
    void start_race(int bike, int character);
    void stop_race();
    void post(const SoundEventRecord&);           // 0x00436830: at most 10 pending
    void update(const Race&);                     // once per simulation tick
    static const char* effect_name(int slot);     // AUDIO/EFFECTS/<name>.WAV
private:
    struct Slot { PcmSound pcm; int voice = -1; bool looping = false; };
    void play_once(int slot, int32_t volume, int32_t pan, int32_t frequency);   // 0x004142c0
    void play_loop(int slot, int32_t frequency, int32_t volume, int32_t pan);   // 0x004141c0
    void stop(int slot);                                                         // 0x00414160
    bool playing(int slot) const;
    void distance(int32_t position, int32_t lateral, int32_t speed, int32_t& doppler, int32_t& volume, int32_t& pan) const;
    uint32_t random();
    const Assets& assets_;
    Mixer& mixer_;
    std::array<int32_t, 101> curve_;
    std::array<Slot, 0x45> slots_;
    std::vector<SoundEventRecord> queue_;
    int32_t effects_ = 80, engine_ = 80, others_ = 80, siren_ = 80;
    bool effects_off_ = false, engine_off_ = false, others_off_ = false, swap_ = false;
    int grunt_offset_ = 0;           // 0x00497694: 3 for Cydney, Pearl and Rhonda
    bool nitro_ = false, tunnel_ = false, finish_yell_ = false, sliding_ = false;
    int32_t slide_volume_ = 0;
    int32_t listener_position_ = 0, listener_lateral_ = 0, listener_speed_ = 0;
    std::array<int32_t, 8> rpm_{}, rpm_speed_{};
    bool racing_ = false;
    int skid_limit_ = 2;
    uint32_t seed_ = 0x1234;
};

// The licensed soundtrack (0x00403040): AUDIO/MUSIC/<track>.RRA from the saved track index,
// advancing 0..13 when a track ends. Stopped during races (0x00402f50).
class MusicPlayer {
public:
    MusicPlayer(const Assets&, Mixer&);
    ~MusicPlayer();
    static const char* track_name(int index);    // 14 tracks
    void play(int track);
    void stop();
    void set_volume(int volume_127);
    void update();                               // advance when the track has ended
    int track() const { return track_; }
    bool active() const { return voice_ >= 0; }
private:
    const Assets& assets_;
    Mixer& mixer_;
    std::unique_ptr<PcmSound> pcm_;
    int voice_ = -1, track_ = 0, volume_ = 127;
};
}
