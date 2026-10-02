#include "input.hpp"

namespace rr {
uint32_t controls_from(const PlayerInput& in) {
    uint32_t c = 0;
    if (in.accelerate) c |= control_accelerate;
    if (in.brake) c |= control_brake;
    if (in.punch) c |= control_punch;
    if (in.kick) c |= control_kick;
    if (in.backhand) c |= control_backhand;
    if (in.left || in.steer_axis < -steer_dead_zone) c |= control_left;
    else if (in.right || in.steer_axis > steer_dead_zone) c |= control_right;
    return c;
}

PlayerInput touch_input(float x, float y, float width, float height) {
    PlayerInput in;
    if (y < height / 3) { in.punch = true; in.accelerate = true; return in; }   // top third: punch
    if (x < width / 3) in.left = true;
    else if (x > width * 2 / 3) in.right = true;
    else if (y > height * 3 / 4) in.brake = true;
    in.accelerate = !in.brake;
    return in;
}

void TouchInput::handle(const SDL_Event& e) {
    const auto id = std::make_pair(e.tfinger.touchID, e.tfinger.fingerID);
    if (e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED)
        fingers_.erase(id);
    else if (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION)
        fingers_[id] = touch_input(e.tfinger.x, e.tfinger.y, 1.f, 1.f);
}

PlayerInput TouchInput::current() const {
    PlayerInput in;
    for (const auto& finger : fingers_) {
        const auto& f = finger.second;
        in.accelerate |= f.accelerate; in.brake |= f.brake;
        in.left |= f.left; in.right |= f.right;
        in.punch |= f.punch; in.kick |= f.kick; in.backhand |= f.backhand;
    }
    // A braking finger overrides auto-throttle from the steering/attack zones.
    if (in.brake) in.accelerate = false;
    return in;
}
}
