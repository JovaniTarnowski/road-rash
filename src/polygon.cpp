#include "raster.hpp"
#include <algorithm>
#include <cstring>
namespace rr {
namespace {
int32_t bits(uint32_t v) {
    int32_t r;
    std::memcpy(&r, &v, 4);
    return r;
}
int32_t add(int32_t a, int32_t b) {
    return bits(uint32_t(a) + uint32_t(b));
}
int32_t sub(int32_t a, int32_t b) {
    return bits(uint32_t(a) - uint32_t(b));
}
int32_t mul(int32_t a, int32_t b) {
    return bits(uint32_t(a) * uint32_t(b));
}
RasterPoint middle(const RasterPoint& a, const RasterPoint& b) {
    return {add(a.x, b.x) / 2, add(a.y, b.y) / 2};
}
} // namespace
bool normalize_quad(std::array<RasterPoint, 4>& p, uint32_t flags, bool* flip) {
    uint8_t positive = 0, zero = 0;
    for (int i = 0; i < 4; ++i) {
        const auto& previous = p[(i + 3) % 4];
        const auto& at = p[i];
        const auto& next = p[(i + 1) % 4];
        const int32_t cross =
            add(mul(sub(previous.y, at.y), sub(next.x, at.x)), mul(sub(previous.x, at.x), sub(at.y, next.y)));
        if (cross > 0)
            positive = uint8_t(positive | (1u << i));
        else if (!cross)
            zero = uint8_t(zero | (1u << i));
    }
    if (zero == 15)
        positive = 255;
    else
        for (int i = 0; i < 3; ++i) positive = uint8_t(positive | ((positive >> 3 | positive << 1) & zero));
    bool reverse = false;
    switch (positive) {
    case 0:
    case 255:
        reverse = true;
        break;
    case 1:
        p[0] = middle(p[3], p[1]);
        reverse = true;
        break;
    case 2:
        p[1] = middle(p[0], p[2]);
        reverse = true;
        break;
    case 3:
        std::swap(p[2], p[3]);
        break;
    case 4:
        p[2] = middle(p[3], p[1]);
        reverse = true;
        break;
    case 5:
    case 10:
        return false;
    case 6:
        std::swap(p[0], p[3]);
        break;
    case 7:
        p[3] = middle(p[0], p[2]);
        break;
    case 8:
        p[3] = middle(p[0], p[2]);
        reverse = true;
        break;
    case 9:
        std::swap(p[1], p[2]);
        break;
    case 11:
        p[2] = middle(p[3], p[1]);
        break;
    case 12:
        std::swap(p[0], p[1]);
        break;
    case 13:
        p[1] = middle(p[0], p[2]);
        break;
    case 14:
        p[0] = middle(p[3], p[1]);
        break;
    case 15:
        break;
    default:
        return false;
    }
    int start = 0;
    if (flags & 4) {
        start = reverse ? 1 : 0;
        if (reverse && flip) *flip = !*flip;
    } else {
        // Stable original comparisons select the lowest two axis values.
        auto value = [&](int i) { return flags & 8 ? p[i].x : p[i].y; };
        int lowest = value(1) <= value(0) ? 1 : 0, second = 1 - lowest;
        for (int i = 2; i < 4; ++i) {
            if (value(i) < value(lowest)) {
                second = lowest;
                lowest = i;
            } else if (value(i) < value(second))
                second = i;
        }
        if (flags & 8)
            start = p[lowest].y <= p[second].y ? lowest : second;
        else {
            // Lookup at 0x475628/0x475638 encodes the shared cyclic edge.
            // Opposite vertices have no preferred edge (original -1).
            int edge = -1;
            if ((lowest + 1) % 4 == second)
                edge = lowest;
            else if ((second + 1) % 4 == lowest)
                edge = second;
            start = edge < 0 ? -1 : (edge + (reverse ? 1 : 0)) % 4;
        }
    }
    const auto source = p;
    if (reverse) {
        if (start >= 0 && start < 4)
            for (int i = 0; i < 4; ++i) p[i] = source[(start - i + 4) % 4];
    } else if (start >= 1 && start <= 3) {
        for (int i = 0; i < 4; ++i) p[i] = source[(start + i) % 4];
    }
    return true;
}
void draw_flat_quad(Framebuffer& fb, RasterQuad quad, RasterClip clip) {
    // The original bypasses normalization for entirely horizontal spans.
    bool horizontal = true;
    for (const auto& p : quad.points) horizontal &= p.y == quad.points[0].y;
    if (!horizontal) normalize_quad(quad.points);
    fill_quad(fb, quad, clip);
}
} // namespace rr
