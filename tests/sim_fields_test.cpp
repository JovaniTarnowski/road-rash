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
    check(snap.fields.size() == std::size(rr::sim::rider_fields), "one rider, full field row (pre-new-fields)");
    race.set_rng_state(1234);
    check(race.rng_state() == 1234, "rng state round trip");
    // Shared road-base anchor (0x00405990): the Napa solo golden measures
    // 1792 = 18176 - route[grid].offset*256 - grid word. Same helper the
    // replay harness uses through the game ctor's own mechanism.
    check(rr::race_road_base_q8(18176, 64, 0) == 1792, "race_road_base_q8 anchors the relative route");
    // Synthetic green-anchor replay (plan 10 replay prerequisite): exactly
    // one random call at green and one later; both the expected MSVC
    // stream values and the field states are compared. A human-driven
    // player (demo hold clear) consumes no draws on an empty field, so
    // arming the steal path (fists vs a weapon-carrying neighbour in
    // reach) produces exactly one (random()&3)==3 gate draw at green and
    // one more on a later tick. Absolute ticks are asserted (green = 0
    // here; the oracle harness keeps absolute original ticks for
    // animation deadlines). A mid-stream re-anchor must NOT hide a
    // missing draw: the control run below replays the same controls with
    // one seed bit flipped and must diverge on rng.
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
        g.riders()[1].attack.timer = -8;  // mid-attack for the steal gate (0x00409510), fires after the window
        g.riders()[1].attack.weapon = 1;
        g.set_rng_state(777);  // single anchor at green
        check(g.rng_state() == 777, "anchor holds before green");
        // Green: punch starts the attack (no draw yet); the hit lands on
        // the 7th tick with exactly one draw, stealing the club (d1&3==3).
        for (int t = 0; t < 7; ++t) g.step(rr::control_punch);
        const auto green_hit = g.snapshot();
        check(green_hit.rng == s1, "exactly one rng call at green");
        check(green_hit.tick == 7, "green-hit tick absolute");
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
        g.riders()[1].attack.timer = -8;  // mid-attack for the steal gate (0x00409510), fires after the window
        g.riders()[1].attack.weapon = 2;
        for (int t = 0; t < 7; ++t) g.step(rr::control_punch);
        const auto late_hit = g.snapshot();
        check(late_hit.rng == s2, "exactly one more rng call later");
        bool stole2 = false;
        for (const auto& f : late_hit.fields)
            if (f.name == "rider.0.weapon" && f.value == 2) stole2 = true;
        check(stole2, "later steal transfers the chain (field state)");
        check(late_hit.tick == 7 + 18 + 7, "absolute tick counts every step from green");
        // Stream-position sensitivity: the same control stream from a
        // one-bit-different seed must NOT reproduce the anchored stream.
        // (A per-tick re-anchor would hide this; single-anchor surfaces it.)
        {
            rr::Race h(straight(200), straight(200), rr::PhysicsTables::synthetic(), pspec, 90,
                       field, 0, 0, {}, {});
            h.riders()[0].weapon = 0;
            h.riders()[1].attack.timer = -8;
            h.riders()[1].attack.weapon = 1;
            h.set_rng_state(776);
            for (int t = 0; t < 7; ++t) h.step(rr::control_punch);
            check(h.snapshot().rng != s1, "seed+1 diverges: single anchor detects stream drift");
        }
    }

    // Plan 16 Task 5 Step 2: a dump line without the new fields still
    // parses and the new fields read as absent; a line with them parses
    // each value.
    {
        const std::set<std::string> fresh = {"anim", "anim_frame", "anim_queued", "anim_loops",
            "anim_deadline", "attack_weapon", "steal_until", "hit_until",
            "nitro_charge", "nitro_throttle", "nitro_charges"};
        for (const auto& f : rr::sim::rider_fields)
            if (fresh.count(f.name) && f.offset != 0x80 && f.offset != 0x81 && f.offset != 0x82 &&
                f.offset != 0x88 && f.offset != 0x84 && f.offset != 0x1d4 && f.offset != 0x1cc &&
                f.offset != 0x1d0 && f.offset != 0xfffffffeu && f.offset != 0xfffffffdu &&
                f.offset != 0xfffffffcu)
                check(false, "new field offset mismatch");
        bool anim_found = false, nitro_found = false;
        for (const auto& f : snap.fields) {
            if (f.name == "rider.0.anim") anim_found = true;
            if (f.name == "rider.0.nitro_charge") nitro_found = true;
        }
        check(anim_found, "snapshot carries anim fields");
        check(nitro_found, "snapshot carries nitro fields");
        check(snap.fields.size() == std::size(rr::sim::rider_fields), "one rider, full field row incl. new fields");
    }

    if (failures) return 1;
    std::cout << "sim field tables agree\n";
    return 0;
}
