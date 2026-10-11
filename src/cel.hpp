#pragma once
#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <vector>
namespace rr {
struct Plut {std::vector<uint16_t> entries;};
struct CelMip {
    int width=0,height=0;unsigned stride_shift=0;
    std::vector<uint8_t> pixels;
    // The original span functions use a 16-bit unsigned byte address. Keep
    // the available resource tail for fractional edge underflow, which can
    // address beyond the logical mip. Bytes beyond the imported resource are
    // zero guards: process-specific heap reads cannot be reproduced safely.
    std::vector<uint8_t> sampling_storage;
};
struct CelTexture {std::array<int32_t,4> bounds{};uint32_t flags=0;uint8_t flat_index=0;std::string name;std::vector<CelMip> mips;};
struct Cel {
    uint32_t flags=0;
    int width=0,height=0;
    Plut palette;
    std::optional<CelTexture> texture;
    // Small legacy PDAT words are retained; their draw behavior is not decoded.
    std::optional<uint32_t> legacy_pixel_word;
};
Plut read_plut(const std::vector<uint8_t>& chunk);
Cel read_cel(const std::vector<uint8_t>& payload);
// Indexed pixel resource (magic 0xbc614e; mip offsets +28-relative, 0x0040b7b0) as found in PDAT
// bodies and cel groups (CLGP, 0x0044a660/0x0044a760).
CelTexture read_indexed_texture(const uint8_t* data,size_t size);
}
