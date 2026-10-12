#pragma once
#include "assets.hpp"
#include "framebuffer.hpp"
#include "race.hpp"
#include "text.hpp"
namespace rr {
struct HudState {
    int place=1,distance=0,speed_angle=0,rpm_angle=0;
    int stamina=0,condition=0,target_stamina=0;
    std::string player,target;
};
// MiniDash 0x00467a98 (F10 cycles 0->1->2->0 in 0x004431d0): 0 full dashboard,
// 1 mini (numeric speed, no needles/name bars), 2 off (0x00446c30 draws nothing).
enum class DashMode : int { full = 0, mini = 1, off = 2 };
// Original full high-resolution dashboard. Geometry and indexed artwork come
// from the imported executable/disc; text and pen rasterization use native code.
// Native 3x3 Bresenham needles document, but do not replicate, the original
// GDI pen (CreatePen(PS_SOLID, HiRes?3:2, 0x81172), MoveToEx/LineTo in 0x00446c30).
class Hud {
public:
    Hud(const Assets&,const std::vector<uint8_t>& exe,int bike,bool metric);
    HudState state(const Race&,const std::string& player,const std::string& target) const;
    RgbImage compose(const HudState&,const PhysicsTables&,DashMode mode = DashMode::full) const;
    // Low-mode artwork: 320-wide KLODASH/MLODASH dashboard + 95-stride LILMETER,
    // selected in low resolution (0x004457f0 choice 1/2); MiniDash picks the stride.
    void set_low_mode(bool low);
    static int meter_level(int current,int base);
    static int cycle_mini_dash(int mode);   // 0->1->2->0, as the F10 pump does
private:
    bool metric_;
    bool low_ = false;
    Palette palette_;
    TextRenderer text_;
    std::vector<uint8_t> dashboard_,dashboard_low_,meter_,meter_low_;
    std::array<int32_t,55> layout_{};
    std::array<int32_t,55> layout_mini_{};
};
}
