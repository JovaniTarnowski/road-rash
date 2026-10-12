#pragma once
#include "framebuffer.hpp"
#include <utility>
#include <vector>
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
// Band-parallel rasterisation (plan 19 Task 6): split the framebuffer into
// `bands` horizontal row ranges rendered on workers. Each band renders the
// whole node list in order, clipped to its rows, so output is bit-identical
// to one thread. Edge steps are computed from the ORIGINAL polygon endpoints
// before clipping to the band's rows; recomputing from clipped endpoints
// changes rounding. Each worker owns a preallocated RasterScratch (see
// texture.hpp) sized to the surface height; no shared edge arrays, no SDL
// calls, no lazy cache mutation and no scene construction on workers.
// Texture resources stay immutable and alive until workers join. Raster
// writes honor top/bottom inclusive and right exclusive. Throws
// std::runtime_error on bands < 1. Uneven bands (e.g. 7 over 480 rows) are
// distributed one extra row to the first `height % bands` bands.
std::vector<std::pair<int,int>> band_rows(int height,int bands);
}
