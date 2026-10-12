// Plan 17 Task 2 step 1: one race setup for every course (all maps
// playable). Skips 77 without the disc. For every course name and level
// 0..4, make_race_setup returns a setup with a non-null scene, HUD, actors
// and race; traffic is non-null when DATA/CARS/<name>.CAR exists; the
// race's player starts at bounds.start_position_q8. Then each race steps
// 600 ticks with zero controls and builds one frame's draw nodes; no throw.
#include "assets.hpp"
#include "golden.hpp"
#include "race_setup.hpp"
#include "road.hpp"
#include <filesystem>
#include <iostream>

int main() {
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
            if (!setup.scene) throw std::runtime_error(std::string(name) + ": null scene");
            if (!setup.hud) throw std::runtime_error(std::string(name) + ": null HUD");
            if (!setup.actors) throw std::runtime_error(std::string(name) + ": null actors");
            if (!setup.race) throw std::runtime_error(std::string(name) + ": null race");
            if (assets.exists("DATA/CARS/" + std::string(name) + ".CAR") && !setup.traffic)
                throw std::runtime_error(std::string(name) + ": null traffic with CAR present");
            if (uint32_t(setup.race->bike().i32(0x1c)) != setup.bounds.start_position_q8)
                throw std::runtime_error(std::string(name) + ": player not at bounds.start_position_q8");
            for (int t = 0; t < 600; ++t) setup.race->step(0);
            const auto slices = rr::project_road(setup.race->route(), setup.race->camera_position(),
                                                 setup.race->camera(), depths);
            const auto nodes = setup.scene->draw_nodes(setup.race->route(), slices, setup.race->camera(), depths);
            std::cout << name << " L" << level << ": 600 ticks, " << nodes.size() << " draw nodes\n";
            ++races;
        }
    }
    std::cout << races << " race setups built\n";
    return 0;
}
