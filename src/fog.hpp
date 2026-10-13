// Plan 21 Task 5: distance fog and procedural colour grading.
#pragma once
#include <cstdint>
#include <vector>
#include "settings.hpp"

namespace rr {
// 0 at the camera, rising to strength/100 at the last drawn slice;
// exponential-squared falloff. Returns Q8 (0..256 at full).
uint16_t fog_factor_q8(int32_t depth_q8, int draw_slices, int strength);
// Average colour of the bottom 8 rows of RGB pixels: the fog colour.
// No data stored in source; computed at runtime from the user's art.
uint32_t fog_colour(const uint32_t* argb, int width, int height);
// Procedural 17x17x17 grading LUT for a Grade; Grade::none is the identity.
// Latticed at r,g,b in {0,16,...,256 clamped to 255}; entries 0x00RRGGBB.
std::vector<uint32_t> grade_lut(Grade grade);
// Apply a LUT to one channel-triplet (trilinear in the lattice).
uint32_t grade_apply(const std::vector<uint32_t>& lut, uint32_t rgb);
}  // namespace rr
