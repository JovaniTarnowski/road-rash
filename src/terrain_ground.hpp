#pragma once
#include "course.hpp"
namespace rr {
// Original 0x00439b10 for the building and hill terrain used by Napa.
// lateral is Q8 relative to the current road centre. Next centre is a world
// offset relative to that same centre; fraction is the sub-slice Q8 byte.
// 0x00454240's mountain values relative to the road centre: outer shoulder
// edges (+0xe8/+0xec, halved shoulders in a tunnel), wall base and top x
// (+0x14c/+0x150, +0x158/+0x15c) and heights (+0x154/+0x160; below 1 they
// become -elevation). A tunnel (marking flag 0x40) picks the mountain side
// (right if its height is at least 1; the other side is then at most 0),
// clamps that height to 800 and moves the open side's wall edge a second
// shoulder width out.
struct WallProfile {
    int32_t outer_left = 0, outer_right = 0;
    std::array<int32_t, 2> base{}, top{}, height{};
    uint8_t mountain = 0;
};
WallProfile wall_profile(const TerrainSlice&, const PathSample&, bool tunnel);
// Types 1 (mountain: wall ramps, a drop to height 0 beyond a cliff edge)
// and 3 (tunnel: the sidewalk heights) use the 0x00454240 sub-slice values;
// centre is the road's lateral offset (+0xcc), which type 1 compares
// against absolute positions.
int32_t terrain_height(const TerrainSlice&, const PathSample&, const TerrainSlice& next,
                       const PathSample& next_path, int32_t lateral_q8, uint8_t fraction,
                       int32_t next_centre = 0, int32_t centre = 0);
} // namespace rr
