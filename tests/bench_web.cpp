// Web-planning microbenchmark (plan 23, Task 1, Step 4): renders the
// synthetic world_renderer_test scene 1,000 times and decodes a synthetic
// 4x4 Cinepak frame 2,000 times. Synthetic data only; no disc needed. Runs
// natively and under Node (wasm) so the two times can be compared.
// Pixel sums are printed so the loops cannot be optimised away.
#include "cinepak.hpp"
#include "framebuffer.hpp"
#include "world_renderer.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

uint64_t render_scene(int iterations) {
    rr::Framebuffer fb(320, 240);
    rr::CelTexture texture;
    texture.bounds = {0, 0, 7, 7};
    texture.flat_index = 99;
    rr::CelMip mip;
    mip.width = mip.height = 8;
    mip.stride_shift = 3;
    mip.pixels.assign(64, 34);
    texture.mips.push_back(mip);
    rr::DrawNode node;
    node.texture = &texture;
    node.view_points = {
        {{2 * 65536, 3 * 65536}, {10 * 65536, 3 * 65536}, {10 * 65536, 11 * 65536}, {2 * 65536, 11 * 65536}}};
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        fb.clear(125);
        rr::render_nodes(fb, {node, {}}, rr::view_mode(false), 16);
    }
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
            .count();
    uint64_t sum = 0;
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x) sum += fb.row(y)[x];
    std::printf("render: %d iterations in %lld ms (checksum %llu)\n", iterations, (long long)ms,
                (unsigned long long)sum);
    return sum;
}

void be16(std::vector<uint8_t>& b, unsigned v) {
    b.push_back(uint8_t(v >> 8));
    b.push_back(uint8_t(v));
}

void be24(std::vector<uint8_t>& b, unsigned v) {
    b.push_back(uint8_t(v >> 16));
    be16(b, v);
}

std::vector<uint8_t> chunk(unsigned id, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> b{uint8_t(id)};
    be24(b, unsigned(data.size() + 4));
    b.insert(b.end(), data.begin(), data.end());
    return b;
}

void decode_stream(int iterations) {
    auto book = chunk(0x26, {10, 20, 30, 40});
    auto vectors = chunk(0x32, {0});
    book.insert(book.end(), vectors.begin(), vectors.end());
    std::vector<uint8_t> encoded{0};
    be24(encoded, unsigned(book.size() + 22));
    be16(encoded, 4);
    be16(encoded, 4);
    be16(encoded, 1);
    encoded.push_back(0x10);
    be24(encoded, unsigned(book.size() + 12));
    be16(encoded, 0);
    be16(encoded, 0);
    be16(encoded, 4);
    be16(encoded, 4);
    encoded.insert(encoded.end(), book.begin(), book.end());
    const auto start = std::chrono::steady_clock::now();
    uint64_t sum = 0;
    for (int i = 0; i < iterations; ++i) {
        rr::CinepakDecoder decoder(4, 4);
        decoder.decode(encoded.data(), encoded.size());
        for (uint8_t v : decoder.rgb24()) sum += v;
    }
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
            .count();
    std::printf("cinepak: %d iterations in %lld ms (checksum %llu)\n", iterations, (long long)ms,
                (unsigned long long)sum);
}

} // namespace

int main() {
    render_scene(20000);
    decode_stream(20000);
    return 0;
}
