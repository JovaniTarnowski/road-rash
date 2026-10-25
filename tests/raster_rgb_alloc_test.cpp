// Plan 29 Task 2: RGB path must not allocate per frame (failing first).
#define RR_ALLOC_COUNTER_DEFINE
#include "alloc_counter.hpp"
#include "bench_scene.hpp"
#include "framebuffer_rgb.hpp"
#include "raster_rgb.hpp"
#include "render_threads.hpp"
#include "settings.hpp"
#include "view.hpp"
#include <iostream>

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
} // namespace

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << '\n';
        }
    };
    rr::test::BenchScene scene = rr::test::make_bench_scene();
    rr::ViewMode mode = rr::view_mode(true);
    rr::Palette pal;
    for (int i = 0; i < 256; ++i) {
        pal.rgbx[4 * i] = uint8_t(i);
        pal.rgbx[4 * i + 1] = uint8_t(255 - i);
        pal.rgbx[4 * i + 2] = uint8_t(i * 2);
        pal.rgbx[4 * i + 3] = 0;
    }
    rr::Enhancements enh;
    enh.filter = rr::TextureFilter::bilinear;
    rr::RenderThreads pool(1);
    rr::FramebufferRgb fb(640, 480);
    fb.clear(0);
    rr::render_nodes_rgb_fog(fb, scene.nodes, mode, 16, pal, enh, pool, 0x808080, 33);
    const uint64_t first = checksum_rgb(fb);
    {
        rr::test::CountAllocs c;
        fb.clear(0);
        rr::render_nodes_rgb_fog(fb, scene.nodes, mode, 16, pal, enh, pool, 0x808080, 33);
        const size_t n = c.count();
        std::cout << "second-frame allocations: " << n << '\n';
        check(n == 0, "zero allocations on second frame");
    }
    check(checksum_rgb(fb) == first, "checksum stable across frames");
    // Pre-change checksum (merge-base 524af8b run): pins no pixel change.
    check(checksum_rgb(fb) == 0x861b81d25eb5f7dbULL, "matches pre-change checksum");
    if (failures == 0) std::cout << "raster_rgb_alloc OK\n";
    return failures ? 1 : 0;
}
