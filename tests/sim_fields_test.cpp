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

    if (failures) return 1;
    std::cout << "sim field tables agree\n";
    return 0;
}
