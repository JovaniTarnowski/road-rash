// Plan 21 Task 5: distance fog and colour grading (failing test).
#include "fog.hpp"
#include <algorithm>
#include <iostream>

namespace {
double luminance(uint32_t c) {
    return 0.2126 * ((c >> 16) & 0xff) + 0.7152 * ((c >> 8) & 0xff) + 0.0722 * (c & 0xff);
}
} // namespace

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    check(rr::fog_factor_q8(0, 33, 100) == 0, "fog is 0 at the camera");
    {
        bool mono = true;
        uint16_t last = 0;
        for (int32_t d = 0; d <= 33 * 256; d += 16) {
            const uint16_t f = rr::fog_factor_q8(d, 33, 100);
            if (f < last) mono = false;
            last = f;
        }
        check(mono, "fog is monotonic in depth");
    }
    check(rr::fog_factor_q8(33 * 256, 33, 100) == 256, "fog is full at the last slice");
    {
        bool zero = true;
        for (int32_t d = 0; d <= 33 * 256; d += 64)
            if (rr::fog_factor_q8(d, 33, 0) != 0) zero = false;
        check(zero, "strength 0 is 0 everywhere");
    }
    {
        const auto lut = rr::grade_lut(rr::Grade::none);
        check(lut.size() == 17 * 17 * 17, "lut has 17^3 entries");
        bool identity = true;
        for (int r = 0; r < 17 && identity; ++r)
            for (int g = 0; g < 17 && identity; ++g)
                for (int b = 0; b < 17 && identity; ++b) {
                    const uint32_t c = lut[size_t((r * 17 + g) * 17 + b)];
                    if (int((c >> 16) & 0xff) != std::min(255, r * 16) ||
                        int((c >> 8) & 0xff) != std::min(255, g * 16) ||
                        int(c & 0xff) != std::min(255, b * 16))
                        identity = false;
                }
        check(identity, "grade none is the identity");
    }
    for (rr::Grade grade : {rr::Grade::warm, rr::Grade::vivid, rr::Grade::dusk}) {
        const auto lut = rr::grade_lut(grade);
        bool valid = lut.size() == 17 * 17 * 17;
        double last_lum = -1;
        bool mono = true;
        for (int i = 0; i < 17 && valid; ++i) {
            const uint32_t c = lut[size_t((i * 17 + i) * 17 + i)];
            const int r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
            if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) valid = false;
            const double lum = luminance(c);
            if (lum < last_lum - 1e-6) mono = false;
            last_lum = lum;
        }
        // Full-lattice validity (not just the grey axis).
        for (uint32_t c : lut) {
            const int r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
            if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
                valid = false;
                break;
            }
        }
        check(valid, "grade entries are valid colours");
        check(mono, "grade is luminance-monotonic on the grey axis");
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
