// Plan 34 Task 4 Step 3: the network decoder never crashes, and any message
// it accepts re-encodes to exactly the same bytes (canonical encoding).
#include "fuzz_common.hpp"
#include "net/protocol.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        const auto m = rr::net::decode(bytes);
        (void)rr::net::peek_hello_protocol(bytes);
        if (m && rr::net::encode(*m) != bytes) std::abort();
    });
}
