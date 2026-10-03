#pragma once
#include "assets.hpp"
#include "bob.hpp"
#include "cel.hpp"
#include <optional>

namespace rr {
// Bike, cop and shadow bob sheets used by dynamic rider display nodes (0x00443610).
struct RiderSprites {
    std::vector<Image8> bob, cop, shadow, shadoc;
    RiderRemaps remaps;   // 15 rider colour tables at 0x0046e158
};
RiderSprites load_rider_sprites(const Assets&, const std::vector<uint8_t>& exe, bool high_resolution);

// Display nodes whose resource is the "Bob" marker (0x00466688) carry a frame (+0x34) and a colour
// (+0x38). 0x00410080 rejects out-of-range pairs; 0x00443820 copies the frame, remapped by the
// colour for riders 0..14, into a 256-wide page with bounds (0,0,w,h). 0xde selects cop frames,
// 0xdf cop shadows and 0xe0 bike shadows (no remap).
std::optional<CelTexture> rider_texture(const RiderSprites&, int32_t colour, int32_t frame);
}
