#pragma once
#include "terrain.hpp"
#include "road.hpp"
namespace rr {
struct HillProjection {
    std::array<std::array<int32_t,2>,6> points{}; // Q16 x,y, left/right alternating
    int32_t maximum_row=0; // includes road surface
};
// Original hill projection 0x0041d6d0, including its near-scale overflow sentinels.
HillProjection project_hills(const TerrainSlice& terrain,int32_t road_world_x,
    int32_t road_world_y,const Camera& camera,int32_t scale,int32_t logical_width=320);
}
