#pragma once
#include <cstdint>
#include <vector>

namespace rr {
// Plan 21 Task 4: full-colour framebuffer (ARGB8888, straight alpha).
class FramebufferRgb {
public:
    FramebufferRgb() = default;
    FramebufferRgb(int w, int h) { resize(w, h); }
    void resize(int w, int h);
    int width() const { return w_; }
    int height() const { return h_; }
    uint32_t* row(int y) { return pixels_.data() + size_t(y) * w_; }
    const uint32_t* row(int y) const { return pixels_.data() + size_t(y) * w_; }
    void clear(uint32_t argb);
private:
    int w_ = 0, h_ = 0;
    std::vector<uint32_t> pixels_;
};
}  // namespace rr
