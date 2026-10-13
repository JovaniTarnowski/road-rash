#include "sound.hpp"
#include "assets.hpp"
#include "exe_tables.hpp"
#include "race.hpp"
#include "race_music.hpp"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace rr {
namespace {
// 0x00435e60: slot 1..0x44 file names (slot 0 is the player's engine, chosen per bike).
const char* const effect_names[0x45] = {
    "EngRat", "EngNitro", "EngTunne", "EngOther", "Engcar11", "EngOther", "Engcar11", "M_YEAH", "F_YEAH",
    "CHAINIMP", "CLUBIMPA", "BOUNCE", "CRASHHAR", "CRASHMED", "CRASHLIT", "GRUNTM1_", "GRUNTM2_", "GRUNTM3_",
    "GRUNTF1_", "GRUNTF2_", "GRUNTF3_", "KICK_MW4", "PUNCHHEA", "SWISHCHA", "SWISHGEN", "SKIDONNR", "SKIDOFFR",
    "FOOTSTEP", "RIDERSLI", "BMWLONG", "SIREN", "SIRENFST", "SK1_EATT", "BKM1_HEY", "BSW3_HEY", "FG2_EEK",
    "FG1_WATC", "BKM4_PTY", "SK1_DUDE", "TOUR_BAC", "DM_USOB", "BSM1_TAK", "BKW3_STP", "CM3_ARRG", "BKW1_DON",
    "HF_HELP", "DM_HOLYS", "OL_AAA", "AF1_EEK", "BKM1_HUH", "AM1_DOOO", "BSW1_NO", "CW_BACK", "BSW1_HUH",
    "BKM4_WAI", "JF_NO", "FCYC_WAT", "TOUR_AAA", "BKM2_HLD", "BKM4_RUM", "FM_SLOWD", "BSM1_WAT", "BSM3_WHA",
    "BSM2_WOA", "BSW2_AAA", "JM_AIIE", "BKW2_BIT", "CM1_HEYM", "OL_MAKE"};
// 0x00435e60 local table: engine by bike id 0..14 (ids 0-4 EngRat, 5-9 EngSport, 10-14 EngSuper).
const char* engine_for(int bike) { return bike < 5 ? "EngRat" : bike < 10 ? "EngSport" : "EngSuper"; }
// 0x00403040.
const char* const tracks[14] = {"HB_Trip", "HB_Pass", "MM_Dino", "P_Pansy", "P_Jessie", "P_Bridge", "SG_Rusty",
                                "SG_Outsh", "SG_Kicks", "SG_Super", "SD_Train", "SD_Duel", "T_Teeth", "T_Auto"};
constexpr int32_t hearing = 0x1e00;   // 0x00434db0 / 0x00435840: 30 samples

std::string upper(const std::string& s) {
    std::string u;
    for (char c : s) u.push_back(char(std::toupper(static_cast<unsigned char>(c))));
    return u;
}

// 0x0044f5a0: octant angle, 256 per turn.
int32_t angle(int32_t dx, int32_t dy) {
    if (dx == 0 && dy == 0) return 0;
    const int32_t ax = std::abs(dx), ay = std::abs(dy);
    int k = (dx < 0) | (dy < 0 ? 2 : 0) | (ax < ay ? 4 : 0);
    switch (k) {
    case 0: return ay * 0x20 / ax;
    case 1: return 0x80 - ay * 0x20 / ax;
    case 2: return -(ay * 0x20 / ax);
    case 3: return ay * 0x20 / ax - 0x80;
    case 4: return 0x40 - ax * 0x20 / ay;
    case 5: return ax * 0x20 / ay + 0x40;
    case 6: return ax * 0x20 / ay - 0x40;
    default: return -0x40 - ax * 0x20 / ay;
    }
}
}

std::array<int32_t, 101> read_volume_curve(const std::vector<uint8_t>& exe) {
    // 0x004362e0 builds a 101-entry stack table: "sub esp, 0x194", "xor ecx, ecx", stores of ecx (0)
    // and of immediates to [esp+disp], interleaved with register pushes that shift esp. Decode that
    // straight-line prologue; table entry k is at the original esp + 4k.
    const auto code = read_exe_data(exe, 0x004362e0, 0x400);
    auto le32 = [&](size_t p) { return int32_t(uint32_t(code[p]) | uint32_t(code[p + 1]) << 8 | uint32_t(code[p + 2]) << 16 | uint32_t(code[p + 3]) << 24); };
    std::array<int32_t, 101> curve{};
    std::array<bool, 101> seen{};
    int32_t pushed = 0;
    std::array<int32_t, 2> reg{};   // eax, ecx
    bool frame = false;
    auto store = [&](int32_t disp, int32_t value) {
        const int32_t offset = disp - pushed;
        if (offset < 0 || offset % 4 || offset / 4 > 100 || value == INT32_MIN) throw std::runtime_error("Unexpected volume table store");
        curve[size_t(offset / 4)] = value;
        seen[size_t(offset / 4)] = true;
    };
    for (size_t p = 0; p + 11 <= code.size() && !std::all_of(seen.begin(), seen.end(), [](bool b) { return b; });) {
        const uint8_t op = code[p], modrm = code[p + 1];
        const int r = (modrm >> 3) & 7;
        if (op == 0x81 && modrm == 0xec) { frame = le32(p + 2) == 0x194; p += 6; }
        else if (op == 0x33 && (modrm == 0xc0 || modrm == 0xc9)) { reg[modrm == 0xc9] = 0; p += 2; }
        else if (op == 0xb8 || op == 0xb9) { reg[op == 0xb9] = le32(p + 1); p += 5; }
        else if (op == 0x53 || op == 0x55 || op == 0x56 || op == 0x57) { pushed += 4; p += 1; }
        else if (op == 0xbe || op == 0xbf || op == 0xba || op == 0xbb) p += 5;                 // mov esi/edi/edx/ebx, imm
        else if (op == 0x2b && (modrm & 0xc0) == 0xc0) p += 2;                                  // sub reg, reg
        else if (op == 0xa1) { reg[0] = INT32_MIN; p += 5; }                                    // mov eax, [mem]
        else if (op == 0x8b && (modrm & 0xc7) == 0x05) { if (r < 2) reg[size_t(r)] = INT32_MIN; p += 6; }   // mov reg, [mem]
        else if (op == 0x89 && r < 2 && code[p + 2] == 0x24 && (modrm & 0xc7) == 0x44) { store(code[p + 3], reg[size_t(r)]); p += 4; }
        else if (op == 0x89 && r < 2 && code[p + 2] == 0x24 && (modrm & 0xc7) == 0x84) { store(le32(p + 3), reg[size_t(r)]); p += 7; }
        else if (op == 0x89 && r < 2 && code[p + 2] == 0x24 && (modrm & 0xc7) == 0x04) { store(0, reg[size_t(r)]); p += 3; }
        else if (op == 0xc7 && modrm == 0x44 && code[p + 2] == 0x24) { store(code[p + 3], le32(p + 4)); p += 8; }
        else if (op == 0xc7 && modrm == 0x84 && code[p + 2] == 0x24) { store(le32(p + 3), le32(p + 7)); p += 11; }
        else { if (std::getenv("RR_DEBUG_CURVE")) std::fprintf(stderr, "curve decode stopped at +%zx op %02x\n", p, op); break; }
        if (!frame) throw std::runtime_error("Volume curve prologue not found");
    }
    if (std::getenv("RR_DEBUG_CURVE")) for (size_t k = 0; k < seen.size(); ++k) if (!seen[k]) std::fprintf(stderr, "missing %zu\n", k);
    if (!std::all_of(seen.begin(), seen.end(), [](bool b) { return b; })) throw std::runtime_error("Volume curve not found");
    for (size_t k = 1; k < curve.size(); ++k)
        if (curve[k] < curve[k - 1] || curve[k] > 100) throw std::runtime_error("Unexpected volume curve");
    return curve;
}

int32_t ds_attenuation(int32_t volume) {
    int32_t a = int32_t((int64_t(volume) * 10000) / 16384) - 10000;   // (v*10000 + (v*10000>>31 & 0x3fff)) >> 14
    return std::clamp(a, -10000, 0);
}

int mixer_volume(int32_t attenuation) {
    return int(std::lround(127.0 * std::pow(10.0, double(attenuation) / 2000.0)));
}

int mixer_pan(int32_t pan, bool swap) {
    pan = std::clamp(swap ? -pan : pan, -10000, 10000);
    // DirectSound attenuates the opposite channel by |pan| hundredths of dB; the mixer scales it linearly.
    const double g = std::pow(10.0, -double(std::abs(pan)) / 2000.0);
    const int linear = int(std::lround((1.0 - g) * 63.0));
    return pan > 0 ? linear : -linear;
}

int pitch_q8(int32_t frequency, int source_rate) {
    frequency = std::clamp(frequency, 100, 100000);
    return std::clamp(int((int64_t(frequency) * 256 + source_rate / 2) / std::max(1, source_rate)), 1, 4096);
}

SoundSystem::SoundSystem(const Assets& assets, Mixer& mixer, std::array<int32_t, 101> curve, int skid_limit)
    : assets_(assets), mixer_(mixer), curve_(curve), skid_limit_(skid_limit) {
    for (int i = 1; i < 0x45; ++i) {
        const std::string path = "AUDIO/EFFECTS/" + upper(effect_names[i]) + ".WAV";
        if (assets_.exists(path)) slots_[size_t(i)].pcm = read_wav(assets_.read(path));
    }
    set_levels({});
}

SoundSystem::~SoundSystem() {
    for (auto& s : slots_) { if (s.voice >= 0) mixer_.stop(s.voice); mixer_.retire(&s.pcm); }
}

const char* SoundSystem::effect_name(int slot) { return slot >= 0 && slot < 0x45 ? effect_names[slot] : ""; }

void SoundSystem::set_levels(const SoundLevels& l) {
    // 0x004362e0: curve[level * master / 100]; a zero level or mute disables the channel.
    auto level = [&](int v) { return curve_[size_t(std::clamp(v * std::clamp(l.master, 0, 100) / 100, 0, 100))]; };
    effects_ = level(l.effects); engine_ = level(l.engine); others_ = level(l.others); siren_ = level(l.siren);
    effects_off_ = l.master == 0 || l.effects == 0 || l.mute;
    engine_off_ = l.master == 0 || l.engine == 0 || l.mute;
    others_off_ = l.master == 0 || l.others == 0 || l.mute;
    swap_ = l.swap_speakers;
    // Plan 13 remainder hook region: forward the music channel to the
    // registered race player (pumped in update()).
    race_music_level_ = std::clamp(l.music, 0, 100);
    if (race_music_) set_race_music_level(race_music_level_);
}

void SoundSystem::set_race_music(void* player) { race_music_ = player; }
void SoundSystem::set_race_music_level(int level_0_100) {
    race_music_level_ = std::clamp(level_0_100, 0, 100);
    if (race_music_) ::rr::set_race_music_level(race_music_level_);
}
uint32_t SoundSystem::random() {
    seed_ = seed_ * 214013u + 2531011u;
    return (seed_ >> 16) & 0x7fff;
}

bool SoundSystem::playing(int slot) const { return slots_[size_t(slot)].voice >= 0 && mixer_.active(slots_[size_t(slot)].voice); }

void SoundSystem::play_once(int slot, int32_t volume, int32_t pan, int32_t frequency) {
    // 0x004142c0: restarts the slot's buffer (a still-playing one-shot is not interrupted).
    Slot& s = slots_[size_t(slot)];
    if (s.pcm.samples.empty() || playing(slot)) return;
    const int v = mixer_volume(ds_attenuation(volume));
    s.voice = mixer_.play(&s.pcm, v, mixer_pan(pan, swap_), pitch_q8(frequency, s.pcm.rate), false);
    s.looping = false;
}

void SoundSystem::play_loop(int slot, int32_t frequency, int32_t volume, int32_t pan) {
    // 0x004141c0: update a looping buffer; below -90 dB it stops.
    Slot& s = slots_[size_t(slot)];
    if (s.pcm.samples.empty()) return;
    const int32_t a = ds_attenuation(volume);
    if (a < -8999) { if (s.looping) stop(slot); return; }
    const int v = mixer_volume(a), p = mixer_pan(pan, swap_), q = pitch_q8(frequency, s.pcm.rate);
    if (s.looping && s.voice >= 0 && mixer_.active(s.voice)) mixer_.set(s.voice, v, p, q);
    else { s.voice = mixer_.play(&s.pcm, v, p, q, true); s.looping = true; }
}

void SoundSystem::stop(int slot) {
    Slot& s = slots_[size_t(slot)];
    if (s.voice >= 0) mixer_.stop(s.voice);
    s.voice = -1;
    s.looping = false;
}

void SoundSystem::start_race(int bike, int character) {
    stop_race();
    // 0x00435e60 slot 0.
    mixer_.retire(&slots_[0].pcm);
    slots_[0].pcm = {};
    const std::string path = "AUDIO/EFFECTS/" + upper(engine_for(bike)) + ".WAV";
    if (assets_.exists(path)) slots_[0].pcm = read_wav(assets_.read(path));
    grunt_offset_ = (character == 3 || character == 5 || character == 6) ? 3 : 0;   // 0x00435da0
    queue_.clear();
    nitro_ = tunnel_ = finish_yell_ = sliding_ = false;
    listener_position_ = 0x3100; listener_lateral_ = 0; listener_speed_ = 0;     // 0x0044b250
    racing_ = true;
}

void SoundSystem::stop_race() {
    for (int i = 0; i < 0x45; ++i) stop(i);
    queue_.clear();
    racing_ = false;
}

void SoundSystem::post(const SoundEventRecord& e) {
    if (queue_.size() < 10) queue_.push_back(e);   // 0x00436830
}

void SoundSystem::distance(int32_t position, int32_t lateral, int32_t speed, int32_t& doppler, int32_t& volume, int32_t& pan) const {
    // 0x00434db0 / 0x00435840: chebyshev-like distance from the camera, Doppler from the
    // relative speed, pan from the bearing (angle * 0x62 hundredths of dB).
    const int32_t dp = position - listener_position_;
    const int32_t ap = std::abs(dp);
    doppler = 0x100; volume = 0; pan = 0;
    if (ap >= hearing) return;
    const int32_t dl = lateral - listener_lateral_;
    const int32_t al = std::abs(dl);
    const int32_t d = ap - std::min(ap, al) / 2 + al;
    if (d >= hearing) return;
    int32_t v = (hearing - d) * 8 / 256;
    const int32_t rel = dp < 0 ? speed - listener_speed_ : listener_speed_ - speed;
    doppler = int32_t(0x13b0000LL / int64_t(0x13b00 - rel));
    int32_t a = angle(dp, dl);
    if (a >= 0xc1) a -= 0x100;
    else if (a > 0x40) a = 0x80 - a;
    if (a < -0x40) a = -0x80 - a;
    pan = a * 0x62;
    volume = v * v / 256;
}

void SoundSystem::update(Race& race) {
    // Plan 13 remainder hook region: pump the registered race sequencer even
    // when the effect channels are off (music has its own channel/mute).
    if (race_music_) ::rr::pump_race_music();
    auto& riders = race.riders();
    Bike& player = riders[0].bike;
    // 0x00434980 refreshes the player's rpm in both of its branches
    // (0x00434ce0 or a direct 0x004349c0), so the dashboard needle moves
    // with or without engine sound; the race tick calls it when there is no
    // SoundSystem at all.
    update_engine_rpm(player);
    if (!racing_) return;
    // Listener: the camera (0x00437c30).
    const int32_t camera_position = int32_t(race.camera_position());
    listener_speed_ = (camera_position - listener_position_) * 0x100;
    listener_position_ = camera_position;   // 0x00437c30: refresh before distance attenuation
    listener_lateral_ = race.camera_x();
    tunnel_ = false;   // sub-slice terrain type 3 (0x00434a80): type 2/3 assignment (0x00454240) not ported
    // 0x004349c0: the engine rpm lives on the bike (+0x2dc), so the
    // dashboard needle and the engine pitch read the same field
    // (0x00434ce0/0x00434fd0); the player's was refreshed above.
    // 0x00434ce0: the player's engine (nitro and tunnel variants replace it).
    if (!engine_off_) {
        int32_t doppler, vol, pan;
        distance(player.i32(0x1c), player.i32(0x28) >> 8, player.i32(0x108), doppler, vol, pan);
        const int32_t r = player.i32(0x2dc);
        const int32_t frequency = (doppler * r) / 512;
        const int32_t volume = (0x9ffe * vol / 0xef10) * engine_;
        const int slot = nitro_ ? 1 : tunnel_ ? 2 : 0;
        for (int s : {0, 1, 2}) if (s != slot && slots_[size_t(s)].looping) stop(s);
        play_loop(slot, frequency, volume, 0);
    }
    // 0x00434c40 / 0x004350e0: the two nearest other vehicles use slots 3/4 and 5/6.
    if (!others_off_) {
        std::vector<std::pair<int32_t, size_t>> near;
        for (size_t i = 1; i < riders.size(); ++i) {
            const int32_t d = std::abs(riders[i].bike.i32(0x1c) - listener_position_);
            if (d < hearing) near.emplace_back(d, i);
        }
        std::sort(near.begin(), near.end());
        for (int k = 0; k < 2; ++k) {
            const int base = k == 0 ? 3 : 5;
            if (size_t(k) >= near.size()) { stop(base); stop(base + 1); continue; }
            const size_t i = near[size_t(k)].second;
            Bike& b = riders[i].bike;
            update_engine_rpm(b);
            int32_t doppler, vol, pan;
            distance(b.i32(0x1c), b.i32(0x28) >> 8, b.i32(0x108), doppler, vol, pan);
            const int32_t r = b.i32(0x2dc);
            stop(base + 1);   // bikes use the EngOther slot; cars (plan 05) use the next
            play_loop(base, (doppler * r) / 512, (vol * 0x9ffe / 0xef10) * others_, pan);
        }
    }
    // 0x00434bd0: skidding while gripping (+0x164 > 0x32): on the road surface classes up to
    // the 0x0046dce5 limit, off-road beyond it.
    if (player.i32(0x160) != 0 && player.i32(0x108) > 0 && int8_t(player.u8(0x164)) > 0x32 && player.i32(0x2f4) == 0)
        post({player.i32(0x158) <= skid_limit_ ? SoundEvent::skid_on : SoundEvent::skid_off, player.i32(0x28), player.i32(0x20), player.i32(0x1c), player.i32(0xec)});
    // 0x00434a80: sliding (state 1, fast, low) and running back to the bike (state 2).
    const int32_t state = player.i32(0x2f4);
    if (state == 1 && player.i32(0xec) >= 0x321 && player.i32(0x20) <= 399)
        post({SoundEvent::slide, player.i32(0x28), player.i32(0x20), player.i32(0x1c), player.i32(0xec)});
    else if (sliding_) { stop(28); sliding_ = false; }
    // Finish yell (0x004351f0): once, when the player has finished.
    if (riders[0].finished && !finish_yell_) {
        finish_yell_ = true;
        if (!effects_off_) play_once(grunt_offset_ ? 8 : 7, effects_ * 0xcc, 0, 0x2b11);
    }
    // Event consumer 0x004351f0.
    for (const SoundEventRecord& e : queue_) {
        if (effects_off_ && e.code != SoundEvent::nitro_on && e.code != SoundEvent::nitro_off) continue;
        const int32_t pitch = int32_t(random() % 0xbb9) + 0x214d;
        const int32_t vol = (int32_t(random() % 0x1001) + 0x3fff) / 100 * effects_;
        switch (e.code) {
        case SoundEvent::crash_light: play_once(14, vol, 0, pitch); break;
        case SoundEvent::crash_medium: play_once(13, vol, 0, pitch); break;
        case SoundEvent::crash_hard: play_once(12, vol, 0, pitch); break;
        case SoundEvent::grunt_a: play_once(15 + grunt_offset_, vol, 0, pitch); break;
        case SoundEvent::grunt_b: play_once(16 + grunt_offset_, vol, 0, pitch); break;
        case SoundEvent::grunt_c: play_once(17 + grunt_offset_, vol, 0, pitch); break;
        case SoundEvent::swish_chain: if (!finish_yell_) play_once(23, vol, 0, pitch); break;
        case SoundEvent::swish_club: if (!finish_yell_) play_once(24, vol, 0, pitch); break;
        case SoundEvent::impact_chain: play_once(9, vol, 0, pitch); break;
        case SoundEvent::impact_club: play_once(10, vol, 0, pitch); break;
        case SoundEvent::impact_kick: play_once(21, vol, 0, pitch); break;
        case SoundEvent::impact_punch: play_once(22, vol, 0, pitch); break;
        case SoundEvent::footstep: play_once(27, (int32_t(random() % 0x4b1) + 0x4bc8) / 100 * effects_, 0, 0x2b11); break;
        case SoundEvent::skid_on: play_once(25, effects_ * 0x96, 0, pitch); break;
        case SoundEvent::nitro_on: nitro_ = true; break;
        case SoundEvent::nitro_off: nitro_ = false; break;
        case SoundEvent::bounce: play_once(11, effects_ * 0x96, 0, pitch); break;
        case SoundEvent::horn: play_once(29, (int32_t(random()) + 0x3554) / 100 * effects_, 0, pitch); break;
        case SoundEvent::skid_off: {
            int32_t d, v, p;
            distance(e.position, e.lateral >> 8, e.speed, d, v, p);
            v = std::min(v * v * 0x4fff >> 16, 0x4fff);   // 0x00435840 volume
            play_once(26, v / 100 * effects_, p, int32_t(random() % 0x191) + 0x2a49);
            break;
        }
        case SoundEvent::slide: {
            // 0x004359b0: RIDERSLI at speed + 4000 Hz, swelling by 0x1e per update to 0x4fff.
            const int32_t frequency = e.speed + 4000;
            if (!sliding_) { slide_volume_ = 13000; play_once(28, effects_ * 0x82, 0, frequency); sliding_ = true; }
            else {
                slide_volume_ = std::min(slide_volume_ + 0x1e, 0x4fff);
                Slot& s = slots_[28];
                if (s.voice >= 0) mixer_.set(s.voice, mixer_volume(ds_attenuation(slide_volume_ / 100 * effects_)), 0, pitch_q8(frequency, s.pcm.rate));
            }
            break;
        }
        case SoundEvent::speech: {
            const int slot = e.argument - 0x44;
            if (slot > 0 && slot < 0x45) play_once(slot, effects_ * 0x96, 0, 0x2b11);
            break;
        }
        default: break;   // siren (police, plan 05)
        }
    }
    queue_.clear();
}

MusicPlayer::MusicPlayer(const Assets& assets, Mixer& mixer) : assets_(assets), mixer_(mixer) {}

MusicPlayer::~MusicPlayer() { stop(); }

const char* MusicPlayer::track_name(int i) { return i >= 0 && i < 14 ? tracks[i] : ""; }

void MusicPlayer::play(int track) {
    stop();
    // 0x00403040: a missing or unreadable track advances to the next one.
    for (int tries = 0; tries < 14; ++tries) {
        track_ = ((track + tries) % 14 + 14) % 14;
        const std::string path = "AUDIO/MUSIC/" + upper(tracks[track_]) + ".RRA";
        if (!assets_.exists(path)) continue;
        try {
            pcm_ = std::make_unique<PcmSound>(read_wav(assets_.read(path)));
        } catch (const std::runtime_error&) { continue; }
        voice_ = mixer_.play(pcm_.get(), volume_, 0, 256, false);
        return;
    }
}

void MusicPlayer::stop() {
    if (voice_ >= 0) mixer_.stop(voice_);
    if (pcm_) mixer_.retire(pcm_.get());
    pcm_.reset();
    voice_ = -1;
}

void MusicPlayer::set_volume(int v) {
    volume_ = std::clamp(v, 0, 127);
    if (voice_ >= 0) mixer_.set(voice_, volume_, 0, 256);
}

void MusicPlayer::update() {
    if (voice_ >= 0 && !mixer_.active(voice_)) play(track_ + 1);   // track end: next track (0x00403040)
}
}
