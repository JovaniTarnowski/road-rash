#include "terrain.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        // Sample count from the first 2 bytes.
        uint32_t n = 0;
        if (bytes.size() >= 2) n = uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8;
        (void)rr::read_terrain(bytes, n);
    });
}
