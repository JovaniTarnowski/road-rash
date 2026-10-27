// Plan 34 Task 3 Step 4: rollback cost on this machine. Needs the disc.
// Measures one race step, save_state, load_state and the worst-case frame
// (restore + 16 re-simulated ticks with their saves) for 2 and 8 humans.
#include "assets.hpp"
#include "net/rollback.hpp"
#include "race_state.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    const std::string root = argc > 1 ? argv[1] : std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    rr::Assets assets(root);
    const auto exe = assets.read("ROADRASH.EXE");
    using clock = std::chrono::steady_clock;
    auto us = [](clock::duration d) { return std::chrono::duration<double, std::micro>(d).count(); };
    for (int humans : {2, 8}) {
        rr::NetRaceConfig net;
        for (int k = 0; k < humans; ++k) net.humans.push_back({k, 1 + k % 8, -1, "P"});
        net.ai_bikers = humans > 4 ? 1 : 2;
        auto setup = rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 0, net));
        auto& in = setup.instance;
        std::vector<uint32_t> w(size_t(humans), rr::control_accelerate);
        for (int t = 0; t < 0x168; ++t) in.race->countdown_step();
        for (int t = 0; t < 1200; ++t) in.race->step(w); // into the race
        std::vector<double> step, save, load, frame;
        std::vector<rr::RaceState> ring(17);
        for (auto& s : ring) rr::save_state(in, s);
        for (int rep = 0; rep < 200; ++rep) {
            auto t0 = clock::now();
            rr::save_state(in, ring[size_t(rep % 17)]);
            auto t1 = clock::now();
            in.race->step(w);
            auto t2 = clock::now();
            save.push_back(us(t1 - t0));
            step.push_back(us(t2 - t1));
            if (rep % 20 == 19) { // worst case: restore 16 back, re-simulate 16 with saves
                auto f0 = clock::now();
                rr::load_state(in, ring[size_t((rep - 16) % 17)]);
                auto f1 = clock::now();
                for (int k = 0; k < 16; ++k) {
                    rr::save_state(in, ring[size_t((rep - 16 + k) % 17)]);
                    in.race->step(w);
                }
                auto f2 = clock::now();
                load.push_back(us(f1 - f0));
                frame.push_back(us(f2 - f0));
            }
        }
        auto pct = [](std::vector<double> v, double p) {
            std::sort(v.begin(), v.end());
            return v[size_t(p * double(v.size() - 1))];
        };
        std::printf("%d humans (%zu riders): step p50 %.1f p99 %.1f us | save p50 %.1f p99 %.1f us | load p50 %.1f us | "
                    "worst frame (16 resim+saves) p50 %.0f max %.0f us\n",
                    humans, in.race->riders().size(), pct(step, 0.5), pct(step, 0.99), pct(save, 0.5),
                    pct(save, 0.99), pct(load, 0.5), pct(frame, 0.5), pct(frame, 1.0));
    }
}
