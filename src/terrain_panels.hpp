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
// Terrain type 1 (RMTN/RDWD mountain) side of one projected pair, drawn by
// 0x0043bef0 after the shoulder. near/far carry the original +0x60/+0x70/+0x80
// points per side in projected[0..2]: shoulder, then for type 1 the wall base
// and top (0x00444630), for type 4 the first two hill points. height is the
// near sub-slice +0x154/+0x160 after 0x00454240 (below 1 = -road elevation).
// Height >= 1: 0x0040dfd0 wall face plus plateau tiles out to the view edge.
// Height < 1: 0x0040d030 cliff face plus the WATER128 surface (CEL 0x10,
// 0x00441870); the left fill is only emitted fully off-screen, as original.
struct WallPanelInput {
    HillRoad near,far;
    uint8_t side=0;
    int32_t height=0;
    int32_t near_lane_width=0,far_lane_width=0;  // projected +0x24 (scale*250)
    int32_t logical_width=320,logical_height=240;
    // Tunnel (type 3) pairs: +0xf4/+0xf8 points of near/far, and 0x00443d70's
    // flag for a camera sample inside a tunnel (both-3 sides are skipped).
    bool near_tunnel=false,far_tunnel=false,camera_in_tunnel=false;
    RasterPoint near_tunnel_point{},far_tunnel_point{};
};
// frame 0 = wall face, 1 = plateau (0x0040ccb0: category 1 of the selector).
using WallMaterial=std::function<const CelTexture*(uint8_t frame,int32_t width_q16,int32_t height_q16)>;
std::vector<DrawNode> wall_draw_nodes(const WallPanelInput&,const WallMaterial&,const CelTexture* water);
using ShoulderMaterial=std::function<const CelTexture*(int32_t width_q16,int32_t height_q16)>;
// 0x0041d260. Caller supplies the appropriate hill/wall outer shoulder edge
// and category-5 material; returned texture pointers have caller lifetime.
std::vector<DrawNode> shoulder_draw_nodes(const ShoulderPanelInput&,const Camera&,
    const DepthTable&,const ShoulderMaterial&);
// 0x0040dac0 arch of a tunnel pair: points +0xb4..+0xe0 of far and near
// (A0 curb, A1..A4 arch, A5 curb). Six panels: left wall, haunch, two roof
// halves, haunch, right wall; material(kind 0 roof / 1 haunch / 2 wall).
// A null material ends the arch, as original. backdrop adds 0x0044fcd0's
// colour 0xfd rectangle first (farthest strip inside the tunnel).
struct TunnelArchInput {
    std::array<RasterPoint,6> near{},far{};
    int32_t near_row=0,near_depth=0,far_depth=0,logical_width=320;
    bool backdrop=false;
};
std::vector<DrawNode> tunnel_arch_nodes(const TunnelArchInput&,const std::function<const CelTexture*(uint8_t kind,int32_t width,int32_t height)>&);
// 0x0040d420 tunnel mouth on the near record: mountain face around the
// portal (wall-face material of the mountain side), portal trim (category 3
// frame 5, chosen once) and curb faces (category 3 frame 4, chosen once).
struct TunnelPortalInput {
    uint8_t mountain=0;                       // sub-slice +0x160 > 0
    std::array<RasterPoint,2> base{},top{};   // +0x70/+0x78, +0x80/+0x88
    RasterPoint tunnel_point{};               // +0xf4/+0xf8
    std::array<RasterPoint,6> arch{};         // +0xb4..+0xe0
    std::array<RasterPoint,2> edge{};         // +0x50/+0x54, +0x58/+0x5c
    std::array<int32_t,2> shoulder_x{};       // +0x60, +0x68
    int32_t row=0,depth=0,logical_width=320;
    int32_t sine=0,cosine=0;                  // 0x0047ea84/0x0047ea80 (<<8)
    std::array<int32_t,2> fractions{64,192};  // 0x00465a58, 0x00465a60
};
using TunnelPortalMaterial=std::function<const CelTexture*(uint8_t part,int32_t width,int32_t height)>;  // 0 face, 1 trim, 2 curb
std::vector<DrawNode> tunnel_portal_nodes(const TunnelPortalInput&,const TunnelPortalMaterial&);
// 0x0044f6b0 signed Q16 multiply.
int32_t fixed_multiply(int32_t a,int32_t b);
}
