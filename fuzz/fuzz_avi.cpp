#include "avi.hpp"
#include "cinepak.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        const auto avi = rr::read_avi(bytes);
        for (const auto& c : avi.chunks) {
            if (c.stream < 0 || size_t(c.stream) >= avi.streams.size()) continue;
            if (avi.streams[size_t(c.stream)].type != rr::fourcc('v', 'i', 'd', 's')) continue;
            // Chunks pointing outside the input are skipped: the decoder takes
            // a raw pointer and cannot report the overrun itself.
            if (c.offset > bytes.size() || c.size > bytes.size() - c.offset) continue;
            rr::CinepakDecoder decoder(320, 240);
            decoder.decode(bytes.data() + c.offset, c.size);
        }
    });
}
