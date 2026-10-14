#include "bike.hpp"
#include "crash.hpp"
#include "frontend.hpp"
#include "fuzz_common.hpp"
#include "police.hpp"
#include "race.hpp"
#include "road.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // These readers take fixed addresses in an EXE image; on short or non-PE
    // input they must throw std::runtime_error, never crash.
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        (void)rr::read_physics_tables(bytes);
        (void)rr::read_depth_table(bytes);
        (void)rr::read_frontend_data(bytes);
        (void)rr::read_personalities(bytes);
        (void)rr::read_crash_config(bytes);
        (void)rr::read_police_config(bytes);
    });
}
