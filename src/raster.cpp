#include "raster.hpp"
#include <algorithm>
#include <cstring>
namespace rr {
namespace {
int32_t bits(uint32_t value) { int32_t result;std::memcpy(&result,&value,4);return result; }
int32_t add(int32_t a,int32_t b) { return bits(uint32_t(a)+uint32_t(b)); }
int32_t q16(int32_t value) { return bits(uint32_t(value)<<16); }
int32_t integer(int32_t fixed) { return fixed>=0 ? fixed/65536 : int32_t((int64_t(fixed)-65535)/65536); }
}
// 0x0044d620 (flat polygon fill) / 0x0044db30 (constant byte span).
void fill_quad(Framebuffer& fb,const RasterQuad& quad,RasterClip clip) {
    clip.left=std::max(clip.left,0);clip.top=std::max(clip.top,0);
    clip.right=std::min(clip.right,fb.width());clip.bottom=std::min(clip.bottom,fb.height()-1);
    if (clip.left>=clip.right || clip.top>clip.bottom) return;
    const auto& points=quad.points;
    int32_t min_y=points[0].y,max_y=min_y,min_x=points[0].x,max_x=min_x;
    for (const auto& p:points) {
        if (p.x<-65536 || p.x>65535 || p.y<-65536 || p.y>65535) throw std::runtime_error("Raster coordinate exceeds original coordinate conversion range");
        min_y=std::min(min_y,p.y);max_y=std::max(max_y,p.y);min_x=std::min(min_x,p.x);max_x=std::max(max_x,p.x);
    }
    if (max_y<clip.top || min_y>clip.bottom || max_x<=clip.left || min_x>=clip.right) return;
    auto span=[&](int32_t y,int32_t left,int32_t right) {
        left=std::max(left,clip.left);right=std::min(right,clip.right);
        if (y>=clip.top && y<=clip.bottom && left<right) std::fill(fb.row(y)+left,fb.row(y)+right,quad.colour);
    };
    if (min_y==max_y) { span(min_y,min_x,max_x);return; }
    struct Edge { int32_t first=0,last=0,x=0,step=0;bool enabled=false; };
    std::array<Edge,4> edges{};
    for (size_t i=0;i<4;++i) {
        const size_t j=(i+1)%4;
        if (points[i].y==points[j].y) continue;
        const bool forward=points[i].y<points[j].y;
        const size_t lower=forward ? i : j,upper=forward ? j : i;
        size_t neighbour=upper;
        for (int n=0;n<3;++n) {
            neighbour=forward ? (neighbour+1)%4 : (neighbour+3)%4;
            if (points[neighbour].x!=points[upper].x || points[neighbour].y!=points[upper].y) break;
        }
        auto& edge=edges[i];edge.enabled=true;edge.first=points[lower].y;edge.last=points[upper].y;
        if (edge.last<points[neighbour].y) --edge.last;
        const int32_t dx=points[upper].x-points[lower].x,dy=points[upper].y-points[lower].y;
        // The executable divides signed quotient and remainder separately.
        // Emulate its 32-bit shifts/wrap without shifting a negative C++ value.
        edge.step=add(q16(dx/dy),bits(uint32_t(q16(dx%dy)/dy)));
        edge.x=q16(points[lower].x);
    }
    std::array<int,2> active{-1,-1};
    for (int32_t y=min_y;y<=std::min(max_y,clip.bottom);++y) {
        for (size_t i=0;i<4;++i) if (edges[i].enabled && edges[i].first==y) {
            if (active[0]<0) active[0]=int(i);else if (active[1]<0) active[1]=int(i);
            else throw std::runtime_error("Raster quad requires polygon normalization");
        }
        if (active[0]>=0 && active[1]>=0) {
            const int32_t a=integer(edges[size_t(active[0])].x),b=integer(edges[size_t(active[1])].x);
            span(y,std::min(a,b),std::max(a,b));
        }
        for (auto& index:active) if (index>=0) {
            auto& edge=edges[size_t(index)];
            if (edge.last==y) index=-1;else edge.x=add(edge.x,edge.step);
        }
    }
}
std::vector<std::pair<int,int>> band_rows(int height,int bands) {
    if (height<0 || height>8192 || bands<1 || bands>256) throw std::runtime_error("Band geometry out of range");
    std::vector<std::pair<int,int>> out;
    if (!height) return out;
    const int base=height/bands,extra=height%bands;
    int top=0;
    for (int b=0;b<bands && top<height;++b) {
        const int rows=base+(b<extra?1:0);
        if (!rows) break;
        out.emplace_back(top,top+rows-1);
        top+=rows;
    }
    return out;
}
}
