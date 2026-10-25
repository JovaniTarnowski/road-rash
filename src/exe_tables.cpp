#include "exe_tables.hpp"
#include <stdexcept>
namespace rr {
std::vector<uint8_t> read_exe_data(const std::vector<uint8_t>& b, uint32_t address, size_t count) {
    auto range = [&](size_t p, size_t n) {
        if (p > b.size() || n > b.size() - p) throw std::runtime_error("Truncated PE image");
    };
    auto u16 = [&](size_t p) {
        range(p, 2);
        return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8;
    };
    auto u32 = [&](size_t p) {
        range(p, 4);
        return u16(p) | u16(p + 2) << 16;
    };
    if (u16(0) != 0x5a4d) throw std::runtime_error("Missing MZ header");
    const size_t pe = u32(60);
    range(pe, 24);
    if (u32(pe) != 0x4550 || u16(pe + 4) != 0x14c) throw std::runtime_error("Expected i386 PE image");
    const auto optional_size = u16(pe + 20);
    if (optional_size < 32) throw std::runtime_error("Truncated PE optional header");
    range(pe + 24, optional_size);
    if (u16(pe + 24) != 0x10b) throw std::runtime_error("Expected PE32 optional header");
    const uint32_t base = u32(pe + 24 + 28);
    if (address < base) throw std::runtime_error("Address below image base");
    const uint32_t rva = address - base;
    const size_t table = pe + 24 + optional_size;
    const auto sections = u16(pe + 6);
    range(table, size_t(sections) * 40);
    for (uint32_t i = 0; i < sections; ++i) {
        const size_t p = table + 40 * i;
        const uint32_t va = u32(p + 12), size = u32(p + 16), raw = u32(p + 20);
        if (rva < va || rva - va >= size) continue;
        const size_t displacement = rva - va;
        if (count > size - displacement) throw std::runtime_error("Executable data crosses section boundary");
        const uint64_t start = uint64_t(raw) + displacement;
        if (start > b.size()) throw std::runtime_error("Executable section outside file");
        range(size_t(start), count);
        return {b.begin() + size_t(start), b.begin() + size_t(start) + count};
    }
    throw std::runtime_error("Executable address has no file data");
}
} // namespace rr
