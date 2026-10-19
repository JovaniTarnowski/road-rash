#pragma once
// Stable race-setup factory shared by the game and the replay harness
// (plan 10 Task 4 specified this factory but it was never extracted;
// plan 17 Task 2 creates it here with plan 10's names, extended;
// plan 26 Task 1 splits it into course-scoped load_race_assets and
// race-scoped build_race so the game, race-soak and the oracle-sim replay
// all build races through one factory).
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
#include "pedestrians.hpp"
#include "race.hpp"
#include "track_ring.hpp"
#include "traffic.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rr {
using AttackCodes = std::array<std::array<std::array<int8_t,4>,2>,2>;
AttackCodes read_attack_codes(const std::vector<uint8_t>& exe);   // 0x004644f8 / 0x00464518, 8 words each
NitroConfig nitro_from_spec(const BikeSpec&);                     // words 77..82 (+0x14c); all zero -> NitroConfig{}

struct RaceConfig {
    std::string course = "NAPA"; // original file name: "CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"
    int level = 0;               // 0..4 (DAT_004b8a12)
    int bike = 0;                // quick-race bike index; career's purchased bike (shop)
    bool bike_from_table = false;  // true: career level table overrides bike (oracle/sweeps only)
    int character = 0;           // 1..8 front-end character (0x00415fc0); career only
    int player_rider = -1;       // [0..9] row in 0x00463c78; -1 means character_rider(character)
    bool career = true;
    int opponents = 7;           // 0..7, for quick races and solo tests
    bool high_resolution = true; // Settings::high_resolution (plan 18 Task 4)
    bool metric = false;
    bool solo = false;                         // freeze opponents + scheduler actors (oracle)
    bool live_schedule = true;                 // live rider scheduler 0x00438170 (Race::set_live_schedule)
    int pedestrian_setup_tick = 2;             // Race::set_pedestrian_setup_tick
    int32_t road_base_q8 = 0;                  // Race ctor anchor (game 0; replay measured)
    bool track_ring = true;                    // ring-driven DZHR pedestrians (0x004552e0)
    bool optional_scene = false;               // plan 26 Task 3: game fallback keeps a null scene
    bool optional_traffic = false;             // finding 2 (2026-10-13): bad .CAR keeps null vehicles
    std::optional<uint32_t> ring_rng_state;    // RNG state loaded just before ring setup
    uint32_t ring_setup_tick = 2;              // TrafficSystem::set_setup_tick
    // +0x1bc level speeds of rider rows 9..14 (Career::spare_levels, jittered at launch by
    // 0x004078f0 from the time-seeded stream); unset keeps personality 0's unjittered words.
    std::optional<std::array<std::array<int32_t, 5>, 6>> spare_levels;
    std::function<void(Race&)> prepare;        // runs after ctor, before ring (demo hold)
};
// Original per-course/per-level start and finish (plan 17 Task 1 traces the
// exact nodes; until then the grid slot-0 position and the route finish).
// Throws std::runtime_error if the tables are out of range.
struct RaceBounds {
    uint32_t start_position_q8 = 0;
    uint32_t finish_position_q8 = 0;
    int route_hint = 0;
};
struct RaceAssets {             // course-scoped: loaded once per course
    // Heap-owned: TrackRing holds a const Course& into it, so the Course
    // itself must not move when a RaceSetup moves (the spec test moves the
    // setup and steps the race). Deviation from the spec's `Course course;`
    // value member, required by Review Focus 1; consumers use course-> / *course.
    std::unique_ptr<Course> course;
    std::unique_ptr<CourseScene> scene;
    std::unique_ptr<Hud> hud;
    std::unique_ptr<ActorNodes> actors;
    std::unique_ptr<VehicleLibrary> vehicles;  // null when the course has no .CAR
    std::unique_ptr<AttackCodes> attack_codes;
    CrashConfig crash;
    PoliceConfig police;
    RaceBounds bounds;
    PhysicsTables physics;
    std::vector<BikeSpec> specs;
    std::vector<Personality> personalities;
    int bike_index = 0;
    int32_t rider_value = 0;
};
struct RaceInstance {           // race-scoped: rebuilt on restart
    std::unique_ptr<TrafficSystem> traffic;
    std::unique_ptr<PedestrianSystem> pedestrians;
    std::unique_ptr<TrackRing> ring;
    std::unique_ptr<Race> race;
};
RaceAssets load_race_assets(const Assets&, const std::vector<uint8_t>& exe, const RaceConfig&);
RaceInstance build_race(const RaceAssets&, const std::vector<uint8_t>& exe, const RaceConfig&);
struct RaceSetup { RaceAssets assets; RaceInstance instance; };
// The only production/replay constructor of Race; tests with synthetic routes may build Race directly.
RaceSetup make_race_setup(const Assets&, const std::vector<uint8_t>& exe, const RaceConfig&);  // load + build
}
