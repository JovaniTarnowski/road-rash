// Plan 34 Task 3 Step 5: rr-determinism prints the state hash every 60
// ticks of a fixed multi-human race so platforms can be compared.
//   rr-determinism --synthetic        synthetic course and bikes (no game
//                                     data; runs in CI on every platform)
//   rr-determinism --disc <ROADRASH>  an 8-human networked Napa race from
//                                     the user's disc (local and devices)
// Output: one "<tick> <hash>" line per 60 ticks, hex hashes.
#include "assets.hpp"
#include "race_setup.hpp"
#include "race_state.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

uint32_t script(int seat, int t) {
    uint32_t w = rr::control_accelerate;
    const int phase = (t / (29 + 13 * seat)) % 7;
    if (phase == 1 || phase == 5) w |= rr::control_left;
    if (phase == 3) w |= rr::control_right;
    if ((t + 11 * seat) % 61 < 5) w |= rr::control_punch;
    if ((t + 3 * seat) % 97 < 3) w |= rr::control_kick;
    if (t % 1300 > 1290) w |= rr::control_nitro;
    if (t % 2100 > 2060) w = rr::control_brake | rr::control_right;
    return w;
}

// Synthetic course: a winding road with crests, built in code.
std::vector<rr::RouteSample> course(size_t n, int phase) {
    std::vector<rr::RouteSample> out(n);
    for (size_t i = 0; i < n; ++i) {
        rr::RouteSample s{};
        const int k = int(i) + phase;
        s.path.bend = ((k / 90) % 4 == 1) ? 6 : ((k / 90) % 4 == 3) ? -5 : 0;
        s.path.left = s.path.right = 500;
        s.path.shoulder_left = s.path.shoulder_right = 200;
        s.path.left_q8 = s.path.right_q8 = 512;
        s.section = uint32_t(i / 256);
        s.local = uint32_t(i % 256);
        out[i] = s;
    }
    return out;
}

rr::BikeSpec spec(int variant) {
    rr::BikeSpec b{};
    b.d[1] = -800;
    b.d[2] = 1024;
    b.d[3] = 160;
    b.d[4] = 160;
    b.d[5] = 1;
    b.d[6] = 196;
    auto sp = [&](int offset) -> int32_t& { return b.d[size_t(7 + (offset - 0x1c4) / 4)]; };
    const int up[6] = {3000, 5000, 7000, 8500, 21504, 16777215}, down[6] = {-999, 2500, 4500, 6500, 8000, 17152};
    for (int g = 0; g < 6; ++g) {
        sp(0x1d0 + 16 * g) = 256 - 8 * g + variant;
        sp(0x1d4 + 16 * g) = up[g];
        sp(0x1d8 + 16 * g) = down[g];
    }
    sp(0x248) = 20000 + 500 * variant;
    sp(0x250) = 38;
    sp(0x254) = 10;
    sp(0x258) = 60;
    sp(0x25c) = 64;
    sp(0x26c) = -60;
    sp(0x270) = 10;
    sp(0x274) = 10;
    sp(0x290) = 64;
    sp(0x294) = 320;
    sp(0x29c) = 11776;
    sp(0x2a0) = 50;
    sp(0x2a4) = 30;
    sp(0x2b0) = 120;
    sp(0x2b8) = 196;
    sp(0x2bc) = 2560;
    sp(0x2d0) = 346;
    sp(0x2d4) = 128;
    return b;
}

void print_run(rr::RaceInstance& in, int humans, int countdown, int ticks) {
    std::vector<uint32_t> w(size_t(humans), 0u);
    for (int t = 0; t < ticks; ++t) {
        if (t < countdown)
            in.race->countdown_step();
        else {
            for (int k = 0; k < humans; ++k) w[size_t(k)] = script(k, t);
            in.race->step(w);
        }
        if ((t + 1) % 60 == 0) std::printf("%d %016" PRIx64 "\n", t, rr::state_hash(in));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--synthetic") == 0) {
        auto t = rr::PhysicsTables::synthetic();
        t.bike_object = {3,     4,    1, 0,    0,      0,    0,      0x12c, 0x100, 0x82, 8, 0x25, -0xa00,
                         0x100, 0x4a, 4, 0x18, 0x1e00, 0x40, 0x2300, 0xc0,  0x100, 0,    1, 1,    0};
        std::vector<rr::Opponent> field;
        for (int k = 1; k < 12; ++k) {
            rr::Opponent o{spec(k % 4), 80 + k, 90 + k, 80 + k, k % 3, k, k, 0, 0, false, 100, {}, k % 4};
            if (k < 8) o.human = k; // seats 1..7; riders 8..11 are A.I.
            field.push_back(o);
        }
        rr::RaceInstance in;
        in.race = std::make_unique<rr::Race>(course(6000, 0), course(6000, 45), t, spec(0), 90, field, 2);
        in.race->set_net(true);
        print_run(in, 8, 0x168, 6000);
        return 0;
    }
    if (argc >= 3 && std::strcmp(argv[1], "--disc") == 0) {
        rr::Assets assets(argv[2]);
        const auto exe = assets.read("ROADRASH.EXE");
        rr::NetRaceConfig net;
        for (int k = 0; k < 8; ++k) net.humans.push_back({k, 1 + k, -1, "P" + std::to_string(k)});
        net.ai_bikers = 2;
        net.seed = 0x0badc0deu;
        auto setup = rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 1, net));
        print_run(setup.instance, 8, 0x168, 6000);
        return 0;
    }
    std::fprintf(stderr, "usage: rr-determinism --synthetic | --disc <ROADRASH dir>\n");
    return 2;
}
