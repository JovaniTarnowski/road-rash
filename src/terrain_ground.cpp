#include "terrain_ground.hpp"
#include <cstring>
#include <stdexcept>
namespace rr {
namespace {
int32_t bits(uint32_t v){int32_t r;std::memcpy(&r,&v,4);return r;}
int32_t add(int32_t a,int32_t b){return bits(uint32_t(a)+uint32_t(b));}
int32_t sub(int32_t a,int32_t b){return bits(uint32_t(a)-uint32_t(b));}
int32_t mul(int32_t a,int32_t b){return bits(uint32_t(a)*uint32_t(b));}
int32_t blend(int32_t a,int32_t b,uint8_t fraction){return add(a,mul(sub(b,a),fraction)/256);}
}
int32_t terrain_height(const TerrainSlice& t,const PathSample& path,const TerrainSlice& next,const PathSample& np,int32_t lateral,uint8_t fraction,int32_t centre) {
    const int32_t x=lateral>>8,left=-path.left,right=path.right;
    if(left<=x && x<=right)return 0;
    const bool side=x>right;
    const int32_t shoulder=side?right+path.shoulder_right:left-path.shoulder_left;
    const int32_t height=side?path.height_right:path.height_left;
    if(t.type==0) {
        if(path.marking_flags&32)return 0;
        if((side?x>=shoulder:x<=shoulder) && !(path.marking_flags&24) && t.selectors[side]!=255)return 999;
        return height;
    }
    if(t.type==3)return height;
    if(t.type!=4)return 0;
    const int32_t next_edge=add(centre,side?np.right:-np.left);
    const int32_t edge=blend(side?right:left,next_edge,fraction);
    if(side?x<=edge:x>=edge)return 0;
    int32_t inner=blend(shoulder,add(centre,side?np.right+np.shoulder_right:-np.left-np.shoulder_left),fraction);
    if(side?x<inner:x>inner)return height;
    int band=0;
    int32_t outer=blend(t.points[side][0],add(centre,next.points[side][0]),fraction);
    while(band<2 && (side?outer<x:x<outer)) {
        ++band;inner=outer;
        outer=blend(t.points[band*2+side][0],add(centre,next.points[band*2+side][0]),fraction);
    }
    if(side?outer<x:x<outer)return blend(t.points[4+side][1],next.points[4+side][1],fraction);
    const int32_t inner_height=band?blend(t.points[(band-1)*2+side][1],next.points[(band-1)*2+side][1],fraction):blend(height,side?np.height_right:np.height_left,fraction);
    const int32_t outer_height=blend(t.points[band*2+side][1],next.points[band*2+side][1],fraction);
    const int32_t span=sub(outer,inner);
    if(!span)throw std::runtime_error("Degenerate terrain ground segment");
    return add(mul(sub(outer_height,inner_height),sub(x,inner))/span,inner_height);
}
}
