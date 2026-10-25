#pragma once
#include "race.hpp"
#include <cstdint>
#include <vector>

namespace rr {
// Plan 21 Task 2: interpolated frame state. Positions/heights/angles blend
// between ticks; animation frames and sprite views come from the newest tick.
struct ActorPose {
    uint32_t id = 0;
    uint32_t position_q8 = 0;
    int32_t lateral_q8 = 0, height_q8 = 0, heading = 0, roll = 0;
    int route = 0;
    bool visible = false;
};
struct ViewState {
    uint32_t camera_position_q8 = 0; // Race::camera_position()
    Camera camera;                   // x, height
    int camera_route = 0;
    std::vector<ActorPose> actors; // riders, crash riders, cars, cops, stable id order
    uint32_t tick = 0;
};
class TrafficSystem;
void capture_view(ViewState&, const Race&, const TrafficSystem*);
void blend_view(ViewState& out, const ViewState& a, const ViewState& b, uint32_t alpha_q16,
                uint32_t teleport_q8 = 8 << 8);
} // namespace rr
