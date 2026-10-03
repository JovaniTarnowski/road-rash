#include "texture.hpp"
#include <stdexcept>
namespace rr {
void texture_span(uint8_t* destination,size_t count,const std::vector<uint8_t>& pixels,
    unsigned shift,int32_t row,int32_t column,int32_t row_step,int32_t column_step,bool transparent) {
    if(shift>8 || (count && !destination) || pixels.empty() || pixels.size()%(size_t(1)<<shift)) throw std::runtime_error("Invalid texture span storage");
    uint32_t u=uint32_t(row),v=uint32_t(column);
    for(size_t i=0;i<count;++i) {
        const uint32_t address=(((u&0xffff0000u)<<shift)+v)>>16;
        if(address>=pixels.size()) throw std::runtime_error("Texture span coordinate outside storage");
        const auto index=pixels[address];if(index || !transparent)destination[i]=index;
        u+=uint32_t(row_step);v+=uint32_t(column_step);
    }
}
}

#include <algorithm>
#include <cstring>
namespace rr {
namespace {
int32_t signed_word(uint32_t v){int32_t result;std::memcpy(&result,&v,4);return result;}
int32_t wrap_add(int32_t a,int32_t b){return signed_word(uint32_t(a)+uint32_t(b));}
int32_t wrap_sub(int32_t a,int32_t b){return signed_word(uint32_t(a)-uint32_t(b));}
int32_t wrap_mul(int32_t a,int32_t b){return signed_word(uint32_t(a)*uint32_t(b));}
int32_t fixed(int32_t a){return signed_word(uint32_t(a)<<16);}
int32_t product_q16(int32_t a,int32_t b){return signed_word(uint32_t(uint64_t(int64_t(a)*b)>>16));}
int32_t floor_q16(int32_t a){return a>=0?a/65536:int32_t((int64_t(a)-65535)/65536);}
}
void texture_edge(std::array<TextureEdge,480>& rows,const TextureVertex& from,const TextureVertex& to) {
    const int32_t dy=fixed(int32_t(to.y)-from.y);
    if(!dy || to.y<from.y)return;
    const int32_t reciprocal=int32_t(0x100000000LL/dy);
    TextureEdge at{fixed(from.x),fixed(from.u),fixed(from.v)};
    const TextureEdge step{product_q16(fixed(int32_t(to.x)-from.x),reciprocal),
        product_q16(fixed(int32_t(to.u)-from.u),reciprocal),
        product_q16(fixed(int32_t(to.v)-from.v),reciprocal)};
    const int first=std::max(0,int(from.y)),last=std::min(479,int(to.y));
    const int skip=first-from.y;
    at.x=wrap_add(at.x,wrap_mul(step.x,skip));at.u=wrap_add(at.u,wrap_mul(step.u,skip));at.v=wrap_add(at.v,wrap_mul(step.v,skip));
    for(int y=first;y<=last;++y) {
        rows[y]=at;at.x=wrap_add(at.x,step.x);at.u=wrap_add(at.u,step.u);at.v=wrap_add(at.v,step.v);
    }
}
size_t texture_mip_level(uint32_t width,uint32_t height,uint32_t source_width,uint32_t source_height,size_t levels) {
    if(!width || !height || !source_width || !source_height || !levels || levels>9)throw std::runtime_error("Invalid mip dimensions");
    auto axis=[&](uint32_t projected,uint32_t source) {
        size_t level=0;
        while(projected<(source>>1) && level+1<levels) {++level;source>>=1;}
        return level;
    };
    return std::min(axis(width,source_width),axis(height,source_height));
}
void draw_textured_quad(Framebuffer& fb,const std::array<TextureVertex,4>& vertices,
    const std::vector<uint8_t>& pixels,unsigned shift,bool transparent,RasterClip clip) {
    if(clip.left!=0 || shift>8 || pixels.empty() || pixels.size()%(size_t(1)<<shift))throw std::runtime_error("Invalid textured quad storage/clip");
    int first=0,bottom=vertices[0].y;
    for(int i=1;i<4;++i){if(vertices[i].y<vertices[first].y)first=i;bottom=std::max(bottom,int(vertices[i].y));}
    if(bottom==vertices[first].y)return;
    std::array<TextureEdge,480> left{},right{};
    auto chain=[&](std::array<TextureEdge,480>& rows,int direction) {
        int index=first;
        for(int guard=0;guard<4;++guard) {
            const int next=(index+direction+4)%4;
            if(vertices[next].y<vertices[index].y)throw std::runtime_error("Textured quad requires normalized monotone edges");
            texture_edge(rows,vertices[index],vertices[next]);index=next;
            if(vertices[index].y==bottom)return;
        }
        throw std::runtime_error("Invalid texture edge chain");
    };
    chain(left,-1);chain(right,1);
    const int top=std::max({0,int(vertices[first].y),int(clip.top)}),last=std::min({479,bottom,int(clip.bottom),fb.height()-1});
    const int bound=std::min(int(clip.right),fb.width());
    for(int y=top;y<=last;++y) {
        auto l=left[y],r=right[y];
        if(floor_q16(r.x)<=0)continue; // original pre-swap test
        if(floor_q16(r.x)<floor_q16(l.x))std::swap(l,r);
        int start=floor_q16(l.x),end=floor_q16(r.x),length=end-start;
        if(!length || start>=bound)continue;
        const int32_t reciprocal=65536/length;
        const int32_t du=product_q16(wrap_sub(r.u,l.u),reciprocal),dv=product_q16(wrap_sub(r.v,l.v),reciprocal);
        if(start<0){const int skip=-start;l.u=wrap_add(l.u,wrap_mul(du,skip));l.v=wrap_add(l.v,wrap_mul(dv,skip));start=0;}
        end=std::min(end,bound);
        if(start<end)texture_span(fb.row(y)+start,size_t(end-start),pixels,shift,l.v,l.u,dv,du,transparent);
    }
}
}
