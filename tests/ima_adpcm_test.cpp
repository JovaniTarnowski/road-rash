#include "ima_adpcm.hpp"
#include <iostream>
#include <stdexcept>
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            ++failures;
            std::cerr << what << '\n';
        }
    };
    // Predictor 100, index 0: nibble 1 adds 1, nibble 2 adds 3.
    const uint8_t mono[] = {100, 0, 0, 0, 0x21, 0, 0, 0};
    check(rr::decode_ima_adpcm(mono, sizeof mono, 1, 8) ==
              std::vector<int16_t>{100, 101, 104, 104, 104, 104, 104, 104, 104},
          "mono low-nibble order");
    const uint8_t stereo[] = {100, 0, 0, 0, 0x9c, 0xff, 0, 0, 0x21, 0, 0, 0, 0xa9, 0, 0, 0};
    check(rr::decode_ima_adpcm(stereo, sizeof stereo, 2, 16) ==
              std::vector<int16_t>{100, -100, 101, -101, 104, -104, 104, -104, 104, -104, 104, -104, 104,
                                   -104, 104, -104, 104, -104},
          "stereo channel grouping");
    auto twice = std::vector<uint8_t>(mono, mono + sizeof mono);
    twice.insert(twice.end(), mono, mono + sizeof mono);
    const auto result = rr::decode_ima_adpcm(twice.data(), twice.size(), 1, 8);
    check(result.size() == 18 && result[9] == 100, "predictor resets per block");
    const uint8_t clipping[] = {0xff, 0x7f, 88, 0, 0x77, 0x77, 0xff, 0xff};
    const auto clipped = rr::decode_ima_adpcm(clipping, sizeof clipping, 1, 8);
    check(clipped[1] == 32767 && clipped.back() == -32768, "predictor saturation");
    for (size_t n = 1; n < sizeof stereo; ++n) {
        if (n == 8) continue; // Header-only final block is a valid single frame.
        try {
            rr::decode_ima_adpcm(stereo, n, 2, 16);
            check(false, "incomplete stereo group accepted");
        } catch (const std::runtime_error&) {
        }
    }
    auto bad = std::vector<uint8_t>(mono, mono + sizeof mono);
    bad[2] = 89;
    try {
        rr::decode_ima_adpcm(bad.data(), bad.size(), 1, 8);
        check(false, "invalid step index accepted");
    } catch (const std::runtime_error&) {
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
