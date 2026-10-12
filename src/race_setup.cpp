#include "race_setup.hpp"
#include "bike.hpp"
#include "career.hpp"
#include "exe_tables.hpp"
#include <stdexcept>

namespace rr {
namespace {
// 0x00463c78 rider-table row (0x40 bytes): weight +0xc, weapon base +0x18,
// fight stats +0x30..+0x3c. Returns the halved weight (rider value).
int32_t rider_value_for(const std::vector<uint8_t>& exe, int rider) {
    if (rider < 0 || rider > 9) throw std::runtime_error("Rider index out of range");
    const auto body = read_exe_data(exe, 0x00463c78 + uint32_t(rider) * 0x40 + 0xc, 4); // 0x00463c78
    return int32_t(uint32_t(body[0]) | uint32_t(body[1]) << 8) / 2;
}
}

RaceSetup make_race_setup(const Assets& assets, const std::vector<uint8_t>& exe, const RaceConfig& config) {
    // Moved out of Game::load_course/restart (src/game_main.cpp) without the
    // course_name=="NAPA" condition: the original scene, dashboard and actors
    // on every course. The race starts and finishes at race_bounds.
    if (config.level < 0 || config.level > 4) throw std::runtime_error("race setup: level out of range");
    RaceSetup setup;
    try {
        setup.course = read_course(assets.read("DATA/COURSES/" + config.course + ".CRS"));
    } catch (const std::exception& e) {
        throw std::runtime_error("DATA/COURSES/" + config.course + ".CRS: " + e.what());
    }
    // Player bike/rider through the career tables (0x00415fc0 character ->
    // rider, 0x004641c8 level_bike, 0x00463c78 rider row); quick races use the
    // explicit bike index instead.
    int bike_index = config.bike;
    int rider = character_rider(config.character >= 1 && config.character <= 8 ? config.character : 1);
    if (config.career) {
        const CareerTables tables = read_career_tables(exe);
        const size_t slot = size_t(config.level) + size_t(rider) * 5; // 0x004641c8
        if (slot >= tables.level_bike.size()) throw std::runtime_error("career level_bike: slot out of range");
        bike_index = tables.level_bike[slot];
    }
    std::vector<BikeSpec> specs;
    try {
        specs = read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("DATA/BIKESPEC.RSC: ") + e.what());
    }
    if (bike_index < 0 || size_t(bike_index) >= specs.size())
        throw std::runtime_error("BIKESPEC: bike index out of range");
    const PhysicsTables physics = read_physics_tables(exe);
    const int32_t rider_value = rider_value_for(exe, rider);
    // Bounds (plan 17 Task 1 traces the exact per-level nodes; until then the
    // grid slot-0 position and the route finish). The grid tables
    // (0x00464088/0x00464108) place slot 0 at sample 0x32.
    const auto route_a = make_route(setup.course, false);
    const auto route_b = make_route(setup.course, true);
    if (route_a.size() < 64 || route_b.size() < 64) throw std::runtime_error("race setup: route too short for a race");
    setup.bounds.start_position_q8 = 0x3200; // 0x00464088 slot 0
    setup.bounds.finish_position_q8 = uint32_t((route_a.size() - 1) << 8);
    setup.bounds.route_hint = 0;
    try {
        setup.scene = std::make_unique<CourseScene>(assets, exe, setup.course, config.course, 1, config.level);
    } catch (const std::exception& e) {
        throw std::runtime_error("course scene " + config.course + ": " + e.what());
    }
    try {
        setup.hud = std::make_unique<Hud>(assets, exe, bike_index, config.metric);
    } catch (const std::exception& e) {
        throw std::runtime_error("dashboard: " + std::string(e.what()));
    }
    try {
        setup.actors = std::make_unique<ActorNodes>(assets, exe, config.high_resolution);
    } catch (const std::exception& e) {
        throw std::runtime_error("actors: " + std::string(e.what()));
    }
    setup.crash = read_crash_config(exe);
    setup.police = read_police_config(exe);
    if (assets.exists("DATA/CARS/" + config.course + ".CAR")) {
        // Free traffic from the course's own vehicle container (0x0044db80 /
        // 0x004465f0) WITH its EXE descriptors (views + playback banks).
        try {
            setup.vehicles = std::make_unique<VehicleLibrary>(assets.read("DATA/CARS/" + config.course + ".CAR"), exe);
            setup.traffic = std::make_unique<TrafficSystem>(*setup.vehicles, exe);
            setup.traffic->configure(6);
        } catch (const std::exception& e) {
            throw std::runtime_error("DATA/CARS/" + config.course + ".CAR: " + e.what());
        }
    }
    auto opponents = original_opponents(exe, specs, config.level, config.opponents);
    auto personalities = read_personalities(exe);
    setup.race = std::make_unique<Race>(std::move(route_a), std::move(route_b), physics,
                                        specs[size_t(bike_index)], rider_value,
                                        std::move(opponents), config.level, 0,
                                        personalities, exe);
    setup.race->set_finish_q8(int32_t(setup.bounds.finish_position_q8));
    setup.race->set_terrain_source([scene = setup.scene.get()](const RouteSample& sample) -> const TerrainSlice& {
        return scene->terrain(sample.section, sample.local);
    });
    if (setup.traffic) setup.race->set_traffic(setup.traffic.get());
    setup.race->set_crash_library(setup.actors->library(), setup.crash);
    return setup;
}
}
