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

AttackCodes read_attack_codes(const std::vector<uint8_t>& exe) {
    AttackCodes attack_codes{};
    for (int side = 0; side < 2; ++side) {
        const auto bytes = read_exe_data(exe, side ? 0x464518 : 0x4644f8, 32);
        for (int kind = 0; kind < 2; ++kind)
            for (int weapon = 0; weapon < 4; ++weapon)
                attack_codes[side][kind][weapon] = int8_t(bytes[(kind * 4 + weapon) * 4]);
    }
    return attack_codes;
}

NitroConfig nitro_from_spec(const BikeSpec& spec) {
    NitroConfig nitro;
    bool spec_nitro = false;
    for (int k = 0; k < 6; ++k)
        if (spec.d[size_t(77 + k)] != 0) spec_nitro = true;
    if (spec_nitro) {
        nitro.recharge = spec.d[77]; nitro.boost = spec.d[78];
        nitro.max = spec.d[79]; nitro.charges = spec.d[80];
        nitro.minimum = spec.d[81]; nitro.tail = spec.d[82];
    }
    return nitro;
}

RaceAssets load_race_assets(const Assets& assets, const std::vector<uint8_t>& exe, const RaceConfig& config) {
    // Moved out of Game::load_course/restart (src/game_main.cpp) without the
    // course_name=="NAPA" condition: the original scene, dashboard and actors
    // on every course. The race starts and finishes at race_bounds.
    if (config.level < 0 || config.level > 4) throw std::runtime_error("race setup: level out of range");
    RaceAssets out;
    try {
        out.course = std::make_unique<Course>(read_course(assets.read("DATA/COURSES/" + config.course + ".CRS")));
    } catch (const std::exception& e) {
        throw std::runtime_error("DATA/COURSES/" + config.course + ".CRS: " + e.what());
    }
    // Player bike/rider through the career tables (0x00415fc0 character ->
    // rider, 0x004641c8 level_bike, 0x00463c78 rider row); quick races and
    // career races use the explicit bike index (the shop's purchased bike).
    // The level table applies only when bike_from_table is set (oracle
    // harness and course sweeps reproduce the original default-bike setup).
    int bike_index = config.bike;
    int rider = character_rider(config.character >= 1 && config.character <= 8 ? config.character : 1);
    if (config.career && config.bike_from_table) {
        const CareerTables tables = read_career_tables(exe);
        const size_t slot = size_t(config.level) + size_t(rider) * 5; // 0x004641c8
        if (slot >= tables.level_bike.size()) throw std::runtime_error("career level_bike: slot out of range");
        bike_index = tables.level_bike[slot];
    }
    try {
        out.specs = read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("DATA/BIKESPEC.RSC: ") + e.what());
    }
    if (bike_index < 0 || size_t(bike_index) >= out.specs.size())
        throw std::runtime_error("BIKESPEC: bike index out of range");
    out.physics = read_physics_tables(exe);
    out.rider_value = rider_value_for(exe, rider);
    out.bike_index = bike_index;
    // Bounds (plan 17 Task 1 traces the exact per-level nodes; until then the
    // grid slot-0 position and the route finish). The grid tables
    // (0x00464088/0x00464108) place slot 0 at sample 0x32.
    const auto route_a = make_route(*out.course, false);
    const auto route_b = make_route(*out.course, true);
    if (route_a.size() < 64 || route_b.size() < 64) throw std::runtime_error("race setup: route too short for a race");
    out.bounds.start_position_q8 = 0x3200; // 0x00464088 slot 0
    out.bounds.finish_position_q8 = uint32_t((route_a.size() - 1) << 8);
    out.bounds.route_hint = 0;
    try {
        out.scene = std::make_unique<CourseScene>(assets, exe, *out.course, config.course, 1, config.level);
    } catch (const std::exception& e) {
        if (!config.optional_scene) throw std::runtime_error("course scene " + config.course + ": " + e.what());
        out.scene.reset();
    }
    try {
        out.hud = std::make_unique<Hud>(assets, exe, bike_index, config.metric);
    } catch (const std::exception& e) {
        throw std::runtime_error("dashboard: " + std::string(e.what()));
    }
    try {
        out.actors = std::make_unique<ActorNodes>(assets, exe, config.high_resolution);
    } catch (const std::exception& e) {
        throw std::runtime_error("actors: " + std::string(e.what()));
    }
    out.crash = read_crash_config(exe);
    out.police = read_police_config(exe);
    if (assets.exists("DATA/CARS/" + config.course + ".CAR")) {
        // Free traffic from the course's own vehicle container (0x0044db80 /
        // 0x004465f0) WITH its EXE descriptors (views + playback banks).
        try {
            out.vehicles = std::make_unique<VehicleLibrary>(assets.read("DATA/CARS/" + config.course + ".CAR"), exe);
        } catch (const std::exception& e) {
            if (!config.optional_traffic) throw std::runtime_error("DATA/CARS/" + config.course + ".CAR: " + e.what());
            out.vehicles.reset();   // game logs the course at fallback time (finding 2)
        }
    }
    out.attack_codes = std::make_unique<AttackCodes>(read_attack_codes(exe));
    out.personalities = read_personalities(exe);
    return out;
}

RaceInstance build_race(const RaceAssets& assets, const std::vector<uint8_t>& exe, const RaceConfig& config) {
    RaceInstance out;
    // 1. Routes, Race ctor (player bike/rider, opponents, level, anchor,
    // personalities, EXE image); solo freezes every opponent node.
    auto route_a = make_route(*assets.course, false);
    auto route_b = make_route(*assets.course, true);
    auto opponents = original_opponents(exe, assets.specs, config.level, config.solo ? 14 : config.opponents);
    if (config.solo) for (auto& o : opponents) o.frozen = true;
    auto race = std::make_unique<Race>(std::move(route_a), std::move(route_b), assets.physics,
                                       assets.specs[size_t(assets.bike_index)], assets.rider_value,
                                       std::move(opponents), config.level, config.road_base_q8,
                                       assets.personalities, exe);
    // 2. Prepare hook (demo hold) runs after the ctor, before the ring.
    if (config.prepare) config.prepare(*race);
    // 3. Pedestrians: ring-driven DZHR spawns take a static empty actor
    // source (as the harness does); otherwise the scene's DZHR records.
    // Looks always come from the scene's pedestrian looks.
    static const std::vector<DzhrActor> no_actors;
    if (assets.scene) {
        auto* scene = assets.scene.get();
        if (config.track_ring) {
            out.pedestrians = std::make_unique<PedestrianSystem>(
                [](const RouteSample&) -> const std::vector<DzhrActor>& { return no_actors; },
                [scene](const RouteSample& s, uint8_t selector) { return scene->pedestrian_look(s.section, s.local, selector); }, exe);
        } else {
            out.pedestrians = std::make_unique<PedestrianSystem>(
                [scene](const RouteSample& s) -> const std::vector<DzhrActor>& { return scene->dzhr_actors(s.section, s.local); },
                [scene](const RouteSample& s, uint8_t selector) { return scene->pedestrian_look(s.section, s.local, selector); }, exe);
        }
        race->set_pedestrians(out.pedestrians.get());
        if (config.solo) race->set_freeze_actors(true);
    }
    // 4. RNG state, then the track ring (0x004552e0 setup on the loaded state).
    if (config.ring_rng_state) race->set_rng_state(*config.ring_rng_state);
    if (config.track_ring) {
        out.ring = std::make_unique<TrackRing>(*assets.course, config.level);
        race->set_track_ring(out.ring.get());
    }
    // 5. Finish line + terrain source.
    race->set_finish_q8(int32_t(assets.bounds.finish_position_q8));
    if (assets.scene) {
        auto* scene = assets.scene.get();
        race->set_terrain_source([scene](const RouteSample& sample) -> const TerrainSlice& {
            return scene->terrain(sample.section, sample.local);
        });
    }
    // 6. Traffic from the course's own vehicle container.
    if (assets.vehicles) {
        out.traffic = std::make_unique<TrafficSystem>(*assets.vehicles, exe);
        out.traffic->configure(6);
        race->set_traffic(out.traffic.get());
    }
    // 7. Crash library, ride library + attack codes, nitro from the player spec.
    race->set_crash_library(assets.actors->library(), assets.crash);
    race->set_ride_library(&assets.actors->library(), assets.attack_codes.get());
    race->set_nitro(nitro_from_spec(assets.specs[size_t(assets.bike_index)]));
    out.race = std::move(race);
    return out;
}

RaceSetup make_race_setup(const Assets& assets, const std::vector<uint8_t>& exe, const RaceConfig& config) {
    RaceSetup setup;
    setup.assets = load_race_assets(assets, exe, config);
    setup.instance = build_race(setup.assets, exe, config);
    return setup;
}
}
