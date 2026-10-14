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
        rr::RaceConfig config; config.course = course; config.character = 1;
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
    if (failures == 0) std::cout << "race_setup OK\n";
    return failures ? 1 : 0;
}
