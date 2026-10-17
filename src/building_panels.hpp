#pragma once
#include "terrain_panels.hpp"
#include "family.hpp"
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
// 0x0040ba10, side streets (marking flags 8/16): pavement tiles (frame 2)
// stepping outward from the transition point, 256 world units each, up to
// sixteen, until off the view; flag 16 (near-side opening) also emits curb
// faces (frame 3), flag 8 (far-side) a colour-0x4d gap fill to the nearer
// strip. Points are the original +0x50..+0xa4 values of one side.
struct SideStreetInput {
    uint8_t side=0,marking_flags=0;
    RasterPoint transition{};                 // near +0xa0/+0xa4
    int32_t near_shoulder_x=0,near_curb_y=0,near_row=0;   // +0x60, +0x54, +0x4c
    int32_t far_shoulder_x=0,far_curb_y=0;    // far record +0x60, +0x54
    int32_t near_scale=0,near_shoulder_scale=0;   // depth, depth+|shoulder|
    int32_t far_scale=0,far_shoulder_scale=0;     // far depth, far depth-|shoulder|
    bool nearer=false;int32_t nearer_curb_y=0;    // nearer strip's +0x54 side value
    int32_t logical_width=320,logical_height=240;
};
std::vector<DrawNode> side_street_nodes(const SideStreetInput&,const BuildingShoulderMaterial&);
// 0x0040bde0 third block (marking flag 16): the curb face across the opening.
std::vector<DrawNode> side_street_curb_nodes(uint8_t side,int32_t edge_x,int32_t curb_y,int32_t shoulder_x,int32_t row,int32_t occlusion_row,const BuildingShoulderMaterial&);
// 0x0040c450 (marking flag 16): facades along the side street. The original
// random helper 0x00441730 returns 0, so runs and indexes are fixed: the
// first run of max(4, 0x00465a40[0]) uses the sample's selector, later runs
// alternate with entry 0; groups under nominal height 2 step down one entry.
struct SideFacadeInput {
    uint8_t side=0,selector=255;
    bool has_building=false;                  // +0x74/+0x7c nonzero
    RasterPoint transition{};
    int32_t scale=0;                          // depth+|shoulder| scale
    int count=8;                              // 16 up to 0x004ba180's sample
    uint8_t run_length=2;                     // 0x00465a40[0]
    int32_t logical_width=320;
};
std::vector<DrawNode> side_facade_nodes(const SideFacadeInput&,const std::function<const CelGroup*(uint32_t index)>& group,
    const std::vector<uint8_t>& logarithms);
}

