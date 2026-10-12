#pragma once
// Stable race-setup factory shared by the game and the replay harness
// (plan 10 Task 4 specified this factory but it was never extracted;
// plan 17 Task 2 creates it here with plan 10's names, extended).
// One explicit config, no hidden replay-only setup: the player bike/rider
// resolve through the career tables (0x00415fc0 character -> rider,
// 0x004641c8 level_bike, 0x00463c78 rider row), opponents through
// original_opponents, and the race starts/finishes at race_bounds.
// Every failure throws std::runtime_error naming the resource; the caller
// shows it and returns to the menu (never a crash).
#include "actor_nodes.hpp"
#include "assets.hpp"
#include "course.hpp"
#include "course_scene.hpp"
#include "crash.hpp"
#include "hud.hpp"
#include "police.hpp"
#include "race.hpp"
#include "traffic.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rr {
struct RaceConfig {
    std::string course = "NAPA"; // original file name: "CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"
    int level = 0;               // 0..4 (DAT_004b8a12)
    int bike = 0;                // quick-race bike index; ignored when career resolves it
    int character = 0;           // 1..8 front-end character (0x00415fc0); career only
    bool career = true;
    int opponents = 7;           // 0..7, for quick races and solo tests
    bool high_resolution = true; // Settings::high_resolution (plan 18 Task 4)
    bool metric = false;
};
// Original per-course/per-level start and finish (plan 17 Task 1 traces the
// exact nodes; until then the grid slot-0 position and the route finish).
// Throws std::runtime_error if the tables are out of range.
struct RaceBounds {
    uint32_t start_position_q8 = 0;
    uint32_t finish_position_q8 = 0;
    int route_hint = 0;
};
struct RaceSetup {
    Course course;
    std::unique_ptr<CourseScene> scene;
    std::unique_ptr<Hud> hud;
    std::unique_ptr<ActorNodes> actors;
    std::unique_ptr<VehicleLibrary> vehicles;
    std::unique_ptr<TrafficSystem> traffic;
    CrashConfig crash;
    PoliceConfig police;
    RaceBounds bounds;
    std::unique_ptr<Race> race;
};
// Builds everything for one race. Throws std::runtime_error naming the
// resource on failure; the caller shows it and returns to the menu
// (never a crash).
RaceSetup make_race_setup(const Assets& assets, const std::vector<uint8_t>& exe, const RaceConfig& config);
}
