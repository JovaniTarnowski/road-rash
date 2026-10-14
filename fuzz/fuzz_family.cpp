#include "family.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        (void)rr::read_family(bytes);
        (void)rr::read_cel_group(bytes, 0);
    });
}
