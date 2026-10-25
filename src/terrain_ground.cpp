#include "terrain_ground.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
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
int32_t blend(int32_t a, int32_t b, uint8_t fraction) {
    return add(a, mul(sub(b, a), fraction) / 256);
}
} // namespace
int32_t terrain_height(const TerrainSlice& t, const PathSample& path, const TerrainSlice& next,
                       const PathSample& np, int32_t lateral, uint8_t fraction, int32_t centre,
                       int32_t offset) {
    const int32_t x = lateral >> 8, left = -path.left, right = path.right;
    if (left <= x && x <= right) return 0;
    if (t.type == 1) {
        // 0x00439b10 case 1 on 0x00454240's wall values (wall_profile).
        const auto w = wall_profile(t, path, false);
        const int32_t base_right = w.base[1], top_right = w.top[1];
        if (base_right < x) {
            const int32_t h = w.height[1];
            if (!h) return 0;
            const int32_t span = sub(top_right, base_right);
            if (mul(h < 0 ? sub(0, h) : h, 2) < span && sub(x, base_right) < add(top_right, offset))
                return mul(h, sub(x, base_right)) / span;
            return h;
        }
        const int32_t base_left = w.base[0], top_left = w.top[0];
        if (base_left <= x) return 0;
        const int32_t h = w.height[0];
        if (!h) return 0;
        const int32_t span = sub(base_left, top_left);
        if (mul(h < 0 ? sub(0, h) : h, 2) < span && add(top_left, offset) < sub(base_left, x))
            return mul(sub(add(left, offset), sub(base_left, x)), h) / span;
        return h;
    }
    if (t.type == 3) {
        // Case 3: tunnel sidewalks, empty pair-B heights become 16 (0x00454240).
        const bool empty = path.height_left == 0;
        return x <= left ? (empty ? 16 : path.height_left) : (empty ? 16 : path.height_right);
    }
    const bool side = x > right;
    const int32_t shoulder = side ? right + path.shoulder_right : left - path.shoulder_left;
    const int32_t height = side ? path.height_right : path.height_left;
    if (t.type == 0) {
        if (path.marking_flags & 32) return 0;
        if ((side ? x >= shoulder : x <= shoulder) && !(path.marking_flags & 24) && t.selectors[side] != 255)
            return 999;
        return height;
    }
    if (t.type != 4) return 0;
    const int32_t next_edge = add(centre, side ? np.right : -np.left);
    const int32_t edge = blend(side ? right : left, next_edge, fraction);
    if (side ? x <= edge : x >= edge) return 0;
    int32_t inner = blend(
        shoulder, add(centre, side ? np.right + np.shoulder_right : -np.left - np.shoulder_left), fraction);
    if (side ? x < inner : x > inner) return height;
    int band = 0;
    int32_t outer = blend(t.points[side][0], add(centre, next.points[side][0]), fraction);
    while (band < 2 && (side ? outer < x : x < outer)) {
        ++band;
        inner = outer;
        outer = blend(t.points[band * 2 + side][0], add(centre, next.points[band * 2 + side][0]), fraction);
    }
    if (side ? outer < x : x < outer) return blend(t.points[4 + side][1], next.points[4 + side][1], fraction);
    const int32_t inner_height =
        band ? blend(t.points[(band - 1) * 2 + side][1], next.points[(band - 1) * 2 + side][1], fraction)
             : blend(height, side ? np.height_right : np.height_left, fraction);
    const int32_t outer_height =
        blend(t.points[band * 2 + side][1], next.points[band * 2 + side][1], fraction);
    const int32_t span = sub(outer, inner);
    if (!span) throw std::runtime_error("Degenerate terrain ground segment");
    return add(mul(sub(outer_height, inner_height), sub(x, inner)) / span, inner_height);
}
WallProfile wall_profile(const TerrainSlice& t, const PathSample& path, bool tunnel) {
    WallProfile w;
    const int32_t left = -path.left, right = path.right;
    std::array<int32_t, 2> edge = {left - path.shoulder_left, right + path.shoulder_right},
                           height = {t.walls[0][2], t.walls[1][2]};
    w.outer_left = edge[0];
    w.outer_right = edge[1];
    if (tunnel) {
        if (height[1] < 1) {
            w.mountain = 0;
            edge[1] = sub(mul(edge[1], 2), right);
        } else {
            if (height[0] > 0) height[0] = 0;
            w.mountain = 1;
            edge[0] = sub(mul(edge[0], 2), left);
        }
        height[w.mountain] = std::max(height[w.mountain], 800);
        w.outer_left = left - path.shoulder_left / 2;
        w.outer_right = right + path.shoulder_right / 2;
    }
    w.base = {sub(edge[0], t.walls[0][0]), add(edge[1], t.walls[1][0])};
    w.top = {sub(edge[0], t.walls[0][1]), add(edge[1], t.walls[1][1])};
    for (size_t side = 0; side < 2; ++side)
        w.height[side] = height[side] < 1 ? sub(0, path.elevation) : height[side];
    return w;
}
} // namespace rr
