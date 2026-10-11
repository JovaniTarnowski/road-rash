#include "actor_render.hpp"
#include "bike.hpp"
#include <stdexcept>
namespace rr {
namespace {
int32_t bits(uint32_t u){return u<=INT32_MAX?int32_t(u):int32_t(int64_t(u)-0x100000000LL);}
int32_t add(int32_t a,int32_t b){return bits(uint32_t(a)+uint32_t(b));}
int32_t sub(int32_t a,int32_t b){return bits(uint32_t(a)-uint32_t(b));}
int32_t mul(int32_t a,int32_t b){return bits(uint32_t(a)*uint32_t(b));}
}
ActorAngles actor_angles(const ActorOrientation& actor,int32_t camera_x) {
    int32_t depth=add(actor.depth,int32_t(uint32_t(actor.position)&255));
    if(actor.rider)depth=mul(depth,2);
    const int32_t lateral=sub(add(actor.road_x,actor.lateral_q8/256),camera_x);
    int32_t view=mul(approx_atan2(depth,lateral),65536),heading=actor.road_heading;
    if(heading>0x800000)heading=add(heading,-0x1000000);
    if(view>0x800000)view=add(view,-0x1000000);
    return {add(sub(heading,view),actor.heading),actor.roll,view};
}
std::optional<DrawNode> actor_node(const AnimationFrame& frame,const CelTexture* texture,
    const ActorProjection& actor,const Camera& camera,const DepthTable& depths,int32_t logical_width) {
    if(!texture)return std::nullopt;
    const uint32_t index=(uint32_t(actor.depth)+actor.fraction)&~1u;
    if(index/2>=depths.scale.size())throw std::runtime_error("Actor projection depth outside table");
    const int32_t projection=depths.scale[index/2];
    const int32_t width=frame.width,height=frame.height;
    if(width<=0 || height<=0)return std::nullopt;
    int32_t size=actor.size;
    if(actor.flags&0x10000)size=mul(size,2);
    if(actor.flags&0x20000)size=mul(size,4);
    // Size zero uses the viewport scale global (0x475618), 128 at 320 pixels.
    const int32_t scale=size?mul(size>>6,projection)>>2:mul((mul(logical_width/2,0x270)>>8),projection)/width;
    const int32_t hotspot_x=actor.flags&1?sub(width,frame.hotspot[0]):frame.hotspot[0];
    const int32_t anchor_x=mul(scale,hotspot_x);
    const int32_t anchor_y=mul(scale,frame.hotspot[1]);
    const int32_t road_y=add(actor.road_y,mul(sub(actor.far_y,actor.road_y),actor.fraction)>>8);
    const int32_t top=sub(add(mul(sub(camera.height,add(actor.ground,road_y)),projection),90*65536),anchor_y);
    if(actor.clip_ground && top>=actor.occlusion)return std::nullopt;
    const int32_t bottom=add(top,mul(scale,height));
    const int32_t road_x=add(actor.road_x,mul(sub(actor.far_x,actor.road_x),actor.fraction)>>8);
    int32_t left=sub(add(mul(sub(add(actor.lateral,road_x),camera.x),projection),(logical_width&~1)*32768),anchor_x);
    int32_t right=add(left,mul(scale,width));
    if(right<=0 || left>=mul(logical_width,65536))return std::nullopt;
    if(actor.flags&1)std::swap(left,right);
    return DrawNode{{{{left,top},{right,top},{right,bottom},{left,bottom}}},texture,std::nullopt,bool(actor.flags&1),false};
}
}
