// Plan 29 Task 1: shared synthetic scene (400 textured + 100 flat nodes).
#pragma once
#include <cstdint>
#include <vector>
#include "cel.hpp"
#include "framebuffer.hpp"
#include "view.hpp"
#include "world_renderer.hpp"

namespace rr::test {
// 50 textures with one dense 2x upscaled level; half the textured nodes
// transparent. Nodes spread over a 640x480 view mapped to 640*S x 480*S.
struct BenchScene {
    std::vector<CelTexture> textures;
    std::vector<DrawNode> nodes;
};
inline BenchScene make_bench_scene() {
    BenchScene sc;
    sc.textures.reserve(50);
    for (int t = 0; t < 50; ++t) {
        CelTexture tex;
        tex.bounds = {0, 0, 63, 63};
        tex.flags = (t % 2) ? 2 : 0;   // half transparent
        tex.name = "bench" + std::to_string(t);
        for (int m = 0; m < 3; ++m) {   // proper halving chain: 64 -> 32 -> 16
            CelMip mip;
            mip.width = 64 >> m; mip.height = 64 >> m;
            mip.stride_shift = unsigned(6 - m);
            mip.pixels.resize(size_t(mip.width) * mip.height);
            for (int i = 0; i < mip.width * mip.height; ++i)
                mip.pixels[size_t(i)] = uint8_t((i * 7 + t * 13 + m) % 250 + 1);
            tex.mips.push_back(std::move(mip));
        }
        CelMip up;   // dense 2x copy of the 64x64 base
        up.width = 128; up.height = 128; up.stride_shift = 7;
        up.pixels.resize(128 * 128);
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x)
                up.pixels[size_t(y) * 128 + x] = tex.mips[0].pixels[size_t(y / 2) * 64 + x / 2];
        tex.upscaled.push_back(std::move(up));
        sc.textures.push_back(std::move(tex));
    }
    sc.nodes.reserve(500);
    for (int n = 0; n < 400; ++n) {
        DrawNode node;
        node.texture = &sc.textures[size_t(n) % sc.textures.size()];
        const int32_t x = (n * 97) % 300, y = (n * 57) % 220;
        const int32_t w = 8 + (n % 5) * 12, h = 8 + (n % 7) * 9;
        node.view_points = {{{x * 65536, y * 65536}, {(x + w) * 65536, y * 65536},
                             {(x + w) * 65536, (y + h) * 65536}, {x * 65536, (y + h) * 65536}}};
        node.view_depth = {512 << 8, 512 << 8, 1024 << 8, 1024 << 8};   // nonzero: fog path active
        sc.nodes.push_back(node);
    }
    for (int n = 0; n < 100; ++n) {
        DrawNode node;
        node.flat_index = uint8_t(n % 250 + 1);
        const int32_t x = (n * 131) % 300, y = (n * 71) % 220;
        node.view_points = {{{x * 65536, y * 65536}, {(x + 40) * 65536, y * 65536},
                             {(x + 40) * 65536, (y + 30) * 65536}, {x * 65536, (y + 30) * 65536}}};
        sc.nodes.push_back(node);
    }
    return sc;
}
}  // namespace rr::test
