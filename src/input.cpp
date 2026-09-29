#include "input.hpp"

namespace rr {
uint32_t controls_from(const PlayerInput& in) {
    uint32_t c = 0;
    if (in.accelerate) c |= control_accelerate;
    if (in.brake) c |= control_brake;
    if (in.left || in.steer_axis < -steer_dead_zone) c |= control_left;
    else if (in.right || in.steer_axis > steer_dead_zone) c |= control_right;
    return c;
}

PlayerInput touch_input(float x, float y, float width, float height) {
    PlayerInput in;
    if (x < width / 3) in.left = true;
    else if (x > width * 2 / 3) in.right = true;
    else if (y > height * 3 / 4) in.brake = true;
    in.accelerate = !in.brake;
    return in;
}
}
