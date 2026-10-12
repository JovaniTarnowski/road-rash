#include "sim_fields.hpp"
#include "race.hpp"
#include "../oracle/sim_fields.h"
#include <cstring>
#include <iostream>
#include <iterator>
#include <set>

// Plan 10 task 2: the native and oracle field tables agree, and a synthetic
// race snapshot carries rider.0.position equal to race.position().
static std::vector<rr::RouteSample> straight(size_t n) {
    rr::RouteSample s{{0, 0, 500, 500, 512, 512}, 0, 0, 0};
    s.path.shoulder_left = s.path.shoulder_right = 200;
    return std::vector<rr::RouteSample>(n, s);
}

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    if (std::size(rr::sim::rider_fields) != RR_SIM_RIDER_FIELD_COUNT) { std::cerr << "rider field count\n"; ++failures; }
    for (size_t i = 0; i < std::size(rr::sim::rider_fields) && i < RR_SIM_RIDER_FIELD_COUNT; ++i) {
        if (std::strcmp(rr::sim::rider_fields[i].name, rr_sim_rider_fields[i].name) != 0 ||
            rr::sim::rider_fields[i].offset != rr_sim_rider_fields[i].offset ||
            rr::sim::rider_fields[i].width != rr_sim_rider_fields[i].width) { std::cerr << "rider field " << i << '\n'; ++failures; }
    }
    if (std::size(rr::sim::car_fields) != RR_SIM_CAR_FIELD_COUNT) { std::cerr << "car field count\n"; ++failures; }
    for (size_t i = 0; i < std::size(rr::sim::car_fields) && i < RR_SIM_CAR_FIELD_COUNT; ++i) {
        if (std::strcmp(rr::sim::car_fields[i].name, rr_sim_car_fields[i].name) != 0 ||
            rr::sim::car_fields[i].offset != rr_sim_car_fields[i].offset) { std::cerr << "car field " << i << '\n'; ++failures; }
    }
    std::set<std::string> seen_rider, seen_car;
    for (const auto& f : rr::sim::rider_fields) if (!seen_rider.insert(f.name).second) { std::cerr << "duplicate rider " << f.name << '\n'; ++failures; }
    for (const auto& f : rr::sim::car_fields) if (!seen_car.insert(f.name).second) { std::cerr << "duplicate car " << f.name << '\n'; ++failures; }

    rr::BikeSpec spec{};
    rr::Race race(straight(200), straight(200), rr::PhysicsTables::synthetic(), spec, 90);
    for (int i = 0; i < 10; ++i) race.step(rr::control_accelerate);
    const auto snap = race.snapshot();
    check(snap.tick == 10, "snapshot tick");
    bool found = false;
    for (const auto& f : snap.fields)
        if (f.name == "rider.0.position") { found = true; check(uint32_t(f.value) == race.position(), "rider.0.position equals race.position()"); }
    check(found, "rider.0.position present");
    check(snap.fields.size() == std::size(rr::sim::rider_fields), "one rider, full field row");
    race.set_rng_state(1234);
    check(race.rng_state() == 1234, "rng state round trip");
    // Synthetic green-anchor replay (replay-harness correction Oct 12):
    // exactly one random call at green and one later. A human-driven
    // player (demo hold clear) consumes no draws on an empty field, so
    // arming the steal path (fists vs a weapon-carrying neighbour in
    // reach) produces exactly one (random()&3)==3 gate draw at green and
    // one more on a later tick; both the MSVC stream values and the
    // field states are compared.
    {
        auto draw = [](uint32_t& s) { s = s * 0x343fdu + 0x269ec3u; return (s >> 16) & 0x7fff; };
        uint32_t s = 777;  // both first draws have (&3)==3 (verified below)
        const uint32_t d1 = draw(s), s1 = s, d2 = draw(s), s2 = s;
        check((d1 & 3) == 3 && (d2 & 3) == 3, "seed 777 draws hit the steal gate twice");
        // Player (fists) + club-carrying neighbour parked alongside right.
        rr::BikeSpec pspec{};
        std::vector<rr::Opponent> field;
        field.push_back({pspec, 90, 0, 0, 0, 1, 1, 0x3200, 16384 + 1000, false, 100, {}, 1});
        rr::Race g(straight(200), straight(200), rr::PhysicsTables::synthetic(), pspec, 90,
                   field, 0, 0, {}, {});
        g.riders()[0].weapon = 0;
        g.set_rng_state(777);  // single anchor at green
        check(g.rng_state() == 777, "anchor holds before green");
        // Green: punch starts the attack (no draw yet); the hit lands on
        // the 7th tick with exactly one draw, stealing the club (d1&3==3).
        for (int t = 0; t < 7; ++t) g.step(rr::control_punch);
        const auto green_hit = g.snapshot();
        check(green_hit.rng == s1, "exactly one rng call at green");
        bool stole = false;
        for (const auto& f : green_hit.fields)
            if (f.name == "rider.0.weapon" && f.value == 1) stole = true;
        check(stole, "green steal transfers the club (field state)");
        // Attack cooldown (18 ticks) then a second punch: one more draw.
        // The player now carries the stolen club, so re-arm fists first
        // and give the neighbour a chain; the second steal lands the same way.
        for (int t = 0; t < 18; ++t) g.step(0);
        g.riders()[0].weapon = 0;  // fists again
        g.riders()[1].weapon = 2;  // re-arm the neighbour with a chain
        for (int t = 0; t < 7; ++t) g.step(rr::control_punch);
        const auto late_hit = g.snapshot();
        check(late_hit.rng == s2, "exactly one more rng call later");
        bool stole2 = false;
        for (const auto& f : late_hit.fields)
            if (f.name == "rider.0.weapon" && f.value == 2) stole2 = true;
        check(stole2, "later steal transfers the chain (field state)");
        check(late_hit.tick == 7 + 18 + 7, "absolute tick counts every step from green");
    }

    if (failures) return 1;
    std::cout << "sim field tables agree\n";
    return 0;
}
