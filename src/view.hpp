#pragma once
#include <cstdint>
#include <cstring>
namespace rr {
struct ViewMode {
    int surface_width=320,surface_height=240,scale_shift=0;
    int display_width=320,display_height=240,origin_x=0,origin_y=0;
    int coordinate_shift=16;
    uint32_t coordinate_mask=0xffffffffu;
    int32_t map(int32_t q16) const {
        const int64_t divisor=int64_t(1)<<coordinate_shift;
        const int64_t rounded=q16>=0 ? int64_t(q16)/divisor : (int64_t(q16)-divisor+1)/divisor;
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
}
