#pragma once
#include "bike.hpp"
#include <SDL3/SDL_events.h>
#include <map>

namespace rr {
// Device-independent player intent; mapped onto the original control bits (bike.hpp).
struct PlayerInput {
    bool accelerate = false, brake = false, left = false, right = false;
    bool punch = false, kick = false, backhand = false;
    int steer_axis = 0;   // gamepad stick, -32768..32767
};
constexpr int steer_dead_zone = 12000;
uint32_t controls_from(const PlayerInput&);
// One touch point on a width x height window.
PlayerInput touch_input(float x, float y, float width, float height);
// Each finger keeps its own intent; releasing one cannot release another.
class TouchInput {
public:
    void handle(const SDL_Event&);
    void clear() { fingers_.clear(); }
    PlayerInput current() const;
private:
    std::map<std::pair<SDL_TouchID, SDL_FingerID>, PlayerInput> fingers_;
};
}
