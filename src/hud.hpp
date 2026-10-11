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
// Original full high-resolution dashboard. Geometry and indexed artwork come
// from the imported executable/disc; text and pen rasterization use native code.
class Hud {
public:
    Hud(const Assets&,const std::vector<uint8_t>& exe,int bike,bool metric);
    HudState state(const Race&,const std::string& player,const std::string& target) const;
    RgbImage compose(const HudState&,const PhysicsTables&) const;
    static int meter_level(int current,int base);
private:
    bool metric_;
    Palette palette_;
    TextRenderer text_;
    std::vector<uint8_t> dashboard_,meter_;
    std::array<int32_t,55> layout_{};
};
}
