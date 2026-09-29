#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace rr {
struct Resource {
    std::string tag;
    uint32_t id, offset, size;
};
// On-disk resource directory. No casting of original 32-bit pointers to host pointers.
std::vector<Resource> read_resources(const std::vector<uint8_t>& bytes);
}
