// Plan 34 Task 3 Step 1: race snapshots. Needs the disc (77 without it).
// Run N ticks, save, run M, load, run M again: the per-tick state hashes and
// the RNG are identical, across a long scripted race that crashes, walks,
// meets police and pedestrians and reaches the finish, and across a
// two-human race with two ring windows. Saving into a warmed slot allocates
// nothing.
#define RR_ALLOC_COUNTER_DEFINE
#include "alloc_counter.hpp"
#include "assets.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "race_state.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace {

uint32_t script(int seat, int t) {
    uint32_t w = rr::control_accelerate;
    const int phase = (t / (37 + 11 * seat)) % 6;
    if (phase == 1 || phase == 4) w |= rr::control_left;
    if (phase == 2) w |= rr::control_right;
    if ((t + 7 * seat) % 53 < 5) w |= rr::control_punch;
    if (t % 700 > 690) w |= rr::control_nitro;
    if (t % 1900 > 1840) w = rr::control_brake | rr::control_left; // into the wall: crash, walk back
    return w;
}

struct Coverage {
    int crashed = 0, walking = 0, cop = 0, pedestrians = 0, finished = 0, branch = 0;
};

void observe(const rr::RaceInstance& in, Coverage& c) {
    for (const auto& r : in.race->riders()) {
        c.crashed += r.bike.i32(0x2f4) != 0;
        c.walking += r.bike.i32(0x2f4) == 1 || r.bike.i32(0x2f4) == 2;
        c.cop += r.cop;
        c.finished += r.finished;
        c.branch += r.route != 0;
    }
    if (in.pedestrians)
        for (const auto& p : in.pedestrians->pool()) c.pedestrians += p.active;
}

} // namespace

int main() {
    rr::test::Checks check;
    const std::string root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(root + "/ROADRASH.EXE")) {
        std::cout << "SKIP no disc\n";
        return rr::golden::skip;
    }
    rr::Assets assets(root);
    const std::vector<uint8_t> exe = assets.read("ROADRASH.EXE");

    auto exercise = [&](const char* name, rr::RaceInstance& in, int humans, int ticks, int every, int span) {
        std::vector<uint32_t> words(size_t(humans), 0u);
        auto tick = [&](int t) {
            for (int k = 0; k < humans; ++k) words[size_t(k)] = script(k, t);
            in.race->step(words);
        };
        Coverage cov;
        rr::RaceState state;
        int compared = 0, mismatches = 0;
        for (int t = 0; t < ticks; ++t) {
            if (t % every == every - 1 && t + span < ticks) {
                const uint64_t at_save = rr::state_hash(in);
                rr::save_state(in, state);
                std::vector<uint64_t> first;
                for (int k = 1; k <= span; ++k) {
                    tick(t + k);
                    first.push_back(rr::state_hash(in));
                }
                const uint32_t rng_after = in.race->rng_state();
                rr::load_state(in, state);
                check(rr::state_hash(in) == at_save, std::string(name) + ": load restores the saved hash");
                for (int k = 1; k <= span; ++k) {
                    tick(t + k);
                    mismatches += rr::state_hash(in) != first[size_t(k - 1)];
                    ++compared;
                }
                check(in.race->rng_state() == rng_after, std::string(name) + ": RNG identical after re-run");
                t += span;
                observe(in, cov);
                continue;
            }
            tick(t);
            observe(in, cov);
            if (in.race->riders()[0].finished && t > 200 && cov.finished > 0 && t % every == 0) break;
        }
        check(mismatches == 0, std::string(name) + ": re-run after load matches the first run on every tick");
        std::cout << name << ": compared " << compared << " ticks, crashed " << cov.crashed << ", walking "
                  << cov.walking << ", cop " << cov.cop << ", pedestrians " << cov.pedestrians
                  << ", finished " << cov.finished << ", branch " << cov.branch << "\n";
        return cov;
    };

    {
        rr::RaceConfig config;
        config.course = "NAPA";
        config.level = 0;
        auto setup = rr::make_race_setup(assets, exe, config);
        for (int t = 0; t < 180; ++t) setup.instance.race->countdown_step();
        const Coverage c = exercise("napa-solo", setup.instance, 1, 9000, 400, 120);
        check(c.crashed > 0 && c.walking > 0, "covers crashes and walking");
        check(c.cop > 0, "covers police");
        check(c.pedestrians > 0, "covers pedestrians");
        // Allocation: a warmed slot saves without allocating.
        rr::RaceState slot;
        rr::save_state(setup.instance, slot);
        setup.instance.race->step(rr::control_accelerate);
        rr::save_state(setup.instance, slot);
        setup.instance.race->step(rr::control_accelerate);
        size_t allocations = 0;
        {
            rr::test::CountAllocs counter;
            rr::save_state(setup.instance, slot);
            allocations = counter.count();
        }
        check(allocations == 0,
              "saving into a warmed slot allocates nothing (" + std::to_string(allocations) + ")");
        // Cost (Debug build; the release budget is measured in Task 3 Step 4).
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 200; ++i) rr::save_state(setup.instance, slot);
        const auto t1 = std::chrono::steady_clock::now();
        for (int i = 0; i < 200; ++i) rr::load_state(setup.instance, slot);
        const auto t2 = std::chrono::steady_clock::now();
        std::cout << "save " << std::chrono::duration<double, std::micro>(t1 - t0).count() / 200
                  << " us, load " << std::chrono::duration<double, std::micro>(t2 - t1).count() / 200
                  << " us\n";
    }
    {
        // Finish line: accelerate straight through on the shortest level.
        rr::RaceConfig config;
        config.course = "NAPA";
        config.level = 0;
        auto setup = rr::make_race_setup(assets, exe, config);
        auto& race = *setup.instance.race;
        for (int t = 0; t < 180; ++t) race.countdown_step();
        int t = 0;
        while (!race.riders()[0].finished && t < 40000) {
            race.step(rr::control_accelerate | (t % 300 == 0 ? rr::control_nitro : 0u));
            ++t;
            if (race.finish_q8() && race.riders()[0].bike.i32(0x1c) > race.finish_q8() - 0x1800) break;
        }
        rr::RaceState state;
        rr::save_state(setup.instance, state);
        std::vector<uint64_t> first;
        for (int k = 0; k < 600; ++k) {
            race.step(rr::control_accelerate);
            first.push_back(rr::state_hash(setup.instance));
        }
        const bool finished = race.riders()[0].finished;
        rr::load_state(setup.instance, state);
        bool same = true;
        for (int k = 0; k < 600; ++k) {
            race.step(rr::control_accelerate);
            same &= rr::state_hash(setup.instance) == first[size_t(k)];
        }
        check(finished, "the finish run reaches the line");
        check(same, "save/load across the finish line");
    }
    {
        // Split: periodic right steering takes Napa's second branch near
        // tick 9393; save before it, cross, load, cross again.
        rr::RaceConfig config;
        config.course = "NAPA";
        auto setup = rr::make_race_setup(assets, exe, config);
        auto& race = *setup.instance.race;
        for (int t = 0; t < 180; ++t) race.countdown_step();
        auto word = [](int t) {
            return rr::control_accelerate | ((t / 20) % 3 == 0 ? rr::control_right : 0u);
        };
        int t = 0;
        for (; t < 9200; ++t) race.step(word(t));
        rr::RaceState state;
        rr::save_state(setup.instance, state);
        std::vector<uint64_t> first;
        bool branched = false;
        for (int k = 0; k < 600; ++k) {
            race.step(word(t + k));
            first.push_back(rr::state_hash(setup.instance));
            for (const auto& r : race.riders()) branched = branched || r.route != 0;
        }
        rr::load_state(setup.instance, state);
        bool same = true;
        for (int k = 0; k < 600; ++k) {
            race.step(word(t + k));
            same &= rr::state_hash(setup.instance) == first[size_t(k)];
        }
        check(branched, "the split run takes the second branch");
        check(same, "save/load across a split");
    }
    {
        rr::NetRaceConfig net;
        net.humans = {{0, 1, -1, "A"}, {1, 6, -1, "B"}};
        net.seed = 77;
        auto setup = rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 1, net));
        for (int t = 0; t < 0x168; ++t) setup.instance.race->countdown_step();
        exercise("napa-two-humans", setup.instance, 2, 4000, 500, 120);
    }
    return check.finish("race_state");
}
