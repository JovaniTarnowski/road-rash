#pragma once
#include "assets.hpp"
#include <unordered_map>
#include <vector>
#include "framebuffer.hpp"
#include "race.hpp"
#include "text.hpp"
namespace rr {
struct HudState {
    int place=1,distance=0,speed_angle=0,rpm_angle=0;
    int stamina=0,condition=0,target_stamina=0;
    std::string player,target;
    // DAT_004b8a34: signed route gap to the target (Q8 samples, +0x1f8
    // units); > 0 the target is behind, <= 0 ahead. Drawn only with a target.
    int32_t target_gap=0;
};
// 0x00438550, run every frame from 0x00438450: the opponent the dashboard
// names. d = player.+0x1f8 - opponent.+0x1f8 over every opponent slot (no
// crash/finish filter); +0x1f8 is the Q8 distance still to the finish
// (0x00407410). The nearest behind wins only when less than half the
// nearest ahead; gap > 0 behind, <= 0 ahead. rider is -1 when no opponent
// is ahead and none is close enough behind (or there are none).
struct NearestOpponent { int rider=-1; int32_t gap=0; };
NearestOpponent nearest_opponent(const Race&);
// MiniDash 0x00467a98 (F10 cycles 0->1->2->0 in 0x004431d0): 0 full dashboard,
// 1 mini (numeric speed, no needles/name bars), 2 off (0x00446c30 draws nothing).
enum class DashMode : int { full = 0, mini = 1, off = 2 };
// Original full high-resolution dashboard. Geometry and indexed artwork come
// from the imported executable/disc; text and pen rasterization use native code.
// Native 3x3 Bresenham needles document, but do not replicate, the original
// GDI pen (CreatePen(PS_SOLID, HiRes?3:2, 0x81172), MoveToEx/LineTo in 0x00446c30).
// Review 2026-10-13 item 2: exact memo for the per-frame dashboard map.
// Same nearest-distance metric as the old inline search, memoized per exact
// color (a cell table would quantize: two colors in one RGB555 cell can map
// to different indices). Entries persist across frames; the caller resets on
// palette change. Cleared maps keep capacity: no per-frame allocation.
class DashLut {
public:
    uint8_t index(uint32_t c, const Palette& pal);
    void reset() { memo_.clear(); }
    size_t size() const { return memo_.size(); }
private:
    std::unordered_map<uint32_t, uint8_t> memo_;
};
class Hud {
public:
    Hud(const Assets&,const std::vector<uint8_t>& exe,int bike,bool metric);
    // rider_names is indexed like Race::riders(); the target is the
    // nearest opponent (0x00446b80), not the last rider hit.
    HudState state(const Race&,const std::string& player,const std::vector<std::string>& rider_names) const;
    // The ahead/behind indicator 0x00446c30 draws in Wingdings (0xe9 up,
    // 0xea down); Wingdings is a Windows system font, not on the disc, so a
    // native arrow of the same cell height stands in for the glyph.
    static void draw_gap_arrow(RgbImage&,int left,int top,bool up,uint32_t color,int scale = 1);
    RgbImage compose(const HudState&,const PhysicsTables&,DashMode mode = DashMode::full,int scale = 1) const;
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
