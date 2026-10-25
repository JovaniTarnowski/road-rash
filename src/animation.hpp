#pragma once
#include <vector>
#include "cel.hpp"
#include <memory>
namespace rr {
struct AnimationFrame {
    std::shared_ptr<const CelTexture> texture;
    std::optional<uint32_t> legacy_pixel_word;
    std::array<int32_t, 2> hotspot{};
    std::array<int32_t, 4> collision{};
    // Collision boxes {left, bottom, right, top} relative to the object
    // (frame +0xc list, frame +0x10 count; read by 0x0044b630 into the ring
    // slice for the 0x004373f0 sweep). Empty when the frame has none.
    std::vector<std::array<int32_t, 4>> boxes;
    uint32_t collision_reference = 0; // unused legacy references are retained, never dereferenced
    uint32_t flags = 0;
    uint16_t width = 0, height = 0;
};
struct Animation {
    std::vector<AnimationFrame> frames;
};
// RRAN / RRFD hierarchy before the original 0x0044a200 relocation. Offsets
// remain owned and bounded; resource aliasing follows the original loader.
Animation read_animation(const std::vector<uint8_t>& bytes, uint32_t offset = 0);
} // namespace rr
