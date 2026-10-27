// Plan 34 Task 2 Step 5: multi-human deterministic races. Needs the disc
// (exit 77 without it). Two and eight humans with scripted inputs give
// identical per-tick field hashes across runs; reading the race through a
// different view seat changes nothing; a departed human handed to its AI at
// tick T is deterministic; the networked field follows 0x00405600.
#include "assets.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "race_setup.hpp"

#include <cstdlib>
#include <filesystem>

namespace {

uint64_t fnv(uint64_t h, int64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= uint64_t(v >> (8 * i)) & 0xff;
        h *= 0x100000001b3ull;
    }
    return h;
}

uint64_t field_hash(const rr::Race& race) {
    const rr::RaceSnapshot s = race.snapshot();
    uint64_t h = 0xcbf29ce484222325ull;
    h = fnv(h, s.tick);
    h = fnv(h, s.rng);
    for (const auto& f : s.fields) h = fnv(h, f.value);
    return h;
}

// Deterministic script per seat: throttle with seat-specific steering and
// attack pulses, so the humans drive differently and meet each other.
uint32_t script(int seat, int tick) {
    uint32_t w = rr::control_accelerate;
    const int phase = (tick / (40 + 7 * seat)) % 4;
    if (phase == 1) w |= rr::control_left;
    if (phase == 3) w |= rr::control_right;
    if ((tick + 13 * seat) % 97 < 6) w |= rr::control_punch;
    if (seat % 3 == 2 && tick % 600 > 590) w |= rr::control_nitro;
    return w;
}

struct Run {
    std::vector<uint64_t> hashes;
    std::vector<rr::Race::HumanOutcome> outcomes;
};

Run run(const rr::Assets& assets, const std::vector<uint8_t>& exe, int humans, int ai, int ticks,
        int handover_tick = -1, int view_seat = 0) {
    rr::NetRaceConfig net;
    for (int k = 0; k < humans; ++k) net.humans.push_back({k, 1 + (k * 3) % 8, -1, "P" + std::to_string(k)});
    net.ai_bikers = ai;
    net.seed = 0x5eed1234u + uint32_t(humans);
    auto setup = rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 1, net));
    rr::Race& race = *setup.instance.race;
    Run out;
    for (int t = 0; t < 0x168; ++t) race.countdown_step(); // networked countdown (0x00442dd0)
    std::vector<uint32_t> words(size_t(humans), 0u);
    for (int t = 0; t < ticks; ++t) {
        if (t == handover_tick) race.set_human_driven(1, false);
        for (int k = 0; k < humans; ++k) words[size_t(k)] = script(k, t);
        race.step(words);
        // View reads (camera, HUD bike) through another seat must not feed back.
        if (view_seat >= 0) (void)race.riders()[size_t(race.human_rider(view_seat))].bike.i32(0x1c);
        (void)race.camera();
        out.hashes.push_back(field_hash(race));
    }
    for (int k = 0; k < humans; ++k) out.outcomes.push_back(race.human_outcome(k));
    return out;
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

    // Field layout (0x00405600 / 0x004317a0).
    check.eq(rr::net_rider_count(2, 2), 15, "full: 15 riders");
    check.eq(rr::net_rider_count(2, 1), 8, "limited: 8 riders");
    check.eq(rr::net_rider_count(3, 0), 3, "humans only");
    check.eq(rr::net_rider_count(8, 1), 8, "eight humans fill a limited field");
    {
        rr::NetRaceConfig net;
        for (int k = 0; k < 6; ++k) net.humans.push_back({k, k + 1, -1, "P"});
        net.ai_bikers = 1;
        auto setup = rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 0, net));
        const auto& race = *setup.instance.race;
        check.eq(race.humans(), 6, "six seats");
        check.eq(race.riders().size(), size_t(8), "limited field with six humans");
        bool seats_ok = true;
        for (int k = 0; k < 6; ++k) {
            const auto& r = race.riders()[size_t(race.human_rider(k))];
            seats_ok &= race.human_rider(k) == k && !r.ai && r.human == k && r.bike.i32(0x324) == 0;
        }
        check(seats_ok, "seat k drives rider k, player-built");
        // More than four humans: grid 0x00464258 (rows 0..3 at 0x3300, 4..7 at 0x3200).
        check.eq(race.riders()[0].bike.i32(0x1c), 0x3300, "seat 0 on the >4-human grid row");
        check.eq(race.riders()[4].bike.i32(0x1c), 0x3200, "seat 4 on the >4-human grid row");
        check(race.riders()[6].ai && race.riders()[6].rider_idx == 6 && race.riders()[7].rider_idx == 7,
              "A.I. take rider rows equal to their entry");
        check(race.net(), "net rules on");
    }
    {
        rr::NetRaceConfig net;
        net.humans = {{0, 1, -1, "A"}, {1, 2, -1, "B"}};
        net.ai_bikers = 0;
        auto setup = rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 0, net));
        const auto& race = *setup.instance.race;
        check.eq(race.riders().size(), size_t(2), "humans-only field");
        check.eq(race.riders()[1].bike.i32(0x1c), 0x3200, "seat 1 on 0x00464218 row 1");
    }

    // Determinism: two and eight humans, identical across runs.
    const int ticks = 1500;
    const Run two_a = run(assets, exe, 2, 2, ticks), two_b = run(assets, exe, 2, 2, ticks);
    check(two_a.hashes == two_b.hashes, "two humans: identical per-tick field hashes across runs");
    const Run eight_a = run(assets, exe, 8, 1, ticks), eight_b = run(assets, exe, 8, 1, ticks);
    check(eight_a.hashes == eight_b.hashes, "eight humans: identical per-tick field hashes across runs");
    check(two_a.hashes != eight_a.hashes, "different fields hash differently");
    // The humans actually raced (moved apart from the grid).
    {
        rr::NetRaceConfig net;
        net.humans = {{0, 1, -1, "A"}, {1, 4, -1, "B"}};
        auto setup = rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 1, net));
        auto& race = *setup.instance.race;
        for (int t = 0; t < 0x168; ++t) race.countdown_step();
        const int32_t start1 = race.riders()[1].bike.i32(0x1c);
        for (int t = 0; t < 600; ++t) {
            const uint32_t w[2] = {script(0, t), script(1, t)};
            race.step(w);
        }
        check(race.riders()[1].bike.i32(0x1c) > start1 + 0x2000, "seat 1 is driven by its own word");
        check(race.riders()[0].bike.i32(0x1c) != race.riders()[1].bike.i32(0x1c),
              "seats drive independently");
    }
    // C1: a seat that pulls away keeps its own ring window, and the field
    // around it stays live while seat 0 waits at the grid.
    {
        rr::NetRaceConfig net;
        net.humans = {{0, 1, -1, "Waits"}, {1, 2, -1, "Goes"}};
        net.ai_bikers = 2;
        auto setup = rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 0, net));
        auto& race = *setup.instance.race;
        check.eq(setup.instance.seat_rings.size(), size_t(1), "one extra ring window for seat 1");
        for (int t = 0; t < 0x168; ++t) race.countdown_step();
        bool odd_kind = false;
        int cars_in_seat1_window = 0;
        for (int t = 0; t < 2400; ++t) {
            const uint32_t w[2] = {rr::control_brake, rr::control_accelerate};
            race.step(w);
            const auto& ring1w = *setup.instance.seat_rings[0];
            if (setup.instance.traffic)
                for (const auto& car : setup.instance.traffic->cars()) {
                    if (!car.active) continue;
                    odd_kind = odd_kind || (car.kind & 1);
                    if (t > 1200 && car.position >= ring1w.back_q8() && car.position < ring1w.front_q8() &&
                        car.position > setup.instance.ring->front_q8())
                        ++cars_in_seat1_window;
                }
        }
        // C4: keep riding past the periodic police spawn (0x00407f30, 4096 ticks).
        const rr::Rider* cop = nullptr;
        for (int t = 2400; t < 4400 && !cop; ++t) {
            const uint32_t w[2] = {rr::control_brake, rr::control_accelerate};
            race.step(w);
            for (const auto& r : race.riders())
                if (r.cop) cop = &r;
        }
        check(cop != nullptr, "C4: a cop spawned in the networked race");
        if (cop) {
            const int32_t cp = cop->bike.i32(0x1c);
            bool near_human = false;
            for (int k = 0; k < 2; ++k)
                near_human = near_human || std::abs(cp - race.riders()[size_t(k)].bike.i32(0x1c)) < 0x8000;
            check(near_human, "C4: the cop is placed from a human, inside that human's window");
            const int column = cop->brain.overtake_index;
            if (column >= 0 && column < 10)
                check(race.riders()[0].brain.rating_tgt[size_t(column * 3 + 2)] ==
                          race.riders()[1].brain.rating_tgt[size_t(column * 3 + 2)],
                      "C4: every human carries the relationship row toward the cop");
            std::cout << "C4: cop at sample " << (cp >> 8) << "\n";
        }
        check(!odd_kind, "networked race spawns no crossing (odd-kind) cars (0x0044df60)");
        check(cars_in_seat1_window > 0, "C3: traffic runs in seat 1's window beyond seat 0's");
        std::cout << "C3: car-ticks in seat 1's own window " << cars_in_seat1_window << "\n";
        if (std::getenv("RR_DEBUG_C3") && setup.instance.traffic)
            for (const auto& car : setup.instance.traffic->cars())
                if (car.active)
                    std::cout << "  car kind " << int(car.kind) << " at " << (car.position >> 8) << "\n";
        const auto& ring0 = *setup.instance.ring;
        const auto& ring1 = *setup.instance.seat_rings[0];
        const int32_t p1 = race.riders()[1].bike.i32(0x1c);
        check(p1 > ring0.front_q8(), "seat 1 has left seat 0's window");
        check(p1 > ring1.back_q8() && p1 < ring1.front_q8(), "seat 1's window follows it");
        int live_near = 0;
        for (const auto& r : race.riders())
            if (r.ai && r.live && std::abs(r.bike.i32(0x1c) - p1) < 0x4000) ++live_near;
        int live_near_seat0 = 0;
        for (const auto& r : race.riders())
            if (r.ai && r.live && r.bike.i32(0x1c) < ring0.front_q8()) ++live_near_seat0;
        check(live_near + live_near_seat0 > 0, "A.I. stay live in some human window");
        std::cout << "C1: seat0 window " << (ring0.back_q8() >> 8) << ".." << (ring0.front_q8() >> 8)
                  << " seat1 at " << (p1 >> 8) << " window " << (ring1.back_q8() >> 8) << ".."
                  << (ring1.front_q8() >> 8) << " live A.I. near seat1 " << live_near << "\n";
    }
    // The view seat changes no simulation hash.
    const Run view7 = run(assets, exe, 8, 1, ticks, -1, 7);
    check(view7.hashes == eight_a.hashes, "following seat 7 instead of seat 0 changes nothing");
    // Hand-over to AI at tick T: deterministic, and it changes the race.
    const Run h1 = run(assets, exe, 2, 2, ticks, 700), h2 = run(assets, exe, 2, 2, ticks, 700);
    check(h1.hashes == h2.hashes, "AI hand-over at a fixed tick is deterministic");
    check(std::equal(h1.hashes.begin(), h1.hashes.begin() + 700, two_a.hashes.begin()) &&
              h1.hashes[ticks - 1] != two_a.hashes[ticks - 1],
          "hand-over changes nothing before T and the race after it");
    return check.finish("race_multi");
}
