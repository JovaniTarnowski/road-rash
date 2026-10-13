#include "building_panels.hpp"
#include <cstring>
namespace rr {
namespace {
int32_t bits(uint32_t u){int32_t i;std::memcpy(&i,&u,4);return i;}
int32_t add(int32_t a,int32_t b){return bits(uint32_t(a)+uint32_t(b));}
int32_t sub(int32_t a,int32_t b){return bits(uint32_t(a)-uint32_t(b));}
int32_t mul(int32_t a,int32_t b){return bits(uint32_t(a)*uint32_t(b));}
int32_t absolute(int32_t a){return a<0?sub(0,a):a;}
DrawNode node(std::array<RasterPoint,4> p,const CelTexture* t,std::array<int32_t,4> depth={}){DrawNode n={p,t,t?std::nullopt:std::optional<uint8_t>(0),p[1].x<p[0].x || p[2].x<p[3].x,p[3].y<p[0].y || p[2].y<p[1].y};n.view_depth=depth;return n;}
}
std::vector<DrawNode> building_draw_nodes(const BuildingPanelInput& in,const BuildingMaterial& material) {
    if(in.side>1 || in.logical_width<1 || in.logical_width>32767)throw std::runtime_error("Invalid building panel input");
    const auto& a=in.far;const auto& b=in.near;const bool right=in.side!=0;
    const int32_t width=in.logical_width*65536;
    std::vector<DrawNode> out;
    if(b.selector==255 && (b.height!=0 || ((b.marking_flags&16) && a.type!=0)))return out;
    // Exposed end of the previous building.
    const int32_t end_width=add(a.roof_offset,a.forward_roof_offset);
    if(!(b.marking_flags&8) && a.type==0 && a.height!=b.forward_height && sub(a.height,b.forward_height)>=0 &&
       (right?end_width>0 && a.shoulder.x<width:end_width<0 && a.shoulder.x>=0)) {
        const auto* texture=material(true,absolute(end_width),a.height);
        const int32_t top=sub(a.shoulder.y,a.height),x=a.shoulder.x;
        out.push_back(node({{{x,top},{add(x,end_width),top},{add(x,end_width),add(top,a.height)},{x,add(top,a.height)}}},texture));
    }
    RasterPoint far=a.shoulder,near=b.shoulder;
    if(b.marking_flags&8)far=b.transition;
    else if(b.marking_flags&16)near=b.transition;
    const bool faces=right?near.x>far.x:far.x>near.x;
    const bool visible=(near.x<=width || far.x<=width) && (near.x>=0 || far.x>=0);
    if(b.forward_height==0) {
        // Original flat exterior ground fill at an absent facade.
        if(near.y>far.y && faces && (right?far.x<width:far.x>0)) {
            const int32_t edge=right?width:0;
            if(right?near.x>edge:near.x<edge) {
                const int32_t dx=sub(far.x,near.x)>>12;
                if(dx) {
                    const int64_t numerator=int64_t(near.y>>12)*(far.x>>12)-int64_t(far.y>>12)*(near.x>>12)+int64_t(sub(far.y,near.y)>>12)*(edge>>12);
                    near.y=bits(uint32_t(numerator/dx)*4096u);near.x=edge;
                }
            }
            out.push_back({{{{far.x,far.y},{far.x,far.y},{near.x,near.y},{near.x,far.y}}},nullptr,uint8_t(253)});
        }
    } else {
        int32_t nh=b.height,fh=b.forward_height;
        if(b.cut_roof){nh=sub(nh,add(nh>>3,nh>>4));fh=sub(fh,add(fh>>3,fh>>4));}
        const int32_t nt=sub(near.y,nh),ft=sub(far.y,fh);
        if(ft<nt && faces && visible)out.push_back({{{{far.x,ft},{near.x,nt},{add(near.x,b.roof_offset),nt},{add(far.x,b.forward_roof_offset),ft}}},nullptr,uint8_t(253)});
    }
    if(b.selector!=255 && faces && visible) {
        const int32_t ft=sub(far.y,b.forward_height),nt=sub(near.y,b.height);
        auto facade=node({{{far.x,ft},{near.x,nt},{near.x,near.y},{far.x,far.y}}},material(false,absolute(sub(near.x,far.x)),b.height),{in.far_depth_q8,in.near_depth_q8,in.near_depth_q8,in.far_depth_q8});
        facade.flip_u=false;facade.flip_v=false;out.push_back(facade);
    }
    return out;
}
std::vector<DrawNode> building_shoulder_nodes(const BuildingShoulderInput& in,const Camera& camera,const DepthTable& depths,const BuildingShoulderMaterial& material) {
    const auto& p=in.top.projected;const bool right=in.top.side!=0;
    if(in.top.side>1 || in.logical_width<1 || in.logical_width>32767)throw std::runtime_error("Invalid building shoulder input");
    const int32_t width=in.logical_width*65536;
    std::vector<DrawNode> out;
    if(p[0].y<p[3].y && p[0].y<=in.top.occlusion_row &&
       (right?p[1].x>=0 && p[0].x<=width:p[0].x>=0 && p[1].x<=width)) {
        const auto* texture=material(0,absolute(sub(p[1].x,p[0].x)),absolute(sub(p[3].y,p[0].y)));
        if(in.near_depth<1234) {
            std::vector<std::array<RasterPoint,4>> quads;
            std::vector<std::array<int32_t,4>> qdepths;
            subdivide_shoulder_depths(quads, qdepths, in.top,camera,depths,in.logical_width,240);
            for(size_t qi=0;qi<quads.size();++qi)out.push_back(node(quads[qi],texture,qdepths[qi]));
        }
        else {auto quad=p;quad[1].y=quad[0].y;quad[2].y=quad[3].y;out.push_back(node(quad,texture,{in.top.far_depth,in.top.far_depth,in.near_depth,in.near_depth}));}
    }
    if((right?p[3].x>=p[0].x:p[3].x<=p[0].x) && (p[0].y<in.top.occlusion_row || p[3].y<in.top.occlusion_row)) {
        HillStrip curb=in.top;curb.adjust_seam=true;
        curb.projected={p[0],{p[0].x,in.far_base_y},{p[3].x,in.near_base_y},p[3]};
        curb.world={in.top.world[0],{in.top.world[0].x,in.far_world_base_y},{in.top.world[3].x,in.near_world_base_y},in.top.world[3]};
        const auto* texture=material(1,0,absolute(sub(p[3].y,p[0].y)));
        if(in.near_depth<1234) {
            std::vector<std::array<RasterPoint,4>> quads;
            std::vector<std::array<int32_t,4>> qdepths;
            subdivide_hill_depths(quads, qdepths, curb,camera,depths,in.logical_width);
            for(size_t qi=0;qi<quads.size();++qi)out.push_back(node(quads[qi],texture,qdepths[qi]));
        }
        else out.push_back(node(curb.projected,texture,{curb.far_depth,curb.far_depth,in.near_depth,in.near_depth}));
    }
    return out;
}

std::vector<DrawNode> marked_ground_nodes(const MarkedGroundInput& in,const std::function<const CelTexture*(uint8_t)>& material) {
    if(in.side>1 || in.logical_width<1 || in.logical_width>32767 || in.strips<1 || in.strips>16)throw std::runtime_error("Invalid marked terrain input");
    const int32_t ns=mul(in.near_scale,in.side?512:-512),fs=mul(in.far_scale,in.side?512:-512),width=in.logical_width*65536;
    std::array<RasterPoint,4> p={in.far_edge,{add(in.far_edge.x,fs),in.far_edge.y},{add(in.near_edge.x,ns),in.near_edge.y},in.near_edge};
    const std::array<int32_t,4> depth={in.far_depth_q8,in.far_depth_q8,in.near_depth_q8,in.near_depth_q8};
    std::vector<DrawNode> result;
    auto append=[&](uint8_t index){const bool visible=in.side?(p[0].x<width || p[3].x<width) && (p[1].x>0 || p[2].x>0):(p[1].x<width || p[2].x<width) && (p[0].x>0 || p[3].x>0);if(visible)result.push_back(node(p,material(index),depth));};
    append(0);
    uint8_t index=in.marking_flags&7;if(index)--index;
    if(index>2)throw std::runtime_error("Invalid marked terrain selector");
    for(int i=1;i<in.strips;++i) {
        if(in.side?p[0].x>=width:p[1].x<=0)break;
        for(int j=0;j<4;++j)p[j].x=add(p[j].x,j<2?fs:ns);
        append(uint8_t(index+1));
    }
    return result;
}

}
