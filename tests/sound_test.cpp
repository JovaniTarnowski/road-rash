#include "assets.hpp"
#include "bike.hpp"
#include "race.hpp"
#include "sound.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

static std::vector<rr::RouteSample> straight(size_t n) {
    rr::RouteSample s{{0, 0, 500, 500, 512, 512}, 0, 0, 0};
    s.path.shoulder_left = s.path.shoulder_right = 200;
    return std::vector<rr::RouteSample>(n, s);
}

int main(int argc, char** argv) {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    // DirectSound conversions (0x004141c0): volume 16384 is 0 dB, 0 is -100 dB; linear in dB.
    check(rr::ds_attenuation(16384) == 0 && rr::ds_attenuation(30000) == 0 && rr::ds_attenuation(0) == -10000,
          "attenuation range");
    check(rr::ds_attenuation(8192) == -5000, "attenuation linear in dB");
    check(rr::mixer_volume(0) == 127 && rr::mixer_volume(-600) == 64 && rr::mixer_volume(-10000) == 0,
          "amplitude from hundredths of dB");
    check(rr::mixer_pan(0, false) == 0 && rr::mixer_pan(10000, false) == 63 &&
              rr::mixer_pan(10000, true) == -63,
          "pan and speaker swap");
    check(rr::pitch_q8(11025, 11025) == 256 && rr::pitch_q8(22050, 11025) == 512 &&
              rr::pitch_q8(1, 11025) == rr::pitch_q8(100, 11025),
          "frequency clamp and pitch");

    rr::Assets assets(argc > 1 ? argv[1] : RR_SOURCE_DIR "/../cdrom/ROADRASH");
    if (!assets.exists("ROADRASH.EXE")) {
        std::cout << "skip: no disc\n";
        return failures ? 1 : 77;
    }
    const auto exe = assets.read("ROADRASH.EXE");
    const auto curve = rr::read_volume_curve(exe);
    check(curve[0] == 0 && curve[100] == 100 && curve[11] == 4 && curve[50] == 0x45,
          "volume curve parsed from 0x004362e0");
    rr::Mixer mixer(48);
    {
        rr::SoundSystem sound(assets, mixer, curve, 2);
        for (int i = 1; i < 0x45; ++i)
            check(assets.exists(std::string("AUDIO/EFFECTS/") + rr::SoundSystem::effect_name(i) + ".WAV") ||
                      assets.exists("AUDIO/EFFECTS/" +
                                    [&] {
                                        std::string u;
                                        for (char c : std::string(rr::SoundSystem::effect_name(i)))
                                            u.push_back(char(std::toupper(static_cast<unsigned char>(c))));
                                        return u;
                                    }() +
                                    ".WAV"),
                  "effect file present");
        sound.start_race(7, 3);
        const auto t = rr::read_physics_tables(exe);
        const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        rr::Race race(straight(4000), straight(4000), t, specs.at(7), 90);
        for (int i = 0; i < 120; ++i) {
            race.step(rr::control_accelerate);
            sound.update(race);
        }
        std::vector<int16_t> out(2 * 2048);
        mixer.render(out.data(), 2048);
        double energy = 0;
        for (auto s : out) energy += double(s) * s;
        check(energy > 0, "engine is audible");
        // Exercise real audio cadence while travelling beyond the starting
        // listener's 30-sample hearing radius (the old test stopped at 120 ticks).
        rr::SoundLevels engine_only;
        engine_only.effects = engine_only.others = 0;
        sound.set_levels(engine_only);
        bool audible = true;
        for (int tick = 0; tick < 2400; ++tick) {
            race.step(rr::control_accelerate);
            sound.update(race);
            mixer.render(out.data(), 368);
            if (tick > 300) {
                bool nonzero = false;
                for (int i = 0; i < 736; ++i) nonzero |= out[size_t(i)] != 0;
                audible &= nonzero;
            }
        }
        check(race.position() > 0x3200 + 0x1e00, "sound regression travels beyond start hearing radius");
        check(audible, "player engine stays audible as the camera travels");
        {
            // Siren (0x00435ae0): its own channel; SIREN loops 0x259 updates, then
            // SIRENFST 0x79; an empty record (cop crashed or gone) silences it.
            sound.stop_race();
            sound.start_race(7, 3); // drop the engine voices
            rr::SoundLevels siren_only;
            siren_only.effects = siren_only.engine = siren_only.others = 0;
            sound.set_levels(siren_only);
            auto energy_now = [&] {
                std::vector<int16_t> block(2 * 1024);
                mixer.render(block.data(), 1024);
                double e = 0;
                for (auto v : block) e += double(v) * v;
                return e;
            };
            auto audible_now = [&] { return energy_now() > 0; };
            auto siren_at = [&](bool emitting) {
                const int32_t p = emitting ? int32_t(race.camera_position()) + 0x400 : 0;
                sound.post({rr::SoundEvent::siren, emitting ? race.camera_x() * 256 : 0, 0, p, 0, 0});
                sound.update(race);
            };
            for (int i = 0; i < 3; ++i) siren_at(true);
            check(audible_now(), "siren audible near the camera");
            bool always = true;
            for (int i = 0; i < 0x259 + 0x79 + 300; ++i) {
                siren_at(true);
                if (i % 50 == 0) always &= audible_now();
            }
            check(always, "siren keeps sounding across the buffer alternation");
            const double with_siren = energy_now();
            for (int i = 0; i < 3; ++i) siren_at(false);
            energy_now();
            check(with_siren > 0 && energy_now() == 0, "empty siren record silences it");
            sound.set_levels({});
        }
        {
            // Original 0x00434f40: each branch separation interpolates and
            // truncates independently, then their sum truncates on division
            // by two. Compare complete PCM with the equivalent translated
            // same-road emitter, so both attenuation and pan are exercised.
            auto a = straight(4000), b = straight(4000);
            for (auto& sample : a) {
                sample.dual = true;
                sample.half = 0;
            }
            for (auto& sample : b) {
                sample.dual = true;
                sample.half = 1;
            }
            a[49].x_sep = 200;
            a[50].x_sep = 195;
            b[51].x_sep = 1000;
            b[52].x_sep = 1513;
            // A negative separation with odd sums catches signed rounding.
            a[53].x_sep = -200;
            a[54].x_sep = -195;
            b[55].x_sep = -1001;
            b[56].x_sep = -1514;
            // Large separation must take the siren outside hearing range.
            a[57].x_sep = a[58].x_sep = 9000;
            b[59].x_sep = b[60].x_sep = 9000;
            b[63].dual = false; // a merged roadway shares both half masks
            b[63].x_sep = b[64].x_sep = 9000;
            rr::Race split(std::move(a), std::move(b), t, specs.at(7), 90);
            split.riders().push_back(rr::Rider{});
            split.riders().back().cop = true;
            rr::Mixer split_mixer(48);
            rr::SoundSystem split_sound(assets, split_mixer, curve, 2);
            rr::SoundLevels only;
            only.effects = only.engine = only.others = 0;
            split_sound.set_levels(only);
            auto pcm = [&](int road, int32_t lateral, int half_road = -1) {
                split_sound.stop_race();
                std::vector<int16_t> block(2 * 1024);
                split_mixer.render(block.data(), 64);
                split_sound.start_race(7, 3);
                split.riders().back().route = road;
                if (split.live_schedule())
                    split.riders().back().half_mirror.route = half_road < 0 ? road : half_road;
                split_sound.post({rr::SoundEvent::siren, lateral * 256, 0,
                                  int32_t(split.camera_position()) + 0x400, 0, 0});
                split_sound.update(split);
                split_mixer.render(block.data(), 1024);
                return block;
            };
            auto position = [&](int camera_sample, int cop_sample) {
                // The camera follows the player half image
                // (half_mirror.position), which the sim copies from the
                // bike's motion words after every move (0x004067d0): bike
                // +0x1c/+0x138 and the mirror always agree outside a contact
                // tick, so poke all three together.
                const int32_t camera_pos = camera_sample * 256 + 128 + 256;
                split.riders()[0].bike.i32(0x1c) = camera_pos;
                split.riders()[0].bike.i32(0x138) = camera_pos;
                split.riders()[0].half_mirror.position = camera_pos;
                split.riders()[0].sample = size_t(camera_sample + 1);
                split.riders().back().sample = size_t(cop_sample);
                split.riders().back().bike.i32(0x1c) = cop_sample * 256 + 192;
            };
            position(49, 51);
            const auto positive = pcm(1, 0);
            check(positive == pcm(0, 791), "split siren matches interpolated lateral separation");
            check(positive != pcm(0, 0), "split siren changes attenuation or pan");
            position(53, 55);
            check(pcm(1, 0) == pcm(0, -791), "negative split separation truncates toward zero");
            position(57, 59);
            const auto distant = pcm(1, 0);
            check(std::all_of(distant.begin(), distant.end(), [](int16_t v) { return v == 0; }),
                  "split separation can silence an otherwise nearby siren");
            position(61, 63);
            check(pcm(1, 0) == pcm(0, 0), "single road shares both branch masks");
            position(49, 51);
            split.set_live_schedule(true);
            // The half may lag its bike on the live scheduler: use its own
            // slice and position, matching DAT_004642dc in the original.
            split.riders()[0].half_mirror.sample = 50;
            split.riders().back().half_mirror.sample = 51;
            split.riders().back().half_mirror.position = 51 * 256 + 192;
            split.riders().back().sample = 200;
            split.riders().back().bike.i32(0x1c) = 200 * 256;
            check(pcm(1, 0) == pcm(0, 791), "live siren uses cop half slice and position");
            check(pcm(0, 0, 1) == pcm(0, 791, 0),
                  "live siren keeps cop half branch after bike changes route");
            split.riders()[0].half_mirror.route = 1;
            const auto player_half_other = pcm(0, 0, 0);
            split.riders()[0].half_mirror.route = 0;
            check(player_half_other == pcm(0, 99, 0),
                  "player half mask and camera separation use their own routes");
            split.set_live_schedule(false);
            position(49, 51);
            split.riders()[0].crash_active = true;
            // While crashed the half IS the crash body (0x00403d60): the
            // mirror reads the body's words, so keep body +0x1c/+0x138 and
            // the mirror together as the sim's copy does.
            split.riders()[0].crash_actor.motion.i32(0x1c) = split.riders()[0].bike.i32(0x1c);
            split.riders()[0].crash_actor.motion.i32(0x138) = split.riders()[0].bike.i32(0x138);
            split.riders()[0].half_mirror.position = split.riders()[0].bike.i32(0x1c);
            split.riders()[0].crash_actor.route = 1;
            split.riders()[0].crash_actor.sample = 50;
            const auto crash_half_other = pcm(0, 0);
            split.riders()[0].crash_active = false;
            check(crash_half_other == pcm(0, 99), "crashed player half uses body's route");
            split.riders().back().half_slice_present = false;
            const auto absent = pcm(1, 0);
            check(std::all_of(absent.begin(), absent.end(), [](int16_t v) { return v == 0; }),
                  "cop with no slice silences siren");
            split_sound.stop_race();
        }
        sound.set_levels({});
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
    check(std::string(rr::MusicPlayer::track_name(0)) == "HB_Trip" &&
              std::string(rr::MusicPlayer::track_name(14)).empty(),
          "track names");
    music.stop();
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
