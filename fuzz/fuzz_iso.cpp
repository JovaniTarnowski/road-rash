#include <algorithm>
#include <cstring>
#include <string>

#include "fuzz_common.hpp"
#include "iso9660.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        rr::Iso9660 iso([&](uint64_t at, uint8_t* out, size_t n) {
            if (at >= bytes.size()) return size_t(0);
            n = std::min(n, bytes.size() - size_t(at));
            if (n) std::memcpy(out, bytes.data() + at, n);
            return n;
        });
        // list("") is the volume root: locate("/") rejects the leading slash.
        size_t done = 0;
        for (const auto& name : iso.list("")) {
            if (++done > 64) break;
            try {
                (void)iso.read(name);
            } catch (const std::runtime_error&) {
            }
            try {
                for (const auto& sub : iso.list(name)) {
                    (void)sub;
                    if (++done > 64) break;
                }
            } catch (const std::runtime_error&) {
            }
        }
    });
}
