#include "race_setup.hpp"
#include "bike.hpp"
#include "career.hpp"
#include "exe_tables.hpp"
#include "scenery.hpp"
#include <stdexcept>

namespace rr {
namespace {
// DZHR solid-object tables (plan 16 Task 2): one span list per route
// sample, from the scene's placed billboards through the pure per-sample
// helper. Centre is the route-frame road centre in Q8 (route offset plus
// the replay anchor), the same frame rider laterals use. Consumes no
// randomness: placement already ran once at course load, so the shared
// race stream is untouched and table builds are deterministic.
std::vector<std::vector<DzhrSolid>> dzhr_table(const std::vector<RouteSample>& route,
                                               const CourseScene& scene, int32_t road_base_q8) {
    std::vector<std::vector<DzhrSolid>> table(route.size());
    for (size_t i = 0; i < route.size(); ++i) {
        const RouteSample& s = route[i];
        table[i] = dzhr_solids_for_sample(route[i].offset * 256 + road_base_q8,
                                          scene.objects(s.section, s.local));
    }
    return table;
}
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
    // 0x00405600 -> 0x0040b0e0(spec + 0x14c) copies the six words even when
    // they are all zero (no nitro: charge max 0 << 8 = 0, no charges); the
    // EXE defaults at 0x00465a28 apply only to a null spec.
    NitroConfig nitro;
    // +0x14c is spec word 83 (BIKESPEC: only the nitro bikes 7..10 carry
    // nonzero words there).
    nitro.recharge = spec.d[83]; nitro.boost = spec.d[84];
    nitro.max = spec.d[85]; nitro.charges = spec.d[86];
    nitro.minimum = spec.d[87]; nitro.tail = spec.d[88];
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
    // The original setup index is not derivable solely from the front-end
    // character: row identity lives in the bike/active rider record. Allow the
    // replay harness to supply it explicitly from recorded 0x00463c78 state.
    int bike_index = config.bike;
    int rider = config.player_rider;
    if (rider == -1)
        rider = character_rider(config.character >= 1 && config.character <= 8 ? config.character : 1);
    if (rider < 0 || rider > 9) throw std::runtime_error("race setup: player rider out of range");
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
    // Original per-course/per-level start and level-tagged finish line
    // (plan 17 Task 1, race_bounds); the route end remains only for course
    // files outside the five originals.
    out.bounds.start_position_q8 = 0x3200; // 0x00464088 slot 0
    out.bounds.finish_position_q8 = uint32_t((route_a.size() - 1) << 8);
    out.bounds.route_hint = 0;
    {
        static const char* originals[] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};   // 0x0046df58
        for (int c = 0; c < 5; ++c)
            if (config.course == originals[c]) {
                // Finish only: the grid still places slot 0 at 0x3200, so the
                // level-4 start 0x3300 waits for the grid port.
                out.bounds.finish_position_q8 = race_bounds(exe, *out.course, c + 1, config.level).finish_position_q8;
                break;
            }
    }
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
        // Live slice statics for the 0x004373f0 sweep (boxes via the scene's
        // family residency, 0x0044b630).
        if (assets.scene) {
            auto* scene = assets.scene.get();
            Race* r = race.get();
            r->set_static_frames([scene](uint32_t section, uint32_t local, uint8_t selector, bool billboard) {
                return scene->static_frame(section, local, selector, billboard);
            });
            out.ring->set_static_sink([r](int32_t sample, uint32_t section, uint32_t local, const RingStatics& statics) {
                r->ring_statics(sample, section, local, statics);
            });
        }
        // Ring slices carry the race stream's terrain (0x00451910); the
        // scene's isolated-stream values stand in only where the ring has
        // not built a slice (splits/merges, the native quick setup).
        if (assets.scene) {
            auto* scene = assets.scene.get();
            out.ring->set_slice_sink([scene](uint32_t section, uint32_t local, const PathSample& path, const TerrainSlice& slice) {
                scene->apply_ring_terrain(section, local, path, slice);
            });
        }
        race->set_track_ring(out.ring.get());
    }
    // 4b. Rider build draws (0x00405600 after the ring): lanes, name roll.
    race->build_draws();
    // 5. Finish line + terrain source.
    race->set_finish_q8(int32_t(assets.bounds.finish_position_q8));
    if (assets.scene) {
        auto* scene = assets.scene.get();
        race->set_terrain_source([scene](const RouteSample& sample) -> const TerrainSlice& {
            return scene->terrain(sample.section, sample.local);
        });
        race->set_rail_height_source([scene](const RouteSample& sample) {
            return scene->rail_height(sample.section, sample.local);
        });
        // 5b. DZHR roadside solidity (plan 16 Task 2): solid billboards
        // collide through the class-1 hazard path; BORR spans stay test-only
        // until AI avoidance lands (the autopilot cannot dodge in-road spans).
        race->set_roadside(dzhr_table(race->route_for(0), *scene, config.road_base_q8),
                           dzhr_table(race->route_for(1), *scene, config.road_base_q8));
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
