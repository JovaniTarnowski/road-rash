#include "explode.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        (void)rr::unpack_resource(bytes);
        if (bytes.size() >= 4) {
            // `expected` from the first 4 bytes, capped at 1 MiB (Review Focus 3).
            uint32_t e = uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 |
                         uint32_t(bytes[3]) << 24;
            if (e > (1u << 20)) e = (1u << 20);
            (void)rr::explode(bytes.data(), bytes.size(), e);
        }
    });
}
