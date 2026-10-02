#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace rr {
// 8-bit palette-indexed image; index 0 is transparent for sprites.
struct Image8 { int width = 0, height = 0; std::vector<uint8_t> pixels; };

// DATA/BIKERS/{Hi,Lo}{BOB,COP,SHADOW,SHADOC}.DAT, read by 0x004436a0: per frame a width
// byte, a height byte, then width*height indices. 0x00443610 loads 0x146 bike frames and
// 0x2d cop frames.
constexpr int bob_frames = 0x146, cop_frames = 0x2d;
std::vector<Image8> read_bobs(const std::vector<uint8_t>& file, int frames);

// Per-rider colour remap applied when a frame is copied to the sprite page (0x00443820):
// 256 entries per rider at 0x0046e158 in ROADRASH.EXE.
struct RiderRemaps { std::vector<std::array<uint8_t,256>> tables; };
RiderRemaps read_rider_remaps(const std::vector<uint8_t>& exe, int riders);
Image8 remap(const Image8&, const RiderRemaps&, int rider);
}
