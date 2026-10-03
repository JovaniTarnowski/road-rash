#pragma once
#include "framebuffer.hpp"
namespace rr {
struct RasterPoint { int32_t x=0,y=0; };
struct RasterQuad { std::array<RasterPoint,4> points{}; uint8_t colour=0; };
// Right edge is exclusive for spans, bottom row inclusive. Points are integer
// surface coordinates, after 0x0040ffc0 and polygon normalization 0x0044ce60.
struct RasterClip { int32_t left=0,top=0,right=0,bottom=0; };
// 0x0044ce60: normalizes topology/orientation and optionally texture flip.
bool normalize_quad(std::array<RasterPoint,4>&,uint32_t flags=0,bool* texture_flip=nullptr);
void fill_quad(Framebuffer&,const RasterQuad&,RasterClip);
// Full flat entry path: normalize, then fill.
void draw_flat_quad(Framebuffer&,RasterQuad,RasterClip);
}
