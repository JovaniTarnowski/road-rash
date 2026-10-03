#include "assets.hpp"
#include "bike.hpp"
#include "race.hpp"
#include "sound.hpp"
#include <cmath>
#include <iostream>

static std::vector<rr::RouteSample> straight(size_t n) {
    rr::RouteSample s{{0, 0, 500, 500, 512, 512}, 0, 0, 0};
    s.path.shoulder_left = s.path.shoulder_right = 200;
    return std::vector<rr::RouteSample>(n, s);
}

int main(int argc, char** argv) {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << "\n"; } };
    // DirectSound conversions (0x004141c0): volume 16384 is 0 dB, 0 is -100 dB; linear in dB.
    check(rr::ds_attenuation(16384) == 0 && rr::ds_attenuation(30000) == 0 && rr::ds_attenuation(0) == -10000, "attenuation range");
    check(rr::ds_attenuation(8192) == -5000, "attenuation linear in dB");
    check(rr::mixer_volume(0) == 127 && rr::mixer_volume(-600) == 64 && rr::mixer_volume(-10000) == 0, "amplitude from hundredths of dB");
    check(rr::mixer_pan(0, false) == 0 && rr::mixer_pan(10000, false) == 63 && rr::mixer_pan(10000, true) == -63, "pan and speaker swap");
    check(rr::pitch_q8(11025, 11025) == 256 && rr::pitch_q8(22050, 11025) == 512 && rr::pitch_q8(1, 11025) == rr::pitch_q8(100, 11025), "frequency clamp and pitch");

    rr::Assets assets(argc > 1 ? argv[1] : RR_SOURCE_DIR "/../cdrom/ROADRASH");
    if (!assets.exists("ROADRASH.EXE")) { std::cout << "skip: no disc\n"; return failures ? 1 : 77; }
    const auto exe = assets.read("ROADRASH.EXE");
    const auto curve = rr::read_volume_curve(exe);
    check(curve[0] == 0 && curve[100] == 100 && curve[11] == 4 && curve[50] == 0x45, "volume curve parsed from 0x004362e0");
    rr::Mixer mixer(48);
    {
        rr::SoundSystem sound(assets, mixer, curve, 2);
        for (int i = 1; i < 0x45; ++i) check(assets.exists(std::string("AUDIO/EFFECTS/") + rr::SoundSystem::effect_name(i) + ".WAV") ||
                                              assets.exists("AUDIO/EFFECTS/" + [&] { std::string u; for (char c : std::string(rr::SoundSystem::effect_name(i))) u.push_back(char(std::toupper(static_cast<unsigned char>(c)))); return u; }() + ".WAV"),
                                              "effect file present");
        sound.start_race(7, 3);
        const auto t = rr::read_physics_tables(exe);
        const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        rr::Race race(straight(4000), straight(4000), t, specs.at(7), 90);
        for (int i = 0; i < 120; ++i) { race.step(rr::control_accelerate); sound.update(race); }
        std::vector<int16_t> out(2 * 2048);
        mixer.render(out.data(), 2048);
        double energy = 0;
        for (auto s : out) energy += double(s) * s;
        check(energy > 0, "engine is audible");
        sound.post({rr::SoundEvent::crash_hard, 0, 0, race.bike().i32(0x1c), 0, 0});
        sound.update(race);
        mixer.render(out.data(), 2048);
        sound.stop_race();
        mixer.render(out.data(), 64);
    }
    rr::MusicPlayer music(assets, mixer);
    music.play(13);
    check(music.active() && music.track() == 13, "soundtrack track 13 (T_Auto) plays");
    std::vector<int16_t> out(2 * 1024);
    mixer.render(out.data(), 1024);
    double energy = 0;
    for (auto s : out) energy += double(s) * s;
    check(energy > 0, "soundtrack audible");
    check(std::string(rr::MusicPlayer::track_name(0)) == "HB_Trip" && std::string(rr::MusicPlayer::track_name(14)).empty(), "track names");
    music.stop();
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
