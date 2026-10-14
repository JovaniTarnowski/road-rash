#include "raster_rgb.hpp"
#include "cel.hpp"
#include "framebuffer.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    rr::Palette pal{};
    pal.rgbx[4 * 1 + 0] = 255;                       // index 1 = red
    pal.rgbx[4 * 2 + 1] = 255;                       // index 2 = green
    rr::CelTexture tex;
    rr::CelMip mip; mip.width = 1; mip.height = 1; mip.stride_shift = 0; mip.pixels = {1};
    tex.mips.push_back(mip);

    rr::clear_rgba_mip_cache();
    check(rr::rgba_mips(tex, pal, rr::palette_generation(pal))[0].premultiplied[0] == 0xffff0000u, "red");
    // A rebuilt texture at the same address (course reload) must not see stale mips.
    tex.mips[0].pixels = {2};
    rr::clear_rgba_mip_cache();
    check(rr::rgba_mips(tex, pal, rr::palette_generation(pal))[0].premultiplied[0] == 0xff00ff00u, "green after clear");
    // A palette change alone selects a new entry.
    pal.rgbx[4 * 2 + 2] = 255;
    check(rr::rgba_mips(tex, pal, rr::palette_generation(pal))[0].premultiplied[0] == 0xff00ffffu, "palette change");
    check(rr::rgba_mip_cache_size() == 2, "two generations cached");
    rr::Palette same = pal;
    check(rr::palette_generation(same) == rr::palette_generation(pal), "equal palettes, equal generation");
    if (failures == 0) std::cout << "rgba_cache OK\n";
    return failures ? 1 : 0;
}
