#include "pe_strings.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        (void)rr::read_string_resources(bytes);
        (void)rr::read_string_table(bytes);
    });
}
