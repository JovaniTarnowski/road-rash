#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace rr {
// WAV IMA, four-bit samples, mono/stereo. Blocks contain one predictor per
// channel followed by four-byte channel groups, low nibble first.
std::vector<int16_t> decode_ima_adpcm(const uint8_t*, size_t, int channels, int block_align);
} // namespace rr
