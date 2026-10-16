// Plan 17 Task 2 step 1: one race setup for every course (all maps
// playable). Skips 77 without the disc. For every course name and level
// 0..4, make_race_setup returns a setup with a non-null scene, HUD, actors
// and race; traffic is non-null when DATA/CARS/<name>.CAR exists; the
// race's player starts at bounds.start_position_q8. Then each race steps
// 600 ticks with zero controls and builds one frame's draw nodes; no throw.
// Plan 26 Task 1: every hook attached (wiring report), pointers survive a
// move (ASan), and build_race gives a fresh race with no cars carried over.
#include "assets.hpp"
#include "golden.hpp"
#include "race_setup.hpp"
#include "road.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

int main() {
    int failures = 0;
    auto check = [&](bool v, const std::string& what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    const std::string assets_root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(assets_root + "/ROADRASH.EXE")) {
        std::cout << "SKIP no disc\n";
        return rr::golden::skip;
    }
    rr::Assets assets(assets_root);
    const std::vector<uint8_t> exe = assets.read("ROADRASH.EXE");
    const rr::DepthTable depths = rr::read_depth_table(exe);
    static const char* courses[] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"}; // 0x0046df58
    size_t races = 0;
    for (const char* name : courses) {
        for (int level = 0; level <= 4; ++level) {
            rr::RaceConfig config;
            config.course = name;
            config.level = level;
            config.career = false;
            config.opponents = 0; // solo: field isolation for the first sweep
            rr::RaceSetup setup = rr::make_race_setup(assets, exe, config);
            if (!setup.assets.scene) throw std::runtime_error(std::string(name) + ": null scene");
            if (!setup.assets.hud) throw std::runtime_error(std::string(name) + ": null HUD");
            if (!setup.assets.actors) throw std::runtime_error(std::string(name) + ": null actors");
            if (!setup.instance.race) throw std::runtime_error(std::string(name) + ": null race");
            if (assets.exists("DATA/CARS/" + std::string(name) + ".CAR") && !setup.instance.traffic)
                throw std::runtime_error(std::string(name) + ": null traffic with CAR present");
            if (uint32_t(setup.instance.race->bike().i32(0x1c)) != setup.assets.bounds.start_position_q8)
                throw std::runtime_error(std::string(name) + ": player not at bounds.start_position_q8");
            for (int t = 0; t < 600; ++t) setup.instance.race->step(0);
            const auto slices = rr::project_road(setup.instance.race->route(), setup.instance.race->camera_position(),
                                                 setup.instance.race->camera(), depths);
            const auto nodes = setup.assets.scene->draw_nodes(setup.instance.race->route(), slices, setup.instance.race->camera(), depths);
            std::cout << name << " L" << level << ": 600 ticks, " << nodes.size() << " draw nodes\n";
            ++races;
        }
    }
    std::cout << races << " race setups built\n";
    for (const char* course : {"NAPA", "CITY", "HIWAY", "CANYN", "MEDLY"}) {
        rr::RaceConfig config; config.course = course; config.character = 1; config.bike_from_table = true;
        rr::RaceSetup setup = rr::make_race_setup(assets, exe, config);
        const rr::RaceWiring w = setup.instance.race->wiring();
        const std::string c = course;
        check(w.terrain && w.pedestrians && w.ring && w.crash_library && w.ride_library &&
              w.nitro && w.finish && w.personalities, c + ": every hook attached");
        check(w.traffic == bool(setup.assets.vehicles), c + ": traffic iff vehicles");
        rr::RaceSetup moved = std::move(setup);           // pointers must survive a move
        for (int t = 0; t < 600; ++t) moved.instance.race->step(0x1);
        rr::RaceInstance again = rr::build_race(moved.assets, exe, config);   // restart path
        check(again.race->ticks() == 0, c + ": fresh race");
        check(!again.traffic || again.traffic->cars().empty() ||
              std::none_of(again.traffic->cars().begin(), again.traffic->cars().end(),
                           [](const auto& car) { return car.active; }), c + ": no cars carried over");
    }
    // DZHR roadside wiring (plan 16 Task 2): the factory attaches solid
    // spans from the scene's placed billboards on both roads, deterministically
    // (course-load seed is fixed, so rebuilds agree exactly).
    {
        rr::RaceConfig config;
        config.course = "NAPA";
        config.level = 1;
        config.career = false;
        config.opponents = 0;
        rr::RaceSetup first = rr::make_race_setup(assets, exe, config);
        rr::RaceSetup second = rr::make_race_setup(assets, exe, config);
        const size_t n0 = first.instance.race->roadside_solids(0) + first.instance.race->roadside_solids(1);
        const size_t n1 = second.instance.race->roadside_solids(0) + second.instance.race->roadside_solids(1);
        check(n0 > 0, "NAPA wires DZHR solids");
        check(n0 == n1, "DZHR wiring is deterministic");
    }
    // Review finding 1 (2026-10-13): career races must use the purchased bike,
    // not the level table default. career=true with an explicit bike keeps it.
    {
        rr::RaceConfig config;
        config.course = "NAPA";
        config.level = 0;
        config.character = 1;
        config.career = true;
        config.bike = 7;   // differs from the level-0 table default
        config.opponents = 0;
        rr::RaceSetup setup = rr::make_race_setup(assets, exe, config);
        check(setup.assets.bike_index == 7, "career keeps the purchased bike");
    }
    // Plan 10: the measured original player row changes mass-scaled throttle
    // ramps even when both rows choose the same bike. Keep the factory's
    // explicit identity separate from the front-end character mapping.
    {
        rr::RaceConfig config;
        config.character = 1;
        config.bike_from_table = true;
        config.opponents = 0;
        config.track_ring = false;
        config.player_rider = 0;
        rr::RaceSetup row0 = rr::make_race_setup(assets, exe, config);
        config.player_rider = 8;
        rr::RaceSetup row8 = rr::make_race_setup(assets, exe, config);
        check(row0.assets.bike_index == row8.assets.bike_index, "row 0/8 select the same bike");
        check(row0.assets.rider_value == 90 && row8.assets.rider_value == 91,
              "explicit rider row selects original halved weight");
        check(row0.instance.race->bike().i32(0x260) == 9188 &&
              row8.instance.race->bike().i32(0x260) == 9160,
              "explicit rider row controls mass-scaled throttle cap");
        check(row0.instance.race->bike().i32(0x264) == 918 &&
              row0.instance.race->bike().i32(0x268) == 612,
              "row 0 preserves original throttle ramp rates");
        config.player_rider = -1;
        rr::RaceSetup mapped = rr::make_race_setup(assets, exe, config);
        check(mapped.assets.rider_value == row8.assets.rider_value &&
              mapped.instance.race->bike().i32(0x260) == row8.instance.race->bike().i32(0x260),
              "unset player row retains character mapping");
        for (int invalid : {-2, 10}) {
            config.player_rider = invalid;
            bool rejected = false;
            try { (void)rr::load_race_assets(assets, exe, config); }
            catch (const std::runtime_error& e) {
                rejected = std::string(e.what()) == "race setup: player rider out of range";
            }
            check(rejected, "reject invalid explicit rider row " + std::to_string(invalid));
        }
    }
    if (failures == 0) std::cout << "race_setup OK\n";
    return failures ? 1 : 0;
}
