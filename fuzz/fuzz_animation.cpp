#include "actor_descriptors.hpp"
#include "animation.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return rr_fuzz_contract(data, size, [](const std::vector<uint8_t>& bytes) {
        (void)rr::read_animation(bytes);
        (void)rr::read_actor_graphics(bytes);
        (void)rr::read_actor_sequence(bytes, bytes);
    });
}
