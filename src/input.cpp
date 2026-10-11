#include "input.hpp"
#include <cstdlib>

namespace rr {
void MenuStickInput::suspend() {
    direction_ = Direction::none;
    next_repeat_ = 0;
    require_neutral_ = true;
}

MenuDirections MenuStickInput::update(int x, int y, uint64_t now_ms) {
    // SDL axis values are signed 16-bit; use int so abs(-32768) is defined.
    const int ax = std::abs(x), ay = std::abs(y);
    if (ax <= 8000 && ay <= 8000) {
        direction_ = Direction::none;
        require_neutral_ = false;
        return {};
    }
    if (require_neutral_) return {};
    const int threshold = direction_ == Direction::none ? steer_dead_zone : 8000;
    if (ax <= threshold && ay <= threshold) return {};
    const Direction next = ax >= ay ? (x < 0 ? Direction::left : Direction::right)
                                    : (y < 0 ? Direction::up : Direction::down);
    if (next != direction_) {
        direction_ = next;
        next_repeat_ = now_ms + 350;
    } else {
        if (now_ms < next_repeat_) return {};
        // At most one navigation step per frame, even after a long stall.
        next_repeat_ = now_ms + 100;
    }
    return {next == Direction::left, next == Direction::right,
            next == Direction::up, next == Direction::down};
}

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
