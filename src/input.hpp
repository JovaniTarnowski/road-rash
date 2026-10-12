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
// Analog-control path as traced (0x0044f880 joystick branch): the original picks
// dead zones 0x3fff/0xbffd when the binding row's analog flag (0x00476c04) is set
// and 0x6fff/0x8fff otherwise, then scales the deflection via 0x0044fa00.
// Natively the stick is digital past one threshold: steer_dead_zone matches the
// non-analog 0x6fff edge scaled to SDL units (0x6fff/0x7fff*32768 ~ 12000...16384;
// 12000 keeps the existing game feel; see analysis/options2-notes.md).
constexpr int steer_dead_zone = 12000;
struct MenuDirections {
    bool left = false, right = false, up = false, down = false;
};
// Native menu convenience: one dominant stick direction, with bounded repeat.
// After interruption, require neutral before accepting a held stick again.
class MenuStickInput {
public:
    MenuDirections update(int x, int y, uint64_t now_ms);
    void suspend();
private:
    enum class Direction { none, left, right, up, down } direction_ = Direction::none;
    uint64_t next_repeat_ = 0;
    bool require_neutral_ = false;
};
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
