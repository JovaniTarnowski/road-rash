#include "framebuffer.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    std::vector<uint8_t> raw(1024);
    for (int i = 0; i < 256; ++i) { raw[4*i] = uint8_t(i); raw[4*i+1] = uint8_t(255-i); raw[4*i+2] = 7; raw[4*i+3] = 0; }
    auto pal = rr::Palette::from_raw(raw);
    check(pal.rgbx[0]==0 && pal.rgbx[1]==0 && pal.rgbx[2]==0, "entry 0 forced black (0x00448f90)");
    check(pal.rgbx[4*5]==5 && pal.rgbx[4*5+1]==250 && pal.rgbx[4*5+2]==7, "entries copied");
    try { rr::Palette::from_raw(std::vector<uint8_t>(1023)); check(false, "reject short palette"); } catch (const std::runtime_error&) {}
    rr::Framebuffer fb(4, 2);
    fb.clear(5);
    fb.row(1)[3] = 9;
    std::vector<uint8_t> rgba(4*2*4);
    fb.to_rgba(pal, rgba.data(), 16);
    check(rgba[0]==5 && rgba[1]==250 && rgba[2]==7 && rgba[3]==255, "index 5 -> rgb, opaque");
    check(rgba[16+12]==9 && rgba[16+13]==246, "row pitch honoured");
    try { rr::Framebuffer bad(0, 10); check(false, "reject empty framebuffer"); } catch (const std::runtime_error&) {}
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
