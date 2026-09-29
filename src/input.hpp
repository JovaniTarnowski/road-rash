#pragma once
#include "bike.hpp"

namespace rr {
// Device-independent player intent; mapped onto the original control bits (bike.hpp).
struct PlayerInput {
    bool accelerate = false, brake = false, left = false, right = false;
    int steer_axis = 0;   // gamepad stick, -32768..32767
};
constexpr int steer_dead_zone = 12000;
uint32_t controls_from(const PlayerInput&);
// One touch point on a width x height window.
PlayerInput touch_input(float x, float y, float width, float height);
}
