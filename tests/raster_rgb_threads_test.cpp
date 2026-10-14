// Plan 29 Task 3: RGB bands must be byte-identical at every thread count (failing first).
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
        for (int x = 0; x < fb.width(); ++x) { h ^= fb.row(y)[x]; h *= 1099511628211ULL; }
    return h;
}
bool equal_buf(const rr::FramebufferRgb& a, const rr::FramebufferRgb& b) {
    if (a.width() != b.width() || a.height() != b.height()) return false;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (a.row(y)[x] != b.row(y)[x]) return false;
    return true;
}
}

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    rr::test::BenchScene scene = rr::test::make_bench_scene();
    rr::Palette pal;
    for (int i = 0; i < 256; ++i) { pal.rgbx[4 * i] = uint8_t(i); pal.rgbx[4 * i + 1] = uint8_t(255 - i); pal.rgbx[4 * i + 2] = uint8_t(i * 2); pal.rgbx[4 * i + 3] = 0; }
    rr::Enhancements bilinear, fog_enh;
    bilinear.filter = rr::TextureFilter::bilinear;
    fog_enh.filter = rr::TextureFilter::bilinear; fog_enh.fog = 60;
    for (auto [w, h] : {std::pair{640, 480}, {1366, 768}, {641, 479}, {64, 5}}) {
        rr::ViewMode mode = rr::view_mode(true);
        mode.surface_width = w; mode.surface_height = h;
        for (bool fog : {false, true}) {
            rr::FramebufferRgb ref(w, h);
            ref.clear(0);
            {
                rr::RenderThreads pool(1);
                if (fog) rr::render_nodes_rgb_fog(ref, scene.nodes, mode, 16, pal, fog_enh, pool, 0x808080, 33);
                else rr::render_nodes_rgb(ref, scene.nodes, mode, 16, pal, bilinear, pool);
            }
            for (int t : {1, 2, 4, 7}) {
                rr::FramebufferRgb fb(w, h);
                fb.clear(0);
                rr::RenderThreads pool(t);
                if (fog) rr::render_nodes_rgb_fog(fb, scene.nodes, mode, 16, pal, fog_enh, pool, 0x808080, 33);
                else rr::render_nodes_rgb(fb, scene.nodes, mode, 16, pal, bilinear, pool);
                check(equal_buf(fb, ref), ("threads identical " + std::to_string(w) + "x" + std::to_string(h) +
                                           (fog ? " fog" : "") + " t=" + std::to_string(t)).c_str());
            }
        }
    }
    if (failures == 0) std::cout << "raster_rgb_threads OK\n";
    return failures ? 1 : 0;
}
