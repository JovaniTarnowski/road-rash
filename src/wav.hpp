#pragma once
#include <cstdint>
#include <vector>
namespace rr {
struct PcmSound {
    int rate = 0, channels = 0;
    std::vector<int16_t> samples;
};
PcmSound read_wav(const std::vector<uint8_t>& riff);
} // namespace rr
