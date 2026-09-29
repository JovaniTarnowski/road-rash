#pragma once
#include "bike.hpp"
#include "road.hpp"

namespace rr {
// Original timing: 0x004163d0 steps the simulation at QueryPerformanceFrequency/0x3c,
// at most 0x14 steps per frame.
constexpr int ticks_per_second = 60;
constexpr int max_steps_per_frame = 20;

// A single rider on a course: player bike physics (bike.hpp), movement along the route
// (0x004395e0/0x00439270/0x00439390/0x004394c0), ground height and vertical motion
// (0x00439b10 case 0, 0x0043a190), branch choice at splits and the chase camera (0x00437c30).
class Race {
public:
    // route_a/route_b: make_route(course,false/true). Identical up to the first split.
    Race(std::vector<RouteSample> route_a, std::vector<RouteSample> route_b,
         const PhysicsTables&, const BikeSpec&, int32_t rider_value);
    void step(uint32_t controls);      // one simulation tick
    const Bike& bike() const { return bike_; }
    uint32_t position() const { return uint32_t(bike_.i32(0x1c)); }
    int32_t lateral() const { return bike_.i32(0x28) >> 8; }
    const std::vector<RouteSample>& route() const { return routes_[current_]; }
    bool finished() const { return finished_; }
    bool crashed() const { return bike_.i32(0x2f4) != 0; }
    int32_t ticks() const { return ticks_; }
    uint32_t camera_position() const;
    Camera camera() const;              // relative to the route's own road at camera_position()
    int32_t speed() const { return bike_.i32(0xec); }
    int gear() const { return bike_.i32(0x23c); }
private:
    SliceEdges edges(size_t sample) const;
    void move_along(int32_t distance);
    void cross_sample(int32_t delta);
    void accumulate(uint32_t from, uint32_t to, size_t from_sample, size_t to_sample);
    void move();
    void vertical();
    void walls();
    void update_camera();
    void crash();

    std::vector<RouteSample> routes_[2];
    int current_ = 0;
    PhysicsTables tables_;
    Bike bike_;
    size_t sample_ = 0, previous_sample_ = 0;   // bike +0x140 / +0x13c
    int32_t ticks_ = 0, crash_timer_ = 0;
    int32_t camera_x_ = 0, camera_height_ = 150, camera_lift_ = 0, camera_back_ = 0;
    bool finished_ = false;
};
}
