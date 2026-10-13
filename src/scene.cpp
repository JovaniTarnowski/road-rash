#include "scene.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace rr {
namespace {
int32_t bits(uint32_t v) {int32_t r;std::memcpy(&r,&v,4);return r;}
int32_t add(int32_t a,int32_t b) {return bits(uint32_t(a)+uint32_t(b));}
int32_t sub(int32_t a,int32_t b) {return bits(uint32_t(a)-uint32_t(b));}
int32_t mul(int32_t a,int32_t b) {return bits(uint32_t(a)*uint32_t(b));}
}
HillProjection project_hills(const TerrainSlice& terrain,int32_t world_x,int32_t world_y,
    const Camera& camera,int32_t scale,int32_t width) {
    if(terrain.type!=4 || scale<=0 || width<=0) throw std::runtime_error("Invalid hill projection input");
    const int32_t centre=bits((uint32_t(width)&0xfffffffeu)<<15);
    const int32_t dx=sub(world_x,camera.x),dy=sub(camera.height,world_y);
    HillProjection out;out.maximum_row=add(mul(dy,scale),view_horizon*65536);
    for(size_t i=0;i<6;++i) {
        const int32_t x=add(terrain.points[i][0],dx),product=mul(x,scale);
        const int32_t projected=add(product,centre);
        if(scale<65536) out.points[i][0]=projected;
        else if(x<0) out.points[i][0]=product<0 ? projected : -0x40000000;
        else out.points[i][0]=projected<0 ? 0x40000000 : projected;
        out.points[i][1]=add(mul(sub(dy,terrain.points[i][1]),scale),view_horizon*65536);
        out.maximum_row=std::max(out.maximum_row,out.points[i][1]);
    }
    return out;
}
std::vector<std::array<RasterPoint,4>> subdivide_hill(const HillStrip& strip,
    const Camera& camera,const DepthTable& table,int32_t width) {
    std::vector<std::array<RasterPoint,4>> quads;
    std::vector<std::array<int32_t,4>> depths;
    subdivide_hill_depths(quads, depths, strip, camera, table, width);
    return quads;
}
void subdivide_hill_depths(std::vector<std::array<RasterPoint,4>>& quads,
    std::vector<std::array<int32_t,4>>& depths, const HillStrip& strip,
    const Camera& camera,const DepthTable& table,int32_t width) {
    if(width<=0 || width>32767 || strip.side>1 || strip.far_depth<0)
        throw std::runtime_error("Invalid hill strip input");
    const int shift=strip.far_depth<978 ? (strip.far_depth<466 ? 3:2):1;
    const int count=1<<shift,step=256>>shift;
    const int32_t centre=(width&~1)*32768;
    const RasterPoint inner_delta{sub(strip.world[3].x,strip.world[0].x),sub(strip.world[3].y,strip.world[0].y)};
    const RasterPoint outer_delta{sub(strip.world[2].x,strip.world[1].x),sub(strip.world[2].y,strip.world[1].y)};
    RasterPoint inner=inner_delta,outer=outer_delta;
    std::array<RasterPoint,4> quad=strip.projected;
    auto& out = quads;
    int32_t depth=sub(strip.far_depth,step);
    auto project=[&](const RasterPoint& base,const RasterPoint& delta,int32_t scale) {
        const int32_t x=add(sub(delta.x>>shift,camera.x),base.x),product=mul(x,scale);
        int32_t px;
        if(x<0) px=product<0 ? add(centre,product):-0x40000000;
        else {px=add(centre,product);if(px<0)px=0x40000000;}
        const int32_t y=mul(sub(sub(camera.height,delta.y>>shift),base.y),scale);
        return RasterPoint{px,add(y,view_horizon*65536)};
    };
    for(int i=1;i<=count;++i) {
        if(depth<211 || (quad[0].y>=strip.occlusion_row && quad[1].y>=strip.occlusion_row)) break;
        if(strip.world[0].x<strip.world[1].x ? quad[0].x>=width*65536 : quad[0].x<1) break;
        if(i==count) {quad[2]=strip.projected[2];quad[3]=strip.projected[3];}
        else {
            const size_t index=size_t(depth&~1)/2;
            if(index>=table.scale.size())throw std::runtime_error("Hill strip depth outside table");
            quad[3]=project(strip.world[0],inner,table.scale[index]);
            quad[2]=project(strip.world[1],outer,table.scale[index]);
            if(strip.adjust_seam) {
                quad[3].y=sub(quad[3].y,0xc000);quad[2].y=add(quad[2].y,0xc000);
                const int32_t dx=strip.side==0?-0x8000:0x8000;
                quad[3].x=add(quad[3].x,dx);quad[2].x=add(quad[2].x,dx);
            }
        }
        out.push_back(quad);
        depths.push_back({add(depth,step),add(depth,step),depth,depth});
        quad[0]=quad[3];quad[1]=quad[2];depth=sub(depth,step);
        inner={add(inner.x,inner_delta.x),add(inner.y,inner_delta.y)};
        outer={add(outer.x,outer_delta.x),add(outer.y,outer_delta.y)};
    }
}
std::vector<std::array<RasterPoint,4>> subdivide_shoulder(const HillStrip& strip,
    const Camera& camera,const DepthTable& table,int32_t width,int32_t height) {
    std::vector<std::array<RasterPoint,4>> quads;
    std::vector<std::array<int32_t,4>> depths;
    subdivide_shoulder_depths(quads, depths, strip, camera, table, width, height);
    return quads;
}
void subdivide_shoulder_depths(std::vector<std::array<RasterPoint,4>>& quads,
    std::vector<std::array<int32_t,4>>& depths, const HillStrip& strip,
    const Camera& camera,const DepthTable& table,int32_t width,int32_t height) {
    if(width<=0 || width>32767 || height<=0 || height>32767 || strip.side>1 || strip.far_depth<0)
        throw std::runtime_error("Invalid shoulder strip input");
    const int shift=strip.far_depth<978 ? (strip.far_depth<466?3:2):1;
    const int count=1<<shift,step=256>>shift;
    const int32_t centre=(width&~1)*32768;
    const int32_t dx=sub(strip.world[3].x,strip.world[0].x),dy=sub(strip.world[3].y,strip.world[0].y),ox=sub(strip.world[2].x,strip.world[1].x);
    int32_t inner=dx,vertical=dy,outer=ox,depth=sub(strip.far_depth,step);
    std::array<RasterPoint,4> quad=strip.projected;
    auto& out = quads;
    auto project_x=[&](int32_t base,int32_t delta,int32_t scale) {
        const int32_t x=add(base,sub(delta>>shift,camera.x)),product=mul(x,scale);
        if(x<0)return product<0?add(centre,product):-0x40000000;
        const int32_t p=add(centre,product);return p<0?0x40000000:p;
    };
    for(int i=1;i<=count;++i) {
        if(quad[0].y>=height*65536 || (strip.side==1?quad[0].x>=width*65536:quad[0].x<1) || depth<211)break;
        if(i==count){quad[2]=strip.projected[2];quad[3]=strip.projected[3];}
        else {
            const size_t index=size_t(depth&~1)/2;
            if(index>=table.scale.size())throw std::runtime_error("Shoulder strip depth outside table");
            const int32_t scale=table.scale[index];
            quad[3].x=project_x(strip.world[0].x,inner,scale);
            quad[3].y=bits(uint32_t(add(mul(sub(sub(camera.height,vertical>>shift),strip.world[0].y),scale),view_horizon*65536))&0xffffc000u);
            quad[2].x=add(project_x(strip.world[1].x,outer,scale),strip.side==0?-0x40000:0x40000);
        }
        quad[1].y=quad[0].y;quad[2].y=quad[3].y;
        out.push_back(quad);
        depths.push_back({add(depth,step),add(depth,step),depth,depth});
        quad[0]=quad[3];quad[1]=quad[2];depth=sub(depth,step);
        inner=add(inner,dx);vertical=add(vertical,dy);outer=add(outer,ox);
    }
}
}
