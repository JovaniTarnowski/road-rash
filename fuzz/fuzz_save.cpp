#include "career.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        // Synthetic tables mirror tests/career_test.cpp (no game data).
        rr::CareerTables t;
        for (size_t i = 0; i < t.start_cash.size(); ++i) t.start_cash[i] = uint16_t(100 * i);
        for (size_t i = 0; i < t.level_bike.size(); ++i) t.level_bike[i] = uint8_t(i % 15);
        for (size_t i = 0; i < t.bike_price.size(); ++i) t.bike_price[i] = uint32_t(1000 * (i + 1));
        for (size_t i = 0; i < t.prize.size(); ++i) t.prize[i] = uint32_t(1000 - 60 * i);
        for (size_t p = 0; p < 10; ++p)
            for (size_t r = 0; r < 10; ++r) t.default_relations[p][r] = {int32_t(p), int32_t(r), int32_t(p * 10 + r)};
        (void)rr::read_save(bytes, t);
    });
}
