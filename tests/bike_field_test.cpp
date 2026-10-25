#include "bike.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << '\n';
        }
    };
    static_assert(!std::is_reference_v<decltype(std::declval<rr::Bike&>().i32(0))>,
                  "non-const i32 must not hand out an int32_t& into byte storage");
    rr::Bike b;
    b.i32(0x1c) = 0x12345678;
    check(b.m[0x1c] == 0x78 && b.m[0x1f] == 0x12, "little-endian store");
    b.i32(0x1c) += 8;
    check(int32_t(b.i32(0x1c)) == 0x12345680, "+=");
    b.i32(0x20) = std::numeric_limits<int32_t>::max();
    b.i32(0x20) += 1; // x86 wraparound, defined here
    check(int32_t(b.i32(0x20)) == std::numeric_limits<int32_t>::min(), "wraparound +=");
    b.i32(0x24) = b.i32(0x1c); // proxy-to-proxy assignment copies the value
    check(int32_t(b.i32(0x24)) == 0x12345680, "copy between fields");
    const rr::Bike& cb = b;
    check(cb.i32(0x24) == 0x12345680, "const read");
    if (failures == 0) std::cout << "bike_field OK\n";
    return failures ? 1 : 0;
}
