#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace rr {
struct ViewMode {
    int surface_width=320,surface_height=240,scale_shift=0;
    int display_width=320,display_height=240,origin_x=0,origin_y=0;
    int coordinate_shift=16;
    uint32_t coordinate_mask=0xffffffffu;
    int multiplier=1;
    int32_t map(int32_t q16) const {
        const int64_t divisor=int64_t(1)<<coordinate_shift;
        const int64_t scaled=int64_t(q16)*int64_t(multiplier);
        const int64_t rounded=scaled>=0 ? scaled/divisor : (scaled-divisor+1)/divisor;
        const uint32_t bits=uint32_t(rounded)&coordinate_mask;
        int32_t result;std::memcpy(&result,&bits,sizeof result);return result;
    }
};
// 0x00448870: 640x480 high; 320x240 low; optionally low centered at
// (160,120) in a 640x480 display. 0x00410160/0x0040ffc0: high shifts
// Q16 by 15 AND clears bit 0, so every high-resolution vertex is even.
inline ViewMode view_mode(bool high,bool inset=false) {
    ViewMode mode;
    if (high) {
        mode.surface_width=mode.display_width=640;mode.surface_height=mode.display_height=480;
        mode.scale_shift=1;mode.coordinate_shift=15;mode.coordinate_mask=0xfffffffeu;
    } else if (inset) {
        mode.display_width=640;mode.display_height=480;mode.origin_x=160;mode.origin_y=120;
    }
    return mode;
}
// Render scale s (plan 19 Task 2): race/menus surface is 640s x 480s. s = 1
// returns the original high mode exactly (multiplier 1, mask 0xfffffffe).
// s = 2..4 keeps sub-pixel bits the original clears. Out-of-range scales are
// rejected: callers must not allocate from hand-edited settings without this
// check. Throws std::runtime_error.
inline ViewMode view_mode_scaled(int s) {
    if (s<1 || s>4) throw std::runtime_error("Render scale out of range 1..4");
    if (s==1) return view_mode(true);
    ViewMode mode=view_mode(true);
    mode.surface_width=mode.display_width=640*s;
    mode.surface_height=mode.display_height=480*s;
    mode.multiplier=s;
    mode.coordinate_mask=0xffffffffu;
    return mode;
}
// Widescreen centering convention (plan 21 Task 8): scene builders center at
// wide_width/2 in Q16 and ViewMode::map adds no further origin. Center exactly
// once: builders that center AND a mode that offsets would shift twice.
// Already-built 4:3 display lists must be rebuilt at the new width, not
// translated: translation cannot reveal side actors/scenery the original
// culled. Odd widths keep half-pixel precision (427 -> 213.5 logical).
inline int32_t view_center_q16(int logical_width) {
    if (logical_width<=0 || logical_width>4096) throw std::runtime_error("Logical width out of range");
    return int32_t(int64_t(logical_width)*65536/2);
}
}
