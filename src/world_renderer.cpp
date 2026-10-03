#include "world_renderer.hpp"
#include "texture.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace rr {
namespace {
int16_t narrow(int32_t v){const uint16_t bits=uint16_t(v);int16_t result;std::memcpy(&result,&bits,2);return result;}
}
void render_nodes(Framebuffer& fb,const std::vector<DrawNode>& nodes,const ViewMode& mode,uint32_t flat_mask) {
    if(fb.width()!=mode.surface_width || fb.height()!=mode.surface_height)throw std::runtime_error("View/framebuffer size mismatch");
    const RasterClip clip{0,0,fb.width(),fb.height()-1};
    for(const auto& node:nodes) {
        if(!node.texture && !node.flat_index)continue;
        std::array<RasterPoint,4> points;
        for(size_t i=0;i<4;++i)points[i]={mode.map(node.view_points[i].x),mode.map(node.view_points[i].y)};
        int32_t minx=points[0].x,maxx=minx,miny=points[0].y,maxy=miny;
        for(const auto& p:points){minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);miny=std::min(miny,p.y);maxy=std::max(maxy,p.y);}
        if(minx>clip.right || maxx<clip.left || miny>clip.bottom || maxy<clip.top)continue;
        if(!node.texture || (node.texture->flags&flat_mask)) {
            draw_flat_quad(fb,{points,node.texture?node.texture->flat_index:*node.flat_index},clip);continue;
        }
        const auto& texture=*node.texture;
        if(texture.flags&0x2000)throw std::runtime_error("Dynamic rider texture must be specialized before rendering");
        bool flip_u=node.flip_u;
        if(!normalize_quad(points,texture.flags,&flip_u))continue;
        const auto& bounds=texture.bounds;
        const size_t level=texture_mip_level(uint32_t(maxx-minx+1),uint32_t(maxy-miny+1),
            uint32_t(std::abs(int64_t(bounds[2])-bounds[0])+1),uint32_t(std::abs(int64_t(bounds[3])-bounds[1])+1),texture.mips.size());
        const uint16_t u0=uint16_t(flip_u?bounds[2]-1:bounds[0]+1),u1=uint16_t(flip_u?bounds[0]+1:bounds[2]-1);
        const uint16_t v0=uint16_t(node.flip_v?bounds[3]-1:bounds[1]+1),v1=uint16_t(node.flip_v?bounds[1]+1:bounds[3]-1);
        const std::array<std::array<uint16_t,2>,4> uv={{{u0,v0},{u1,v0},{u1,v1},{u0,v1}}};
        std::array<TextureVertex,4> vertices;
        for(size_t i=0;i<4;++i)vertices[i]={narrow(points[i].x),narrow(points[i].y),uint16_t(uv[i][0]>>level),uint16_t(uv[i][1]>>level)};
        const auto& mip=texture.mips.at(level);
        draw_textured_quad(fb,vertices,mip.pixels,mip.stride_shift,(texture.flags&2)!=0,clip);
    }
}
}
