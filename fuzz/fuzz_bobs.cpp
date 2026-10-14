#include "bob.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // 0x146 is the HIBOB frame count (tests/bob_test.cpp).
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) { (void)rr::read_bobs(bytes, 0x146); });
}
