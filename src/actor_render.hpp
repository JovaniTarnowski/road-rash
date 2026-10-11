#pragma once
#include "animation.hpp"
#include "road.hpp"
#include "world_renderer.hpp"
namespace rr {
struct ActorOrientation {
    int32_t depth=0,position=0,road_x=0,lateral_q8=0,road_heading=0,heading=0,roll=0;
    bool rider=false;
};
struct ActorAngles {int32_t yaw=0,roll=0,view_angle=0;};
// 0x00413ae0, before descriptor reversal/wrapping in select_actor_view.
ActorAngles actor_angles(const ActorOrientation&,int32_t camera_x);
struct ActorProjection {
    int32_t road_x=0,road_y=0,far_x=0,far_y=0;
    int32_t lateral=0,ground=0,depth=0;
    uint8_t fraction=0;
    int32_t size=512,occlusion=240*65536;
    uint32_t flags=0;
    bool clip_ground=true;
};
// Axis-aligned actor billboard path 0x0044ba40. Bike/shadow dispatch clears
// rotation before calling this routine. Caller owns the selected texture.
std::optional<DrawNode> actor_node(const AnimationFrame&,const CelTexture*,
    const ActorProjection&,const Camera&,const DepthTable&,int32_t logical_width=320);
}
