#include "race_music.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        (void)rr::read_mids(bytes);
        (void)rr::read_sf1(bytes, true);
        (void)rr::read_sf1(bytes, false);
    });
}
