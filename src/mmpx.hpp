#pragma once
#include <array>
#include <cstdint>
#include "bob.hpp"
namespace rr {
// MMPX 2x (McGuire & Gagiu 2021, "MMPX Style-Preserving Pixel Art
// Magnification", Journal of Computer Graphics Techniques 10(2)):
// style-preserving pixel-art magnification on 8-bit indexed images. Input and
// output are palette indices; the output uses only indices present in the
// input, so the 8-bit palette, the 15 rider remaps and palette shading keep
// working. `transparent` (index 0 for sprites/scenery, -1 for none) is its
// own colour and never blended into opaque pixels.
// The paper's brightness measure uses alpha and RGB sum, not perceptual luma.
// Its tie-break compares colours by (RGB sum, alpha); pass `luma` as the
// 32-bit RGB-sum brightness table (a general transparent colour can exceed 16
// bits). Implemented in integer arithmetic; deterministic on every platform.
// NOTE: `remap(mmpx(img)) == mmpx(remap(img))` is NOT generally true, even for
// a bijective rider remap: arbitrary relabelling can change a brightness
// tie-break. The game always upscales index images once, then remaps at draw
// time (valid indices, documented shared silhouette); that form is correct
// even when the remap is not a bijection.
Image8 mmpx2x(const Image8& source, const std::array<uint32_t, 256>& luma, int transparent);
Image8 mmpx4x(const Image8& source, const std::array<uint32_t, 256>& luma, int transparent);
// Luma table from the game palette (Palette::rgbx). Throws
// std::runtime_error on bad dimensions.
std::array<uint32_t, 256> mmpx_luma(const std::array<uint8_t, 1024>& rgbx);
} // namespace rr
