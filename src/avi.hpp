#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace rr {
struct AviStreamInfo {
    uint32_t type = 0, fourcc = 0, rate = 0, scale = 0;
    int width = 0, height = 0, audio_rate = 0, audio_channels = 0, block_align = 0;
    int audio_format = 0, audio_bits = 0, samples_per_block = 0;
};
struct AviChunk {
    int stream = 0;
    size_t offset = 0, size = 0;
    bool keyframe = false;
};
struct AviFile {
    std::vector<AviStreamInfo> streams;
    std::vector<AviChunk> chunks;
    std::vector<uint8_t> palette;
    bool truncated = false;
};
constexpr uint32_t fourcc(char a, char b, char c, char d) {
    return uint32_t(uint8_t(a)) | uint32_t(uint8_t(b)) << 8 | uint32_t(uint8_t(c)) << 16 |
           uint32_t(uint8_t(d)) << 24;
}
// Index-independent physical scan. A truncated tail retains complete packets;
// malformed headers and unsupported stream layouts report an error.
AviFile read_avi(const std::vector<uint8_t>&);
} // namespace rr
