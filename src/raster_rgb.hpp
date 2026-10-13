// Plan 21 Task 4: RGB raster with bilinear/trilinear filtering.
// Texels are premultiplied at conversion; filtering blends four texels, then
// alpha-tests (discard if alpha < 0.5) and un-premultiplies — no dark halos.
#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "cel.hpp"
#include "framebuffer.hpp"
#include "framebuffer_rgb.hpp"
#include "raster.hpp"
#include "render_threads.hpp"
#include "settings.hpp"
#include "view.hpp"
#include "world_renderer.hpp"

namespace rr {
struct RgbaMip {
    int width = 0, height = 0;
    std::vector<uint32_t> premultiplied;  // alpha 0 where index 0 was transparent
};
// Converted once per (texture, palette generation); remaps apply to the index
// image before conversion (caller converts the remapped CelTexture).
const std::vector<RgbaMip>& rgba_mips(const CelTexture&, const Palette&, uint32_t palette_generation);
// Test hook: convert one mip without caching.
std::vector<RgbaMip> rgba_mips_for_test(const CelMip&, const Palette&);
struct RgbVertex { int32_t x = 0, y = 0; float u = 0, v = 0; };
void draw_rgb_quad_bilinear(FramebufferRgb&, const std::array<RgbVertex, 4>&, const std::vector<RgbaMip>&,
                            bool transparent, RasterClip);
void draw_rgb_quad_trilinear(FramebufferRgb&, const std::array<RgbVertex, 4>&, const std::vector<RgbaMip>&,
                             bool transparent, RasterClip, float lod);
void render_nodes_rgb(FramebufferRgb&, const std::vector<DrawNode>&, const ViewMode&, uint32_t flat_mask,
                      const Palette&, const Enhancements&, RenderThreads& pool);
}  // namespace rr
