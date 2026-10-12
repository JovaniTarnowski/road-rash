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
namespace {
// 0x0044f510: quadrant sin/cos from the quarter table (imported, never embedded).
void actor_sincos(const std::vector<int32_t>& table,int32_t index,int32_t& cos_out,int32_t& sin_out) {
    const uint32_t k = uint32_t(index) & 0x3ff;
    const uint32_t q = (uint32_t(index) & 0xfff) >> 10;
    // 0x0044f510 case 3: param_2 = iVar2 * -2 (i.e. -2*tab[1023-k]),
    // param_3 = iVar1 * 2 (2*tab[k]).
    // (Port names: cos_out holds local_3c, sin_out holds local_34[0].)
    if (q == 3) {
        const int32_t back = table.at(size_t(1023 - k));
        cos_out = sub(0,add(back,back));
        sin_out = add(table.at(size_t(k)),table.at(size_t(k)));
        return;
    }
    const int32_t forward = table.at(size_t(k)), backward = table.at(size_t((1023 - k) & 0x3ff));
    switch (q) {
    case 0: cos_out = add(forward,forward); sin_out = add(backward,backward); return;
    case 1: cos_out = add(backward,backward); sin_out = sub(0,add(forward,forward)); return;
    default: cos_out = sub(0,add(forward,forward)); sin_out = sub(0,add(backward,backward)); return;
    }
}
int32_t decoded_dim(int32_t v) {
    if (v < 0) return bits(1u << (uint32_t(uint8_t(v)) & 31));
    return v & 0x1ff;
}
// 0x00450ad0 idivl: (val & 0xffff00ff) with an arithmetic >>8 (CONCAT44
// sign-extends), 64-bit truncating divide by the decoded dim.
int32_t masked_div(int32_t value,int32_t divisor) {
    const int32_t lo = bits(uint32_t(value) & 0xffff00ffu);
    return int32_t(int64_t(lo >> 8) / int64_t(divisor));
}
}   // namespace
std::optional<DrawNode> actor_node_rotated(const AnimationFrame& frame,const CelTexture* texture,
    const ActorProjection& actor,const Camera& camera,const DepthTable& depths,const std::vector<int32_t>& sin_table,
    int32_t roll,int32_t logical_width) {
    if(!texture)return std::nullopt;
    const uint32_t index=(uint32_t(actor.depth)+actor.fraction)&~1u;
    if(index/2>=depths.scale.size())throw std::runtime_error("Actor projection depth outside table");
    const int32_t projection=depths.scale[index/2];
    const int32_t width=frame.width,height=frame.height;
    if(width<=0 || height<=0)return std::nullopt;
    int32_t size=actor.size;
    if(actor.flags&0x10000)size=mul(size,2);
    if(actor.flags&0x20000)size=mul(size,4);
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
    // Axis corners in 0x0044ba40 local order [top,leftA,bottom,rightB],
    // flip-arranged (local_10/local_8 swap). local_c IS the bottom: line 89
    // adds iVar4*uVar6 with the pre-reassignment height decode.
    int32_t c0=top,c1=left,c2=bottom,c3=right;
    if(actor.flags&1){c1=right;c3=left;}
    // aux inputs from 0x0044ba40. decW/decH decode the resource dims
    // (+0x3c = width, +0x40 = height for positive RRAN sizes). local_20 is
    // the flip-adjusted hotspot; aux0/aux1 select it unless it aliases a dim.
    const int32_t res_w=decoded_dim(width),res_h=decoded_dim(height);
    const int32_t hx=frame.hotspot[0],hy=frame.hotspot[1];
    const int32_t local_20=(actor.flags&1)?sub(res_w,hx):hx;
    const int32_t aux1=(hy!=res_h && hy)?mul(scale,hy):mul(scale,res_h);
    const int32_t aux0=(local_20!=res_w && local_20)?mul(scale,local_20):mul(scale,res_w);
    // 0x00450ad0 with the node dims, corners, aux pair and roll.
    int32_t angle=roll;
    if(angle>0x7fffff)angle=add(angle,-0x1000000);
    const int32_t index2=mul(angle,16)>>16;
    int32_t cosv=0,sinv=0;
    actor_sincos(sin_table,index2,cosv,sinv);
    const int32_t qw=masked_div(sub(c3,c1),res_w),qh=masked_div(sub(c2,c0),res_h);
    int32_t l38=mul(aux0,qw)>>8;
    const bool flip=sub(c3,c1)<0;
    if(flip)l38=sub(sub(c3,c1),l38);
    int32_t i5=mul(aux1,qh)>>24;
    const int32_t l38s=l38>>16;
    const int32_t x0=add(add(sub(mul(cosv,i5),mul(sinv,l38s)),c1),l38);
    const int32_t y0=add(sub(sub(c0,mul(cosv,l38s)),mul(sinv,i5)),mul(aux1,qh)>>8);
    const int32_t cosf=flip?sub(0,cosv):cosv;
    const int32_t t_qw=qw,t_qh=qh;
    const int32_t i5b=sub(x0,mul(mul(t_qw,cosf)>>8,res_h));
    const int32_t i9=add(mul(mul(t_qh,sinv)>>8,res_h),y0);
    // 0x00450ad0 final block: P1 = P0 + (qw*C>>8)*W,(qh*S>>8)*W and
    // P2 = that offset + (i5b,i9). o2 holds the X offset, o3 the Y offset
    // (C is local_34[0], S the flip-negated local_3c).
    const int32_t o2=mul(mul(t_qw,sinv)>>8,res_w);
    const int32_t o3=mul(mul(t_qh,cosf)>>8,res_w);
    const int32_t x1=add(x0,o2),y1=add(y0,o3),x2=add(o2,i5b),y2=add(o3,i9);
    return DrawNode{{{{x0,y0},{x1,y1},{x2,y2},{i5b,i9}}},texture,std::nullopt,bool(actor.flags&1),false};
}
}
