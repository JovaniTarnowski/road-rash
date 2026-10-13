#pragma once
#include "terrain_panels.hpp"
namespace rr {
struct BuildingRoad {
    RasterPoint shoulder{},transition{};
    int32_t height=0,forward_height=0,roof_offset=0,forward_roof_offset=0;
    uint8_t type=0,selector=255,marking_flags=0;
    bool cut_roof=false;
};
struct BuildingPanelInput {
    BuildingRoad near,far;
    uint8_t side=0;
    int32_t logical_width=320;
    int32_t near_depth_q8=0,far_depth_q8=0;  // plan 21 Task 5
};
using BuildingMaterial=std::function<const CelTexture*(bool far,int32_t width,int32_t height)>;
// Original 0x0040c770 / 0x0040c930 / 0x0040cbb0: facade,
// flat roof and exposed end face, in original display-list order.
std::vector<DrawNode> building_draw_nodes(const BuildingPanelInput&,const BuildingMaterial&);
struct BuildingShoulderInput {
    HillStrip top;
    int32_t near_depth=0,far_base_y=0,near_base_y=0,logical_width=320;
    int32_t far_world_base_y=0,near_world_base_y=0;
};
using BuildingShoulderMaterial=std::function<const CelTexture*(uint8_t frame,int32_t width,int32_t height)>;
// 0x0040bde0, ordinary category-5 building shoulders and raised curbs.
std::vector<DrawNode> building_shoulder_nodes(const BuildingShoulderInput&,const Camera&,const DepthTable&,const BuildingShoulderMaterial&);
struct MarkedGroundInput {
    RasterPoint near_edge{},far_edge{};
    int32_t near_scale=0,far_scale=0,logical_width=320;
    int32_t near_depth_q8=0,far_depth_q8=0;  // plan 21 Task 5
    uint8_t side=0,marking_flags=32;
    int strips=16;
};
std::vector<DrawNode> marked_ground_nodes(const MarkedGroundInput&,const std::function<const CelTexture*(uint8_t material)>&);
}
