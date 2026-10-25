#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace rr {
class CinepakDecoder {
  public:
    CinepakDecoder(int width, int height);
    void decode(const uint8_t*, size_t);
    const std::vector<uint8_t>& rgb24() const { return pixels_; }

  private:
    using Vector = std::array<uint8_t, 12>;
    struct Books {
        std::array<Vector, 256> v1{}, v4{};
    };
    int width_, height_;
    std::vector<Books> books_;
    std::vector<uint8_t> pixels_;
};
} // namespace rr
