#include "framebuffer.hpp"
#include <algorithm>

namespace rr {
Palette Palette::from_raw(const std::vector<uint8_t>& bytes) {
    if (bytes.size() != 1024) throw std::runtime_error("Palette must be 1024 bytes");
    Palette p;
    std::copy(bytes.begin(), bytes.end(), p.rgbx.begin());
    p.rgbx[0] = p.rgbx[1] = p.rgbx[2] = 0;
    return p;
}

Framebuffer::Framebuffer(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        throw std::runtime_error("Invalid framebuffer size");
    pixels_.assign(size_t(width) * height, 0);
}

void Framebuffer::clear(uint8_t index) {
    std::fill(pixels_.begin(), pixels_.end(), index);
}

void Framebuffer::to_rgba(const Palette& palette, uint8_t* out, int pitch) const {
    for (int y = 0; y < height_; ++y) {
        const uint8_t* src = row(y);
        uint8_t* dst = out + size_t(y) * pitch;
        for (int x = 0; x < width_; ++x) {
            const uint8_t* c = &palette.rgbx[4 * src[x]];
            dst[4 * x] = c[0];
            dst[4 * x + 1] = c[1];
            dst[4 * x + 2] = c[2];
            dst[4 * x + 3] = 255;
        }
    }
}
} // namespace rr
