#pragma once
#include "course.hpp"
namespace rr {
// Original 0x00439b10 for the building and hill terrain used by Napa.
// lateral is Q8 relative to the current road centre. Next centre is a world
// offset relative to that same centre; fraction is the sub-slice Q8 byte.
// Types 1 (mountain: wall ramps, a drop to height 0 beyond a cliff edge)
// and 3 (tunnel: the sidewalk heights) use the 0x00454240 sub-slice values;
// centre is the road's lateral offset (+0xcc), which type 1 compares
// against absolute positions.
int32_t terrain_height(const TerrainSlice&,const PathSample&,const TerrainSlice& next,
    const PathSample& next_path,int32_t lateral_q8,uint8_t fraction,int32_t next_centre=0,int32_t centre=0);
}
