#include "terrain_panels.hpp"
#include <algorithm>
#include <cstring>

namespace rr {
namespace {
int32_t bits(uint32_t v){int32_t r;std::memcpy(&r,&v,4);return r;}
int32_t add(int32_t a,int32_t b){return bits(uint32_t(a)+uint32_t(b));}
int32_t sub(int32_t a,int32_t b){return bits(uint32_t(a)-uint32_t(b));}
int32_t mul(int32_t a,int32_t b){return bits(uint32_t(a)*uint32_t(b));}
int32_t length(RasterPoint a,RasterPoint b) {
    auto absolute=[](int32_t n){return n<0?sub(0,n):n;};
    const int32_t x=absolute(sub(a.x,b.x)),y=absolute(sub(a.y,b.y));
    return add(sub(x,std::min(x,y)/2),y);
}
uint32_t octant(RasterPoint a,RasterPoint b) {
    const int32_t x=sub(b.x,a.x),y=sub(b.y,a.y);
    if(x<1)return y<1?uint32_t(y<x)+6:uint32_t(y<=sub(0,x))+4;
    if(y<1)return uint32_t(sub(0,x)==y || sub(0,y)<x);
    return uint32_t(x<y)+2;
}
void overlap(std::array<RasterPoint,4>& p,const std::array<RasterPoint,16>& offsets,bool reverse) {
    std::array<uint32_t,4> directions;
    for(size_t i=0;i<4;++i)directions[i]=octant(p[i],p[(i+1)%4]);
    for(size_t i=0;i<4;++i) {
        const uint32_t a=(directions[(i+3)%4]+4)&7,b=directions[i];
        if(a==b)continue;
        const uint32_t index=(a+b+((reverse ? b>=a:b<a)?8:0))&15;
        p[i]={add(p[i].x,offsets[index].x),add(p[i].y,offsets[index].y)};
    }
}
bool facing(const std::array<RasterPoint,4>& p,bool right) {
    const int32_t a=mul(sub(p[0].y,p[3].y)>>12,sub(p[2].x,p[3].x)>>12);
    const int32_t b=mul(sub(p[2].y,p[3].y)>>12,sub(p[0].x,p[3].x)>>12);
    const int32_t c=mul(sub(p[0].y,p[1].y)>>12,sub(p[2].x,p[1].x)>>12);
    const int32_t d=mul(sub(p[2].y,p[1].y)>>12,sub(p[0].x,p[1].x)>>12);
    return right ? (a<b || (sub(c,d)!=0 && d<=c)) : ((sub(a,b)!=0 && b<=a) || c<d);
}
DrawNode node(const std::array<RasterPoint,4>& p,const CelTexture* texture,std::array<int32_t,4> depth={}) {
    DrawNode n={p,texture,texture?std::nullopt:std::optional<uint8_t>(0),
        p[1].x<p[0].x || p[2].x<p[3].x,p[3].y<p[0].y || p[2].y<p[1].y};
    n.view_depth=depth;
    return n;
}
}
std::vector<DrawNode> hill_draw_nodes(const HillPanelInput& in,const Camera& camera,
    const DepthTable& depths,const HillMaterial& material) {
    if(in.side>1 || in.logical_width<1 || in.logical_width>32767)
        throw std::runtime_error("Invalid hill panel input");
    std::array<RasterPoint,5> near{},far{};
    std::copy(in.near.projected.begin(),in.near.projected.end(),near.begin());
    std::copy(in.far.projected.begin(),in.far.projected.end(),far.begin());
    if(in.far.type==0)for(int i=1;i<4;++i)far[i]={near[i].x,far[0].y};
    else if(in.far.type==1)far[3]=far[2];
    int bands=3;const int32_t width=in.logical_width*65536;
    if(in.far.type!=0 && in.far.type!=1 &&
        (in.side==0 ? near[3].x>0 || far[3].x>0 : near[3].x<width || far[3].x<width)) {
        auto edge=[&](const std::array<RasterPoint,5>& p) {const int32_t x=sub(mul(p[3].x,2),p[2].x);return RasterPoint{in.side==0?std::min(0,x):std::max(width,x),p[2].y};};
        near[4]=edge(near);far[4]=edge(far);bands=4;
    }
    std::vector<DrawNode> result;
    for(int band=bands-1;band>=0;--band) {
        std::array<RasterPoint,4> p={far[band],far[band+1],near[band+1],near[band]};
        const bool on_screen=in.side==0 ?
            ((p[0].x>=0 || p[3].x>=0) && (p[1].x<=width || p[2].x<=width)):
            ((p[0].x<=width || p[3].x<=width) && (p[1].x>=0 || p[2].x>=0));
        if(!on_screen || !facing(p,in.side==1) ||
            std::all_of(p.begin(),p.end(),[&](RasterPoint point){return point.y>=in.occlusion_row;}))continue;
        if(in.near.depth<1234 && band<3) {
            const int shift=in.far.depth<978 ? (in.far.depth<466?3:2):1;
            const auto* texture=material(uint8_t(band),length(p[0],p[1]),std::max(length(p[1],p[2]),int32_t(1<<(shift+16))));
            if(!texture){result.push_back(node(p,nullptr));continue;}
            HillStrip strip;
            strip.world={in.far.world[band],in.far.world[band+1],in.near.world[band+1],in.near.world[band]};
            strip.projected=p;strip.side=in.side;strip.far_depth=in.far.depth;strip.occlusion_row=in.occlusion_row;
            std::vector<std::array<RasterPoint,4>> quads;
            std::vector<std::array<int32_t,4>> qdepths;
            subdivide_hill_depths(quads, qdepths, strip,camera,depths,in.logical_width);
            for(size_t qi=0;qi<quads.size();++qi)result.push_back(node(quads[qi],texture,qdepths[qi]));
        } else {
            const int selected_band=band<3?band:2;
            if(band>=3)p={p[1],p[0],p[3],p[2]};
            if(in.overlap)overlap(p,in.join_offsets,(band>2)==(in.side==1));
            const auto* texture=material(uint8_t(selected_band),length(p[0],p[1]),std::max(length(p[1],p[2]),length(p[0],p[3])));
            result.push_back(node(p,texture,{in.far.depth,in.far.depth,in.near.depth,in.near.depth}));
        }
    }
    return result;
}
std::vector<DrawNode> shoulder_draw_nodes(const ShoulderPanelInput& in,const Camera& camera,
    const DepthTable& depths,const ShoulderMaterial& material) {
    auto strip=in.strip;
    if(strip.side>1 || in.logical_width<1 || in.logical_width>32767)throw std::runtime_error("Invalid shoulder panel input");
    auto& p=strip.projected;
    p[0].y=bits(uint32_t(p[0].y)&0xffffc000u);p[3].y=bits(uint32_t(p[3].y)&0xffffc000u);
    p[1].y=p[0].y;p[2].y=p[3].y;
    const int32_t width=in.logical_width*65536;
    if(strip.side==0 ? p[0].x<0 || p[1].x>width : p[1].x<0 || p[0].x>width)return {};
    if(in.has_lanes) {const int32_t dx=strip.side==0?-65536:65536;p[1].x=add(p[1].x,dx);p[2].x=add(p[2].x,dx);}
    auto absolute=[](int32_t v){return v<0?sub(0,v):v;};
    const auto* texture=material(absolute(sub(p[1].x,p[0].x)),absolute(sub(p[3].y,p[0].y)));
    if(in.near_depth>=1234)return {node(p,texture,{strip.far_depth,strip.far_depth,in.near_depth,in.near_depth})};
    if(!texture)return {};
    std::vector<DrawNode> out;
    std::vector<std::array<RasterPoint,4>> quads;
    std::vector<std::array<int32_t,4>> qdepths;
    subdivide_shoulder_depths(quads, qdepths, strip,camera,depths,in.logical_width,in.logical_height);
    for(size_t qi=0;qi<quads.size();++qi)out.push_back(node(quads[qi],texture,qdepths[qi]));
    return out;
}

std::vector<DrawNode> wall_draw_nodes(const WallPanelInput& in,const WallMaterial& material,const CelTexture* water) {
    if(in.side>1 || in.logical_width<1 || in.logical_width>32767 || in.logical_height<1 || in.logical_height>32767)
        throw std::runtime_error("Invalid wall panel input");
    const int32_t width=in.logical_width*65536;
    const bool right=in.side==1;
    // ((a>>12)*(b>>12)) products of the original visibility tests.
    auto cross=[](int32_t a,int32_t b){return mul(a>>12,b>>12);};
    auto absolute=[](int32_t n){return n<0?sub(0,n):n;};
    std::vector<DrawNode> result;
    const auto& np=in.near.projected;const auto& fp=in.far.projected;
    if(in.height<1) {
        // 0x0040d030: far base, near base (both 0x6000 up), near top, far top.
        std::array<RasterPoint,4> p={RasterPoint{fp[1].x,sub(fp[1].y,0x6000)},RasterPoint{np[1].x,sub(np[1].y,0x6000)},np[2],fp[2]};
        bool draw;
        const int32_t a=cross(sub(p[1].y,p[2].y),sub(p[3].x,p[2].x)),b=cross(sub(p[3].y,p[2].y),sub(p[1].x,p[2].x));
        const int32_t c=cross(sub(p[3].y,p[0].y),sub(p[1].x,p[0].x)),d=cross(sub(p[1].y,p[0].y),sub(p[3].x,p[0].x));
        if(right)draw=p[1].x<p[0].x ? p[1].x<width : (a<b || c<d);
        else draw=p[0].x<p[1].x ? p[1].x>0 : (a>b || c>d);
        if(draw)result.push_back(node(p,material(0,length(p[0],p[1]),length(p[1],p[2])),{in.far.depth,in.near.depth,in.near.depth,in.far.depth}));
        if(p[3].y<in.logical_height*65536) {
            // Water from the cliff foot to the view edge; 0x00450140 makes
            // both edges horizontal. Right: x clamps to the edge unless within
            // 256 columns of it. Left (0x0040d2f2): only when both feet are
            // already left of column 0, so the original never shows it.
            bool fill=false;int32_t x1=0,x2=0;
            if(right) {
                if(p[3].x<width || p[2].x<width) {
                    fill=true;const int32_t limit=(in.logical_width-256)*65536;
                    x1=p[2].x<limit?width:add(p[2].x,0x1000000);
                    x2=p[3].x<limit?width:add(p[3].x,0x1000000);
                }
            } else if(p[2].x<0 && p[3].x<0) {fill=true;x1=sub(p[2].x,0x1000000);x2=sub(p[3].x,0x1000000);}
            if(fill)result.push_back(node({p[2],RasterPoint{x1,p[2].y},RasterPoint{x2,p[3].y},p[3]},water,{in.near.depth,in.near.depth,in.far.depth,in.far.depth}));
        }
        return result;
    }
    // 0x0040dfd0: nothing within 0x3d of the camera.
    if(in.near.depth<0x3d)return result;
    // Far top/base: a hill neighbour joins at its shoulder and first point.
    const RasterPoint far_top=in.far.type==4?fp[1]:fp[2],far_base=in.far.type==4?fp[0]:fp[1];
    std::array<RasterPoint,4> p={far_top,np[2],RasterPoint{np[1].x,add(np[1].y,0x10000)},RasterPoint{far_base.x,add(far_base.y,0x10000)}};
    const int32_t a=cross(sub(p[2].x,p[1].x),sub(p[0].y,p[1].y)),b=cross(sub(p[2].y,p[1].y),sub(p[0].x,p[1].x));
    const int32_t c=cross(sub(p[2].x,p[3].x),sub(p[0].y,p[3].y)),d=cross(sub(p[2].y,p[3].y),sub(p[0].x,p[3].x));
    bool draw;
    if(right)draw=p[3].x<p[2].x ? p[3].x<width : (a>b || c<d);
    else draw=p[2].x<p[3].x ? p[3].x>0 : (a<b || c>d);
    if(draw) {
        // Near edge pushed one column outward.
        const int32_t shift=right?65536:-65536;
        p[2].x=add(p[2].x,shift);p[1].x=add(p[1].x,shift);
        result.push_back(node(p,material(0,length(p[0],p[1]),length(p[1],p[2])),{in.far.depth,in.near.depth,in.near.depth,in.far.depth}));
    }
    // Plateau above a wall that rises towards the camera: tiles four lanes
    // wide (+0x24*4 per pair end) until both ends leave the view.
    if(p[0].y<p[1].y && p[1].y>0) {
        const int32_t near_step=mul(in.near_lane_width,right?4:-4),far_step=mul(in.far_lane_width,right?4:-4);
        std::array<RasterPoint,4> q={p[0],RasterPoint{add(p[0].x,far_step),p[0].y},RasterPoint{add(p[1].x,near_step),p[1].y},p[1]};
        const auto* texture=material(1,absolute(sub(q[1].x,q[0].x)),absolute(sub(q[3].y,q[0].y)));
        // The original loops until off-screen; bound it against a zero step.
        for(int tiles=0;tiles<4096 && (right ? q[0].x<width || q[3].x<width : q[0].x>0 || q[3].x>0);++tiles) {
            result.push_back(node(q,texture,{in.far.depth,in.far.depth,in.near.depth,in.near.depth}));
            q[0].x=add(q[0].x,far_step);q[1].x=add(q[1].x,far_step);q[2].x=add(q[2].x,near_step);q[3].x=add(q[3].x,near_step);
        }
    }
    return result;
}
}
