#pragma once
#include "terrain.hpp"
#include "road.hpp"
#include "raster.hpp"
namespace rr {
struct HillProjection {
    std::array<std::array<int32_t,2>,6> points{}; // Q16 x,y, left/right alternating
    int32_t maximum_row=0; // includes road surface
};
// Original hill projection 0x0041d6d0, including its near-scale overflow sentinels.
HillProjection project_hills(const TerrainSlice& terrain,int32_t road_world_x,
    int32_t road_world_y,const Camera& camera,int32_t scale,int32_t logical_width=320);

struct HillStrip {
    // Far inner, far outer, near outer, near inner, in world units.
    std::array<RasterPoint,4> world{};
    std::array<RasterPoint,4> projected{}; // corresponding Q16 screen points
    int32_t far_depth=0, occlusion_row=0;
    uint8_t side=0;
    bool adjust_seam=false;
};
// Original 0x0044bd40: perspective subdivision of a near hill band.
// Includes early rejection and the original exact final edge.
std::vector<std::array<RasterPoint,4>> subdivide_hill(const HillStrip&,
    const Camera&,const DepthTable&,int32_t logical_width=320);
// 0x0044c0e0: horizontal shoulder strips, with quarter-pixel rows and
// the original four-pixel outer expansion at intermediate depths.
std::vector<std::array<RasterPoint,4>> subdivide_shoulder(const HillStrip&,
    const Camera&,const DepthTable&,int32_t logical_width=320,int32_t logical_height=240);
}
