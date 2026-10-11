#pragma once
#include "bike.hpp"
#include "crash.hpp"
#include "actor_descriptors.hpp"
#include "road.hpp"
#include "sound_event.hpp"
#include <vector>
#include <functional>
#include <string>

namespace rr {
class TrafficSystem;
// Original timing: 0x004163d0 steps the simulation at QueryPerformanceFrequency/0x3c,
// at most 0x14 steps per frame.
constexpr int ticks_per_second = 60;
constexpr int max_steps_per_frame = 20;

// Rider fight stats: rider table 0x00463c78 words 12..15 (+0x30 strength, +0x34 base stamina,
// +0x38 maximum, +0x3c current). Hits lower maximum by damage*4 (not below base/2) and current
// by damage*64; current regenerates one point per tick up to maximum (0x00407d40).
struct RiderStats { int32_t strength = 7, base = 2500, max = 2500, current = 2500; };

// Weapons (rider +0x1d4): 0 fists, 1 club, 2 chain, 3 kick. Reach in units of 0x1000 Q8
// (0x00409c10) and damage multipliers (0x00465a00).
constexpr int32_t weapon_reach[4] = {5, 7, 8, 5};
constexpr int32_t weapon_damage[4] = {256, 320, 384, 64};

struct Attack {
    int timer = 0;      // 0 idle; counts ticks while attacking
    int weapon = 0;
    bool right = false; // toward larger lateral (0x004095a0)
    bool backhand = false;
};
constexpr int attack_hit_tick = 7, attack_ticks = 18;   // animation timing stand-in

// An AI opponent (bike + rider pair built by 0x00405990 with the AI tables).
struct Opponent {
    BikeSpec spec;
    int32_t rider_value;   // rider table +0xc / 2
    int32_t base_speed;    // personality +0x1bc + level*4 (units of (v>>12)+(v>>7)+(v>>9))
    int lane;              // personality lane preference (+0x18c): 0, 1, 2
    int slot;              // grid slot 0..7 (0x00464218)
    int32_t throttle_percent = 100;   // 0x004642a0[level]
    RiderStats stats{};
    int weapon = 0;                   // rider table word 6 + level
};

// The original opponents for a level: personalities 1..count (0x00464558, 0x204 bytes each),
// bikes from 0x004641c8[level + rider*5], rider values from 0x00463c78 (+0xc / 2), AI throttle
// scale 0x004642a0[level]; grid slots 1..count.
std::vector<Opponent> original_opponents(const std::vector<uint8_t>& exe, const std::vector<BikeSpec>& specs,
                                         int level, int count);

struct AiState {
    int32_t target_x = 0;        // +0x194 (world units * 256)
    int32_t target_position = 0; // +0x198 (Q8 samples)
    int32_t target_speed = 0;    // +0x19c
    int32_t current_speed = 0;   // +0x1a4
    int32_t base_speed = 0;      // +0x1bc + level*4
    int32_t replan_ticks = 0;    // +0x188
    int lane = 0;                // +0x18c
    int32_t bias = 0;            // +0x190
};

struct Rider {
    Bike bike;
    int route = 0;                          // which branch route (0 = half 0)
    int slot = 0;                           // grid slot 0..7 (0x00464218); 0 is the player
    size_t sample = 0, previous_sample = 0; // bike +0x140 / +0x13c
    bool ai = false;
    AiState brain;
    bool finished = false;
    int32_t finish_tick = -1;
    CrashActor crash_actor;
    ActorAnimationState crash_bike_animation;
    bool crash_active=false;
    int32_t crash_timer = 0;
    int colour = 0;                         // rider colour remap index (0x0046e158)
    RiderStats stamina;
    int weapon = 0;                         // carried weapon (+0x1c8)
    Attack attack;
    int32_t invulnerable_until = 0;         // +0x1d0: no further hits before this tick
};

// Per-tick snapshot for the simulation oracle (plan 10). Rider fields are
// keyed "rider.<grid slot>.<field>" from tests/sim_fields.hpp; the tick and
// rng come first. Traffic cars are appended by TrafficSystem.
struct FieldValue { std::string name; int64_t value; };
struct RaceSnapshot { int32_t tick; uint32_t rng; std::vector<FieldValue> fields; };
// Riders on a course: bike physics (bike.hpp), movement along the route
// (0x004395e0/0x00439270/0x00439390/0x004394c0), ground height and vertical motion
// (0x00439b10 case 0, 0x0043a190), branch choice at splits, AI racing (0x00407bd0 state 1)
// and the chase camera (0x00437c30) on rider 0, the player.
class Race {
public:
    // route_a/route_b: make_route(course,false/true). Identical up to the first split.
    Race(std::vector<RouteSample> route_a, std::vector<RouteSample> route_b,
         const PhysicsTables&, const BikeSpec&, int32_t rider_value, std::vector<Opponent> opponents = {},
         int level = 0);   // level 0..4 (0x004b8a12)
    void set_terrain_source(std::function<const TerrainSlice&(const RouteSample&)> source) { terrain_source_=std::move(source); }
    // Free traffic steps with the race on the player's route (0x0044b3b0 calls
    // 0x0044dd90 with the per-step camera advance). The random-call
    // interleaving against the original's scheduler is not verified.
    void set_traffic(TrafficSystem* traffic) { traffic_ = traffic; }
    void set_crash_library(const ActorAnimationLibrary&,const CrashConfig&);
    void step(uint32_t controls);      // one simulation tick
    const std::vector<Rider>& riders() const { return riders_; }
    const Bike& bike() const { return riders_[0].bike; }
    uint32_t position() const { return uint32_t(bike().i32(0x1c)); }
    int32_t lateral() const { return bike().i32(0x28) >> 8; }
    const std::vector<RouteSample>& route() const { return route_of(riders_[0]); }
    const std::vector<RouteSample>& route_of(const Rider& r) const { return routes_[r.route]; }
    const std::vector<RouteSample>& route_for(int road) const { return routes_[road]; }
    bool finished() const { return riders_[0].finished; }
    bool crashed() const { return bike().i32(0x2f4) != 0; }
    int32_t ticks() const { return ticks_; }
    uint32_t camera_position() const;
    int32_t horizon_scroll() const { return rr::horizon_scroll(camera_heading_); }
    int32_t camera_x() const { return camera_x_; }   // 0x004b7ee8: camera lateral, world units
    Camera camera() const;              // relative to the route's own road at camera_position()
    int32_t speed() const { return bike().i32(0xec); }
    int gear() const { return bike().i32(0x23c); }
    // Simulation oracle hooks (plan 10): read and write the MSVC rand state
    // (0x00455eb0) and dump every snapshotted field.
    uint32_t rng_state() const { return seed_; }
    void set_rng_state(uint32_t s) { seed_ = s; }
    RaceSnapshot snapshot() const;
    int place(size_t rider) const;      // 1-based race order (finished riders by finish tick)
    int last_target() const { return last_target_; }   // rider most recently hit by the player
    // Sound events queued this tick (0x00436830 call sites that involve the player).
    std::vector<SoundEventRecord> take_sound_events() { auto e = std::move(sound_events_); sound_events_.clear(); return e; }
    bool wrecked() const { return bike().i32(0x2f0) == 0; }   // bike condition used up
private:
    SliceEdges edges(const Rider&, size_t sample) const;
    void move_along(Rider&, int32_t distance, bool accumulate_sweep = true);
    void cross_sample(Rider&, int32_t delta);
    void accumulate(Rider&, uint32_t from, uint32_t to, size_t from_sample, size_t to_sample);
    void move(Rider&, bool player);
    void vertical(Rider&);
    void walls(Rider&);
    void crash(Rider&);
    void step_crash(Rider&,bool player,uint32_t controls);
    void step_rider(Rider&, uint32_t controls, bool player);
    void ai_think(Rider&);
    int32_t curvature(const Rider&, size_t sample, int samples) const;
    int32_t ai_speed_limit(const Rider&) const;
    void collide(Rider&, Rider&);
    void contact(Rider& self, Rider& other);
    void update_camera();
    void fight(Rider&, uint32_t controls, size_t index);
    void resolve_hit(Rider& attacker, size_t index);
    uint32_t random();

    std::function<const TerrainSlice&(const RouteSample&)> terrain_source_;
    TrafficSystem* traffic_ = nullptr;
    const ActorAnimationLibrary* crash_library_=nullptr;
    CrashConfig crash_config_;
    std::vector<RouteSample> routes_[2];
    PhysicsTables tables_;
    std::vector<Rider> riders_;
    int32_t ticks_ = 0;
    int level_ = 0;
    int last_target_ = -1;
    uint32_t seed_ = 1;   // MSVC rand() state (0x00455eb0)
    std::vector<SoundEventRecord> sound_events_;
    void sound(SoundEvent, const Bike&);
    uint32_t previous_camera_position_=0;
    int32_t camera_heading_=0;
    int32_t camera_x_ = 0, camera_height_ = 150, camera_lift_ = 0, camera_back_ = 0;
};
}
