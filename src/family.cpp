#include "family.hpp"
#include "explode.hpp"
#include <functional>
#include <set>
#include <stdexcept>

namespace rr {
namespace {
struct Reader {
    const std::vector<uint8_t>& bytes;
    void range(size_t p, size_t n) const {
        if (p > bytes.size() || n > bytes.size() - p) throw std::runtime_error("Truncated family resource");
    }
    uint32_t word(size_t p) const {
        range(p, 4);
        return uint32_t(bytes[p]) | uint32_t(bytes[p+1]) << 8 |
               uint32_t(bytes[p+2]) << 16 | uint32_t(bytes[p+3]) << 24;
    }
    uint32_t relative(size_t base, uint32_t displacement, size_t minimum = 4) const {
        const uint64_t result = uint64_t(base) + displacement;
        if (result > UINT32_MAX) throw std::runtime_error("Family offset overflow");
        range(size_t(result), minimum);
        return uint32_t(result);
    }
};
uint16_t power(unsigned exponent) {
    // 0xff is the unused axis in height-selected terrain groups. The x86
    // shift by 31 and wrapped table addressing read dimension-table entry 0.
    if (exponent == 255) return 0;
    if (exponent > 9) throw std::runtime_error("Cel group dimension exceeds 512");
    return uint16_t(1u << exponent);
}
}

CelGroup read_cel_group(const std::vector<uint8_t>& bytes, uint32_t offset) {
    const Reader r{bytes};
    r.range(offset, 28);
    if (r.word(offset) != 0x434c4750u) throw std::runtime_error("Not a CLGP resource");
    CelGroup group;
    group.flags = bytes[offset+20];
    group.nominal_height = bytes[offset+22];
    group.last_column = bytes[offset+24];
    group.last_row = bytes[offset+25];
    const unsigned mode = group.flags & 3;
    if (mode > 1) throw std::runtime_error("Unsupported cel group mode");
    // Validate the CCB body and hotspot pointers used by the original loader.
    r.relative(size_t(offset)+12, r.word(offset+8), 68);
    r.relative(size_t(offset)+12, r.word(offset+12));
    r.relative(size_t(offset)+8, r.word(offset+16));
    const size_t table = size_t(offset) + size_t(bytes[offset+21]) * 4;
    if (table < size_t(offset)+28) throw std::runtime_error("Cel group table overlaps header");
    auto append = [&](uint32_t source, uint16_t width, uint16_t height) {
        const uint32_t resource = r.relative(size_t(offset)+8, source, 56);
        group.cells.push_back({read_indexed_texture(bytes.data()+resource, bytes.size()-resource), width, height, resource});
    };
    if (mode == 0) {
        r.range(table, (size_t(group.last_row)+1)*4);
        for (unsigned y = 0; y <= group.last_row; ++y) {
            const uint32_t row = r.relative(offset, r.word(table+4*y), (size_t(group.last_column)+1)*4);
            for (unsigned x = 0; x <= group.last_column; ++x)
                append(r.word(size_t(row)+4*x), power(unsigned(bytes[offset+26])+x), power(unsigned(bytes[offset+27])+y));
        }
    } else {
        const size_t count = size_t(group.last_column)+1;
        r.range(table, count*4);
        const size_t thresholds = size_t(offset) + size_t(bytes[offset+22])*4;
        if (thresholds < size_t(offset)+28) throw std::runtime_error("Cel group thresholds overlap header");
        r.range(thresholds, count*2);
        for (size_t i = 0; i < count; ++i) {
            const std::array<uint8_t,2> size{bytes[thresholds+2*i],bytes[thresholds+2*i+1]};
            if (!(group.flags & 0x40) && (!size[0] || !size[1])) throw std::runtime_error("Zero cel group dimensions");
            group.thresholds.push_back(size);
            append(r.word(table+4*i), group.flags & 0x40 ? power(size[0]) : uint16_t(size[0]),
                   group.flags & 0x40 ? power(size[1]) : uint16_t(size[1]));
        }
    }
    return group;
}

const GroupCel& CelGroup::select(int32_t width, int32_t height, const std::vector<uint8_t>& logarithms) const {
    if (cells.empty()) throw std::runtime_error("Empty cel group");
    if ((flags & 3) == 0) return cells.at(size_t(last_row)*(size_t(last_column)+1)+last_column);
    if ((flags & 3) != 1 || thresholds.size() != cells.size()) throw std::runtime_error("Invalid cel group selection table");
    int32_t size = (flags & 0x20 ? height : width) >> 16;
    if (flags & 0x40) {
        if (size < 0) throw std::runtime_error("Negative cel group projected size");
        if (size < 256) {
            if (logarithms.size() != 256) throw std::runtime_error("Cel group logarithm table must have 256 entries");
            size = logarithms.at(size_t(size));
        } else size = 8;
    }
    size_t index = size_t(last_column);
    const size_t axis = flags & 0x20 ? 1 : 0;
    while (index && int32_t(thresholds.at(index)[axis]) > size) --index;
    return cells.at(index);
}

Family read_family(const std::vector<uint8_t>& payload) {
    Family family;
    family.bytes = unpack_resource(payload);
    const Reader r{family.bytes};
    std::set<uint32_t> visiting;
    std::function<void(uint32_t, unsigned)> visit = [&](uint32_t offset, unsigned depth) {
        if (depth > 32 || family.nodes.size() > 65536) throw std::runtime_error("Family hierarchy too large");
        if (visiting.count(offset)) throw std::runtime_error("Cycle in family hierarchy");
        if (family.nodes.count(offset)) return;
        const uint32_t tag = r.word(offset);
        visiting.insert(offset);
        FamilyNode node;
        if ((tag & 0xf000u) == 0 && tag < 0x1000u) {
            r.range(size_t(offset)+4, size_t(tag)*4);
            for (uint32_t i = 0; i < tag; ++i) {
                const uint32_t child = r.relative(offset, r.word(size_t(offset)+4+size_t(i)*4));
                node.children.push_back(child);
                visit(child, depth+1);
            }
        } else if (tag == 0x4f465354u) { // OFST, 0x0044aa80
            const uint32_t target = r.relative(offset, r.word(size_t(offset)+4));
            visit(target, depth+1);
            node = family.nodes.at(target);
            if (family.groups.count(target)) family.groups.emplace(offset, family.groups.at(target));
        } else {
            node.tag = tag;
            if (tag == 0x434c4750u) family.groups.emplace(offset, read_cel_group(family.bytes, offset));
            else if (tag != 0x5252414eu && tag != 0x414e494du && tag != 0x43414e53u &&
                     tag != 0x464f524du && tag != 0x4f465353u && tag != 0x43434220u && tag != 0x504c5554u)
                throw std::runtime_error("Unknown family leaf tag");
        }
        family.nodes.emplace(offset, std::move(node));
        visiting.erase(offset);
    };
    visit(0, 0);
    return family;
}

std::optional<uint32_t> Family::resolve(uint32_t category, uint32_t selector, std::optional<uint32_t> frame) const {
    auto child = [&](uint32_t offset, uint32_t index) -> std::optional<uint32_t> {
        const auto it = nodes.find(offset);
        if (it == nodes.end() || it->second.tag || index >= it->second.children.size()) return std::nullopt;
        return it->second.children[index];
    };
    auto result = child(0, category);
    if (result) result = child(*result, selector);
    if (result && frame) result = child(*result, *frame);
    return result;
}
}
