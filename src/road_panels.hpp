#pragma once
#include "raster.hpp"
#include <vector>
namespace rr {
struct RoadPanelInput {
    int32_t first_left=0,first_y=0,first_lane_width=0;
    int32_t second_left=0,second_y=0,second_lane_width=0;
    int32_t left_lanes_q8=0,right_lanes_q8=0;
    uint8_t left_lane_count=0,right_lane_count=0;
    uint8_t surface_flags=0,marking_flags=0;
    uint8_t terrain_type=0,next_terrain_type=0;
    bool visible=true;
    int32_t near_depth_q8=0,far_depth_q8=0;  // plan 21 Task 3: perspective depths
};
enum class RoadPanelMaterial { plain,alternate,marked0,marked1,marked2,normal0,normal1,normal2 };
struct RoadPanel {
    std::array<RasterPoint,4> points{}; // Q16, original CCB quad order
    std::array<int32_t,4> view_depth{};  // plan 21 Task 3: Q8, {far,far,near,near}
    RoadPanelMaterial material=RoadPanelMaterial::plain;
    uint32_t pre0=0;
    bool flip_u=false,flip_v=false;
};
// 0x0043c170 / 0x0043c0b0: geometry and material selection. Resource binding is external.
std::vector<RoadPanel> road_panels(const RoadPanelInput&,int32_t logical_width=320);
}
