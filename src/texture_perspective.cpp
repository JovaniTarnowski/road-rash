// Plan 21 Task 3: perspective-correct textured quads. Edge X walks the
// SAME Q16 reciprocal arithmetic as the affine raster (exact coverage);
// u/z, v/z, 1/z walk in double along the same edges and divide every 16
// pixels (classic span subdivision). Equal inv_z at all corners delegates
// to the affine path (float cannot reproduce integer wrap/rounding).
#include "texture.hpp"
#include "cel.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace rr {
namespace {
int32_t p_signed(uint32_t v) {
    int32_t r;
    std::memcpy(&r, &v, 4);
    return r;
}
int32_t p_wrap_add(int32_t a, int32_t b) {
    return p_signed(uint32_t(a) + uint32_t(b));
}
int32_t p_wrap_mul(int32_t a, int32_t b) {
    return p_signed(uint32_t(a) * uint32_t(b));
}
int32_t p_fixed(int32_t a) {
    return p_signed(uint32_t(a) << 16);
}
int32_t p_product_q16(int32_t a, int32_t b) {
    return p_signed(uint32_t(uint64_t(int64_t(a) * b) >> 16));
}
int32_t p_floor(int32_t a) {
    return a >= 0 ? a / 65536 : int32_t((int64_t(a) - 65535) / 65536);
}
} // namespace

void draw_textured_quad_perspective(Framebuffer& fb, const std::array<PerspectiveVertex, 4>& vertices,
                                    const CelMip& mip, bool transparent, RasterClip clip) {
    if (mip.pixels.empty() || mip.width <= 0 || mip.height <= 0)
        throw std::runtime_error("Invalid perspective mip");
    // Equal depths: the affine path (bit-identical to the original).
    if (vertices[0].inv_z == vertices[1].inv_z && vertices[0].inv_z == vertices[2].inv_z &&
        vertices[0].inv_z == vertices[3].inv_z) {
        std::array<TextureVertex, 4> av;
        for (int i = 0; i < 4; ++i) {
            if (vertices[i].x < -32768 || vertices[i].x > 32767 || vertices[i].y < -32768 ||
                vertices[i].y > 32767)
                throw std::runtime_error("Perspective affine fallback outside int16 range");
            av[size_t(i)] = {int16_t(vertices[i].x), int16_t(vertices[i].y),
                             uint16_t(std::max(0, std::min(65535, int(std::lround(vertices[i].u))))),
                             uint16_t(std::max(0, std::min(65535, int(std::lround(vertices[i].v)))))};
        }
        draw_textured_quad(fb, av, mip.pixels, unsigned(mip.stride_shift), transparent, clip);
        return;
    }
    const int mw = mip.width, mh = mip.height;
    int first = 0;
    int bottom = vertices[0].y;
    for (int i = 1; i < 4; ++i) {
        if (vertices[i].y < vertices[first].y) first = i;
        bottom = std::max(bottom, vertices[i].y);
    }
    if (bottom == vertices[first].y) return;
    // Integer X edges (affine-identical) + double u/z, v/z, 1/z edges.
    struct PEdge {
        int32_t x = 0;
        double uoz = 0, voz = 0, oz = 0;
    };
    auto chain = [&](std::vector<PEdge>& rows, std::vector<char>& set, int direction) {
        int index = first;
        for (int guard = 0; guard < 4; ++guard) {
            const int next = (index + direction + 4) % 4;
            if (vertices[size_t(next)].y < vertices[size_t(index)].y)
                throw std::runtime_error("Textured quad requires normalized monotone edges");
            const auto& a = vertices[size_t(index)];
            const auto& b = vertices[size_t(next)];
            const int32_t dy = p_fixed(b.y - a.y);
            if (dy) {
                const int32_t reciprocal = int32_t(0x100000000LL / dy);
                int32_t x = p_fixed(a.x);
                const int32_t sx = p_product_q16(p_fixed(b.x - a.x), reciprocal);
                double uoz = double(a.u) * a.inv_z, voz = double(a.v) * a.inv_z, oz = double(a.inv_z);
                const double su = (double(b.u) * b.inv_z - uoz) / double(b.y - a.y);
                const double sv = (double(b.v) * b.inv_z - voz) / double(b.y - a.y);
                const double so = (double(b.inv_z) - oz) / double(b.y - a.y);
                const int y0 = std::max(0, a.y), y1 = std::min(fb.height() - 1, b.y);
                const int skip = y0 - a.y;
                x = p_wrap_add(x, p_wrap_mul(sx, skip));
                uoz += su * skip;
                voz += sv * skip;
                oz += so * skip;
                for (int y = y0; y <= y1; ++y) {
                    rows[size_t(y)] = {x, uoz, voz, oz};
                    set[size_t(y)] = 1;
                    x = p_wrap_add(x, sx);
                    uoz += su;
                    voz += sv;
                    oz += so;
                }
            }
            index = next;
            if (vertices[size_t(index)].y == bottom) break;
        }
    };
    std::vector<PEdge> left(size_t(fb.height())), right(size_t(fb.height()));
    std::vector<char> lset(size_t(fb.height()), 0), rset(size_t(fb.height()), 0);
    chain(left, lset, -1);
    chain(right, rset, 1);
    const int top = std::max({0, vertices[size_t(first)].y, clip.top});
    const int last = std::min({bottom, clip.bottom, fb.height() - 1});
    const int bound = std::min(clip.right, fb.width());
    for (int y = top; y <= last; ++y) {
        if (!lset[size_t(y)] || !rset[size_t(y)]) continue;
        auto l = left[size_t(y)], r = right[size_t(y)];
        if (p_floor(r.x) <= 0) continue; // original pre-swap test
        if (p_floor(r.x) < p_floor(l.x)) std::swap(l, r);
        int start = p_floor(l.x), end = p_floor(r.x);
        const int full = end - start;
        if (full <= 0 || start >= bound) continue;
        if (start < 0) start = 0;
        end = std::min(end, bound);
        if (start >= end) continue;
        // Span subdivision every 16 pixels over u/z, v/z, 1/z.
        for (int x = start; x < end;) {
            const int x1 = std::min(x + 16, end);
            const double t0 = full ? (double(x) - p_floor(l.x)) / full : 0;
            const double t1 = full ? (double(x1) - p_floor(l.x)) / full : 0;
            const double uoz0 = l.uoz + (r.uoz - l.uoz) * t0, uoz1 = l.uoz + (r.uoz - l.uoz) * t1;
            const double voz0 = l.voz + (r.voz - l.voz) * t0, voz1 = l.voz + (r.voz - l.voz) * t1;
            const double oz0 = l.oz + (r.oz - l.oz) * t0, oz1 = l.oz + (r.oz - l.oz) * t1;
            for (int px = x; px < x1; ++px) {
                const double f = double(px - x) / double(x1 - x);
                const double oz = oz0 + (oz1 - oz0) * f;
                if (oz <= 0) continue;
                const double uoz = uoz0 + (uoz1 - uoz0) * f;
                const double voz = voz0 + (voz1 - voz0) * f;
                int tu = int(std::floor(uoz / oz)), tv = int(std::floor(voz / oz));
                tu = std::max(0, std::min(mw - 1, tu));
                tv = std::max(0, std::min(mh - 1, tv));
                const uint8_t index = mip.pixels[size_t(tv) * size_t(mw) + size_t(tu)];
                if (index || !transparent) fb.row(y)[px] = index;
            }
            x = x1;
        }
    }
}
} // namespace rr
