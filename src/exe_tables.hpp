#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace rr {
// Read initialized data at a virtual address from the user's i386 PE executable.
std::vector<uint8_t> read_exe_data(const std::vector<uint8_t>&, uint32_t address, size_t count);
} // namespace rr
