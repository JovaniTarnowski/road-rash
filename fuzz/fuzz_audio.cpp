#include "ima_adpcm.hpp"
#include "wav.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        (void)rr::read_wav(bytes);
        if (bytes.size() >= 2)
            (void)rr::decode_ima_adpcm(bytes.data(), bytes.size(), 1 + (bytes[0] & 1), 1 + bytes[1]);
    });
}
