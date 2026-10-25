#include "ima_adpcm.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace rr {
namespace {
constexpr int steps[89] = {
    7,    8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,   28,
    31,   34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,  118,
    130,  143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,  494,
    544,  598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878, 2066,
    2272, 2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
constexpr int change[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
struct Channel {
    int predictor = 0, index = 0;
    int16_t sample(int nibble) {
        const int step = steps[index];
        int delta = step / 8;
        if (nibble & 1) delta += step / 4;
        if (nibble & 2) delta += step / 2;
        if (nibble & 4) delta += step;
        predictor = std::clamp(predictor + ((nibble & 8) ? -delta : delta), -32768, 32767);
        index = std::clamp(index + change[nibble & 7], 0, 88);
        return int16_t(predictor);
    }
};
} // namespace
std::vector<int16_t> decode_ima_adpcm(const uint8_t* bytes, size_t size, int channels, int align) {
    if ((channels != 1 && channels != 2) || align < 4 * channels || align > 65535 ||
        (align - 4 * channels) % (4 * channels) || (!bytes && size))
        throw std::runtime_error("Invalid WAV IMA block layout");
    std::vector<int16_t> out;
    for (size_t at = 0; at < size;) {
        const size_t count = std::min(size - at, size_t(align));
        if (count < size_t(4 * channels) || (count - 4 * channels) % size_t(4 * channels))
            throw std::runtime_error("Truncated WAV IMA channel group");
        std::array<Channel, 2> state;
        for (int c = 0; c < channels; ++c) {
            const uint8_t* header = bytes + at + 4 * c;
            const unsigned value = unsigned(header[0]) | (unsigned(header[1]) << 8);
            state[c].predictor = value < 32768 ? int(value) : int(value) - 65536;
            state[c].index = header[2];
            if (state[c].index > 88) throw std::runtime_error("Invalid WAV IMA step index");
            out.push_back(int16_t(state[c].predictor));
        }
        for (size_t group = 4 * channels; group < count; group += 4 * channels) {
            std::array<std::array<int16_t, 8>, 2> decoded{};
            for (int c = 0; c < channels; ++c)
                for (int b = 0; b < 4; ++b) {
                    const int value = bytes[at + group + 4 * c + b];
                    decoded[c][2 * b] = state[c].sample(value & 15);
                    decoded[c][2 * b + 1] = state[c].sample(value >> 4);
                }
            for (int n = 0; n < 8; ++n)
                for (int c = 0; c < channels; ++c) out.push_back(decoded[c][n]);
        }
        at += count;
    }
    return out;
}
} // namespace rr
