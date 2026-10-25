#include "road.hpp"
#include "bike.hpp"
#include <array>

namespace rr {
namespace {
constexpr uint32_t image_base = 0x00400000, depth_table_va = 0x0046f098;
constexpr size_t depth_entries = 4352; // last positive entry before zero padding

// Signed integer division by 256 truncating toward zero: (x + (x>>31 & 0xff)) >> 8.
int32_t div256(int64_t x) {
    return int32_t(x / 256);
}
} // namespace

DepthTable read_depth_table(const std::vector<uint8_t>& b) {
    auto range = [&](size_t p, size_t n) {
        if (p > b.size() || n > b.size() - p) throw std::runtime_error("Executable range outside file");
    };
    auto u16 = [&](size_t p) {
        range(p, 2);
        return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8;
    };
    auto u32 = [&](size_t p) {
        range(p, 4);
        return u16(p) | u16(p + 2) << 16;
    };
    if (u16(0) != 0x5a4d) throw std::runtime_error("Not an MZ executable");
    const size_t pe = u32(0x3c);
    if (u32(pe) != 0x4550 || u16(pe + 4) != 0x14c) throw std::runtime_error("Not an i386 PE executable");
    if (u32(pe + 24 + 28) != image_base) throw std::runtime_error("Unexpected image base");
    const size_t sections = pe + 24 + u16(pe + 20);
    const uint32_t rva = depth_table_va - image_base, bytes = depth_entries * 4;
    for (uint32_t i = 0; i < u16(pe + 6); ++i) {
        const size_t s = sections + 40 * i;
        const uint32_t va = u32(s + 12), raw_size = u32(s + 16), raw = u32(s + 20);
        if (rva < va || rva - va >= raw_size) continue;
        if (bytes > raw_size - (rva - va)) throw std::runtime_error("Depth table not in file data");
        DepthTable table;
        for (size_t k = 0; k < depth_entries; ++k)
            table.scale.push_back(int32_t(u32(raw + (rva - va) + 4 * k)));
        // Corroborate this is the reference binary's table: flat near clamp, 2^24/depth at 512,
        // non-increasing and positive throughout.
        if (table.scale[0] != 163840 || table.scale[256] != 32768)
            throw std::runtime_error("Unrecognised depth table");
        for (size_t k = 1; k < depth_entries; ++k)
            if (table.scale[k] <= 0 || table.scale[k] > table.scale[k - 1])
                throw std::runtime_error("Depth table not monotonic");
        return table;
    }
    throw std::runtime_error("Depth table address not mapped");
}

std::vector<RoadSlice> project_road(const std::vector<RouteSample>& route, uint32_t position,
                                    const Camera& camera, const DepthTable& table, int32_t logical_width) {
    if (logical_width < view_width || logical_width > 4096)
        throw std::runtime_error("Logical width out of range");
    std::vector<RoadSlice> slices;
    const uint32_t first = position >> 8;
    const int32_t frac = int32_t(position & 0xff);
    if (first >= route.size()) throw std::runtime_error("Camera beyond route");
    // Ring sub-slice samples: half 0 (+0x30) and half 1 (+0x194); single roads use half 0.
    struct Sub {
        const PathSample* path;
        int32_t offset;
    };
    auto subs = [&](const RouteSample& s) -> std::array<Sub, 2> {
        if (!s.dual) return {Sub{&s.path, s.offset}, Sub{&s.path, s.offset}};
        Sub own{&s.path, s.offset}, other{&s.other, s.other_offset};
        return s.half ? std::array<Sub, 2>{other, own} : std::array<Sub, 2>{own, other};
    };
    // 0x00443d70 local_50 (half 0) and local_30+2 (half 1): rise, y, dx, x.
    struct State {
        int32_t rise, y, dx, x;
    };
    State road[2]{};
    const auto& start = route[first];
    const int32_t camera_x = camera.x + start.offset;
    auto begin = [&](State& st, const PathSample& p) {
        st.rise = p.rise;
        st.y = -int32_t((int64_t(frac) * p.rise) >> 8);
        st.dx = div256(int64_t(p.bend) * (256 - frac));
        st.x = 0;
    };
    const auto first_subs = subs(start);
    bool dual = start.dual;
    if (!dual)
        begin(road[0], *first_subs[0].path);
    else {
        const int camera_road = start.half;
        begin(road[camera_road], *first_subs[camera_road].path);
        road[1 - camera_road].rise = first_subs[1 - camera_road].path->rise;
        road[1 - camera_road].y = -int32_t((int64_t(frac) * road[1 - camera_road].rise) >> 8);
        if (camera_road == 0) {
            road[1].dx = start.dx_sep + road[0].dx;
            road[1].x = start.x_sep;
        } else {
            road[0].dx = road[1].dx - start.dx_sep;
            road[0].x = -start.x_sep;
        }
    }
    int32_t depth = 256 - frac;
    for (uint32_t n = 0; n < view_slices && first + n < route.size(); ++n) {
        const auto& sample = route[first + n];
        const auto sub = subs(sample);
        if (n) {
            const auto& previous = route[first + n - 1];
            if (sample.dual && !previous.dual) {
                // Second roadway appears ahead: at a split it starts on the current road;
                // at a merge window it is offset by the stored separation.
                road[1] = road[0];
                if (sample.event != RoadEvent::split) {
                    if (sample.half == 0) {
                        road[1].dx += sample.dx_sep;
                        road[1].x += sample.x_sep;
                    } else {
                        road[0].dx -= sample.dx_sep;
                        road[0].x = road[1].x - sample.x_sep;
                    }
                }
            } else if (!sample.dual && previous.dual && sample.event != RoadEvent::merged &&
                       previous.half == 1) {
                road[0] = road[1]; // other road dropped: continue on half 1
            }
            for (int r = 0; r < (sample.dual ? 2 : 1); ++r) {
                road[r].dx += sub[r].path->bend;
                road[r].rise = sub[r].path->rise;
            }
        }
        const size_t index = size_t(depth & ~1) / 2;
        if (index >= table.scale.size()) throw std::runtime_error("Depth outside table");
        RoadSlice slice{first + n,
                        depth,
                        table.scale[index],
                        uint8_t(sample.dual ? 2 : 1),
                        uint8_t(sample.dual ? sample.half : 0),
                        {},
                        (logical_width & ~1) * 32768};
        for (int r = 0; r < slice.count; ++r) {
            slice.road[r] = {
                road[r].x + sub[r].offset - camera_x,
                int32_t(int64_t(camera.height - road[r].y) * slice.scale + int64_t(view_horizon) * 65536),
                -sub[r].path->left,
                sub[r].path->right,
                -sub[r].path->left - sub[r].path->shoulder_left,
                sub[r].path->right + sub[r].path->shoulder_right,
                road[r].y,
                approx_atan2(0x1000000, int32_t(uint32_t(road[r].dx) * 65536u)) * 65536};
            road[r].x += road[r].dx;
            road[r].y += road[r].rise;
        }
        slices.push_back(slice);
        depth += 256;
    }
    return slices;
}
int32_t advance_heading(int32_t heading, uint32_t previous, uint32_t current, int32_t previous_bend,
                        int32_t current_bend, bool frozen) {
    const int64_t delta = int64_t(current) - previous;
    if (frozen || !delta || delta > 256 || delta < -256) return heading;
    int64_t weighted;
    if (!((current ^ previous) & 0xffffff00u))
        weighted = int64_t(current_bend) * delta;
    else if (delta > 0)
        weighted = int64_t(previous_bend) * (256 - int32_t(previous & 255)) +
                   int64_t(current_bend) * (current & 255);
    else
        weighted = -int64_t(previous_bend) * (previous & 255) +
                   int64_t(current_bend) * (int32_t(current & 255) - 256);
    const uint32_t result = uint32_t(heading) + uint32_t(weighted) * 256u;
    return result <= INT32_MAX ? int32_t(result) : int32_t(int64_t(result) - 0x100000000LL);
}
int32_t horizon_scroll(int32_t heading, bool high) {
    const int32_t quarter = heading / 4;
    return -((quarter + quarter / 4) >> (high ? 15 : 16));
}

} // namespace rr
