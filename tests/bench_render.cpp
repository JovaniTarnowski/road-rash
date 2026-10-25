// Plan 29 Task 1: synthetic render benchmark (indexed + RGB paths).
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include "alloc_counter.hpp"
#include "bench_scene.hpp"
#include "framebuffer.hpp"
#include "framebuffer_rgb.hpp"
#include "raster_rgb.hpp"
#include "render_threads.hpp"
#include "settings.hpp"
#include "view.hpp"
#include "world_renderer.hpp"

namespace {
uint64_t checksum_rgb(const rr::FramebufferRgb& fb) {
    uint64_t h = 1469598103934665603ULL;
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x) {
            h ^= fb.row(y)[x];
            h *= 1099511628211ULL;
        }
    return h;
}
uint64_t checksum_idx(const rr::Framebuffer& fb) {
    uint64_t h = 1469598103934665603ULL;
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x) {
            h ^= fb.row(y)[x];
            h *= 1099511628211ULL;
        }
    return h;
}
} // namespace

int main(int argc, char** argv) {
    int threads = 1, scale = 1, frames = 20;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--threads") && i + 1 < argc) threads = std::atoi(argv[++i]);
        if (!std::strcmp(argv[i], "--scale") && i + 1 < argc) scale = std::atoi(argv[++i]);
        if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frames = std::atoi(argv[++i]);
    }
    rr::test::BenchScene scene = rr::test::make_bench_scene();
    rr::ViewMode mode = rr::view_mode(true);
    mode.surface_width = 640 * scale;
    mode.surface_height = 480 * scale;
    rr::Palette pal;
    for (int i = 0; i < 256; ++i) {
        pal.rgbx[4 * i] = uint8_t(i);
        pal.rgbx[4 * i + 1] = uint8_t(255 - i);
        pal.rgbx[4 * i + 2] = uint8_t(i * 2);
        pal.rgbx[4 * i + 3] = 0;
    }
    rr::Enhancements enh;
    rr::RenderThreads pool(threads);
    auto bench = [&](const char* name, auto fn) {
        fn(); // warm-up
        const auto start = std::chrono::steady_clock::now();
        uint64_t sum = 0;
        for (int f = 0; f < frames; ++f) sum += fn();
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() /
            frames;
        std::printf("%s_ms=%.3f checksum=%llu\n", name, ms, (unsigned long long)sum);
    };
    bench("indexed", [&]() -> uint64_t {
        rr::Framebuffer fb(640 * scale, 480 * scale);
        fb.clear(0);
        rr::render_nodes_threaded(fb, scene.nodes, mode, 16, pool);
        return checksum_idx(fb);
    });
    bench("rgb", [&]() -> uint64_t {
        rr::FramebufferRgb fb(640 * scale, 480 * scale);
        fb.clear(0);
        rr::render_nodes_rgb(fb, scene.nodes, mode, 16, pal, enh, pool);
        return checksum_rgb(fb);
    });
    rr::Enhancements fog_enh = enh;
    fog_enh.fog = 60;
    bench("rgb_fog", [&]() -> uint64_t {
        rr::FramebufferRgb fb(640 * scale, 480 * scale);
        fb.clear(0);
        rr::render_nodes_rgb_fog(fb, scene.nodes, mode, 16, pal, fog_enh, pool, 0x808080, 33);
        return checksum_rgb(fb);
    });
    bench("rgb_upscaled", [&]() -> uint64_t {
        rr::FramebufferRgb fb(640 * scale, 480 * scale);
        fb.clear(0);
        rr::render_nodes_rgb(fb, scene.nodes, mode, 16, pal, enh, pool);
        return checksum_rgb(fb);
    });
    return 0;
}
