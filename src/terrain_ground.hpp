#pragma once
#include "course.hpp"
namespace rr {
// Original 0x00439b10 for the building and hill terrain used by Napa.
// lateral is Q8 relative to the current road centre. Next centre is a world
// offset relative to that same centre; fraction is the sub-slice Q8 byte.
int32_t terrain_height(const TerrainSlice&,const PathSample&,const TerrainSlice& next,
    const PathSample& next_path,int32_t lateral_q8,uint8_t fraction,int32_t next_centre=0);
}
