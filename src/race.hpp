#pragma once
#include "bike.hpp"
#include "road.hpp"

namespace rr {
// Original timing: 0x004163d0 steps the simulation at QueryPerformanceFrequency/0x3c,
// at most 0x14 steps per frame.
constexpr int ticks_per_second = 60;
constexpr int max_steps_per_frame = 20;

// An AI opponent (bike + rider pair built by 0x00405990 with the AI tables).
struct Opponent {
    BikeSpec spec;
    int32_t rider_value;   // rider table +0xc / 2
    int32_t base_speed;    // personality +0x1bc + level*4 (units of (v>>12)+(v>>7)+(v>>9))
    int lane;              // personality lane preference (+0x18c): 0, 1, 2
    int slot;              // grid slot 0..7 (0x00464218)
    int32_t throttle_percent = 100;   // 0x004642a0[level]
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
    size_t sample = 0, previous_sample = 0; // bike +0x140 / +0x13c
    bool ai = false;
    AiState brain;
    bool finished = false;
    int32_t finish_tick = -1;
    int32_t crash_timer = 0;
    int colour = 0;                         // rider colour remap index (0x0046e158)
};

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
    void step(uint32_t controls);      // one simulation tick
    const std::vector<Rider>& riders() const { return riders_; }
    const Bike& bike() const { return riders_[0].bike; }
    uint32_t position() const { return uint32_t(bike().i32(0x1c)); }
    int32_t lateral() const { return bike().i32(0x28) >> 8; }
    const std::vector<RouteSample>& route() const { return route_of(riders_[0]); }
    const std::vector<RouteSample>& route_of(const Rider& r) const { return routes_[r.route]; }
    bool finished() const { return riders_[0].finished; }
    bool crashed() const { return bike().i32(0x2f4) != 0; }
    int32_t ticks() const { return ticks_; }
    uint32_t camera_position() const;
    Camera camera() const;              // relative to the route's own road at camera_position()
    int32_t speed() const { return bike().i32(0xec); }
    int gear() const { return bike().i32(0x23c); }
    int place(size_t rider) const;      // 1-based race order (finished riders by finish tick)
private:
    SliceEdges edges(const Rider&, size_t sample) const;
    void move_along(Rider&, int32_t distance, bool accumulate_sweep = true);
    void cross_sample(Rider&, int32_t delta);
    void accumulate(Rider&, uint32_t from, uint32_t to, size_t from_sample, size_t to_sample);
    void move(Rider&, bool player);
    void vertical(Rider&);
    void walls(Rider&);
    void crash(Rider&);
    void step_rider(Rider&, uint32_t controls, bool player);
    void ai_think(Rider&);
    int32_t curvature(const Rider&, size_t sample, int samples) const;
    int32_t ai_speed_limit(const Rider&) const;
    void collide(Rider&, Rider&);
    void contact(Rider& self, Rider& other);
    void update_camera();

    std::vector<RouteSample> routes_[2];
    PhysicsTables tables_;
    std::vector<Rider> riders_;
    int32_t ticks_ = 0;
    int level_ = 0;
    int32_t camera_x_ = 0, camera_height_ = 150, camera_lift_ = 0, camera_back_ = 0;
};
}
