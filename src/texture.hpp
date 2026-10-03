#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "raster.hpp"
namespace rr {
// Original 0x00418910..0x00419e10 and transparent variants: power-of-two
// row stride 1..256, Q16 row/column and signed wrapped per-pixel increments.
void texture_span(uint8_t* destination,size_t count,const std::vector<uint8_t>& pixels,
    unsigned stride_shift,int32_t row_q16,int32_t column_q16,
    int32_t row_step_q16,int32_t column_step_q16,bool transparent);
}

namespace rr {
struct TextureVertex { int16_t x=0,y=0; uint16_t u=0,v=0; };
struct TextureEdge { int32_t x=0,u=0,v=0; };
void texture_edge(std::array<TextureEdge,480>& rows,const TextureVertex& from,const TextureVertex& to);
// 0x004180b0 selects the smaller of two axis LODs.
size_t texture_mip_level(uint32_t projected_width,uint32_t projected_height,
    uint32_t source_width,uint32_t source_height,size_t available_levels);
// Normalized convex vertices with UVs already selected (no mip selection here).
void draw_textured_quad(Framebuffer&,const std::array<TextureVertex,4>&,
    const std::vector<uint8_t>& pixels,unsigned stride_shift,bool transparent,RasterClip);
}
