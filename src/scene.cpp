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
}
