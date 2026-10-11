#pragma once
#include "scene.hpp"
#include "world_renderer.hpp"
#include <functional>

namespace rr {
struct HillRoad {
    uint8_t type=4;
    int32_t depth=0;
    // Shoulder, followed by three hill edges on one side of the road.
    std::array<RasterPoint,4> projected{},world{};
};
struct HillPanelInput {
    HillRoad near,far;
    uint8_t side=0;
    int32_t occlusion_row=240*65536,logical_width=320;
    // Imported original join offsets at 0x004688e0. Disabling overlap is
    // supported by the original quality setting at 0x00468960.
    bool overlap=false;
    std::array<RasterPoint,16> join_offsets{};
};
using HillMaterial=std::function<const CelTexture*(uint8_t band,int32_t width_q16,int32_t height_q16)>;
// 0x0041c390, 0x0041cf60, 0x0041d060: far-to-near hill bands.
std::vector<DrawNode> hill_draw_nodes(const HillPanelInput&,const Camera&,
    const DepthTable&,const HillMaterial&);
struct ShoulderPanelInput {
    HillStrip strip;
    int32_t near_depth=0,logical_width=320,logical_height=240;
    bool has_lanes=true;
};
using ShoulderMaterial=std::function<const CelTexture*(int32_t width_q16,int32_t height_q16)>;
// 0x0041d260. Caller supplies the appropriate hill/wall outer shoulder edge
// and category-5 material; returned texture pointers have caller lifetime.
std::vector<DrawNode> shoulder_draw_nodes(const ShoulderPanelInput&,const Camera&,
    const DepthTable&,const ShoulderMaterial&);
}
