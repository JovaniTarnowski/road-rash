#pragma once
#include "cel.hpp"
#include <map>
#include <optional>

namespace rr {
struct GroupCel {
    CelTexture texture;
    uint16_t width = 0, height = 0;
    uint32_t resource_offset = 0;
};
struct CelGroup {
    uint8_t flags = 0, last_column = 0, last_row = 0;
    uint8_t nominal_height = 0; // +0x16, building height in units of 16 for mode 0
    uint8_t row_shift = 0;      // +0x1b; 0x0041c200 rail height ((1<<+0x1b)<<+0x19)*4
    std::vector<GroupCel> cells;
    std::vector<std::array<uint8_t, 2>> thresholds;
    // 0x0044aa10 / 0x0044a8c0. Sizes are Q16; logarithmic lookup is
    // read from the imported EXE at 0x00476af8 rather than embedded.
    const GroupCel& select(int32_t width_q16, int32_t height_q16,
                           const std::vector<uint8_t>& logarithms) const;
};
CelGroup read_cel_group(const std::vector<uint8_t>& unpacked, uint32_t offset);

struct FamilyNode {
    uint32_t tag = 0; // zero for an array; other unported leaves retain their tag/offset
    std::vector<uint32_t> children;
};
// 0x0044aa80 relocates this hierarchy in the original. Native offsets remain
// bounded, owned offsets, without host pointers or mutable CCB templates.
struct Family {
    std::vector<uint8_t> bytes;
    std::map<uint32_t, FamilyNode> nodes;
    std::map<uint32_t, CelGroup> groups;
    std::optional<uint32_t> resolve(uint32_t category, uint32_t selector,
                                    std::optional<uint32_t> frame = std::nullopt) const;
};
Family read_family(const std::vector<uint8_t>& packed_payload);
} // namespace rr
