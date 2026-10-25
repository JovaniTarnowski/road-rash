#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rr {
struct Palette {
    std::array<uint8_t, 1024> rgbx{};
    // PALETTE.RAW: 256 x {r,g,b,flags}. 0x00448f90 forces entry 0 to black.
    static Palette from_raw(const std::vector<uint8_t>& bytes);
};

class Framebuffer {
  public:
    Framebuffer(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    uint8_t* row(int y) { return pixels_.data() + size_t(y) * width_; }
    const uint8_t* row(int y) const { return pixels_.data() + size_t(y) * width_; }
    void clear(uint8_t index);
    void to_rgba(const Palette& palette, uint8_t* out, int pitch) const;

  private:
    int width_, height_;
    std::vector<uint8_t> pixels_;
};
} // namespace rr
