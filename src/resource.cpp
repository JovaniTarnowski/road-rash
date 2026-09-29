#include "resource.hpp"
#include <algorithm>

namespace rr {
std::vector<Resource> read_resources(const std::vector<uint8_t>& b) {
    auto range = [&](size_t p, size_t n) {
        if (p > b.size() || n > b.size()-p) throw std::runtime_error("Resource range outside file");
    };
    auto word = [&](size_t p) -> uint32_t {
        range(p, 4);
        return uint32_t(b[p]) | uint32_t(b[p+1])<<8 | uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24;
    };
    auto tag = [&](size_t p) {
        range(p, 4);
        return std::string(b.begin()+p, b.begin()+p+4);
    };
    range(0, 24);
    if (tag(0) != "CRSR" || tag(12) != "LBTR") throw std::runtime_error("Not a Road Rash resource container");
    size_t table = word(16);
    range(table, 16);
    if (tag(table) != "LBTR") throw std::runtime_error("Missing resource table");
    const size_t count = word(table+8);
    if (count > (b.size()-table-16)/32) throw std::runtime_error("Truncated resource directory");
    std::vector<Resource> entries;
    for (size_t i = 0; i < count; ++i) {
        size_t p = table+16+i*32;
        auto name = tag(p);
        std::reverse(name.begin(), name.end());
        Resource item{name, word(p+4), word(p+8), word(p+12)};
        range(item.offset, item.size);
        entries.push_back(item);
    }
    return entries;
}
}
