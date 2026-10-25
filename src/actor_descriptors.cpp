#include "actor_descriptors.hpp"
#include <stdexcept>
#include "exe_tables.hpp"
#include "resource.hpp"
#include <map>
namespace rr {
namespace {
struct Reader {
    const std::vector<uint8_t>& b;
    void range(size_t p, size_t n) const {
        if (p > b.size() || n > b.size() - p) throw std::runtime_error("Truncated actor descriptor resource");
    }
    uint32_t word(size_t p) const {
        range(p, 4);
        return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24;
    }
};
int32_t signed_word(uint32_t u) {
    return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
}
struct Layout {
    std::vector<uint32_t> flags, codes;
    ActorGraphicsGroups views;
    std::vector<std::array<uint32_t, 2>> info;
    explicit Layout(const std::vector<uint8_t>& bytes) {
        Reader r{bytes};
        for (size_t chunk = 0; chunk < bytes.size();) {
            const uint32_t extent = r.word(chunk + 4);
            if (extent < 8) throw std::runtime_error("Invalid actor chunk extent");
            r.range(chunk, extent);
            chunk += extent;
        }
        if (r.word(0) != 0x43414e53) throw std::runtime_error("Invalid actor SNAC");
        const uint32_t type = r.word(8);
        if (type < 1 || type > 4) throw std::runtime_error("Unsupported actor SNAC type");
        const uint32_t count = r.word(type == 1 ? 44 : 12);
        if (count > 65536) throw std::runtime_error("Actor descriptor table too large");
        size_t group = type == 1 ? 48 : 16;
        const size_t header = r.word(4);
        r.range(0, header);
        while (group < header) {
            const uint32_t id = r.word(group), frames = r.word(group + 4), delay = r.word(group + 8);
            if (!id || id > count || frames > 65536) throw std::runtime_error("Invalid actor graphics group");
            r.range(group, 16 + size_t(frames) * 28);
            if (info.size() < id) {
                info.resize(id);
                views.resize(id);
            }
            info[id - 1] = {frames, delay};
            auto& cells = views[id - 1];
            if (!cells.empty()) throw std::runtime_error("Duplicate actor graphics group");
            for (uint32_t f = 0; f < frames; ++f) {
                const uint32_t frame = (r.word(group + 16 + size_t(f) * 28) >> 16);
                if (!frame || frame > 32767) throw std::runtime_error("Invalid actor RRAN frame reference");
                cells.push_back({frame - 1, 0});
            }
            group += 16 + size_t(frames) * 28;
        }
        if (group != header || info.size() != count)
            throw std::runtime_error("Incomplete actor graphics groups");
        size_t cel = type == 1 ? 0 : header;
        cel += r.word(cel + 4) + 12;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t words = r.word(cel);
            if (!words || words > 65536) throw std::runtime_error("Invalid actor cel block");
            r.range(cel, 4 + size_t(words) * 4);
            flags.push_back(r.word(cel + 4));
            if (words < views.at(i).size()) throw std::runtime_error("Incomplete actor view flags");
            for (size_t f = 0; f < views[i].size(); ++f) views[i][f].flags = r.word(cel + 4 + f * 4);
            cel += 4 + size_t(words) * 4;
        }
        size_t table = header;
        for (int i = 0; i < 3; ++i) table += r.word(table + 4);
        table += 12;
        r.range(table, size_t(count) * 16);
        for (uint32_t i = 0; i < count; ++i) codes.push_back(r.word(table + size_t(i) * 16));
    }
};
} // namespace
ActorGraphicsGroups read_actor_graphics(const std::vector<uint8_t>& cans) {
    return Layout{cans}.views;
}
std::optional<std::vector<ActorGraphicsDescriptor>> read_actor_sequence(const std::vector<uint8_t>& cans,
                                                                        const std::vector<uint8_t>& source) {
    if (source.empty() || source.size() % 8) throw std::runtime_error("Invalid source animation descriptors");
    Layout l{cans};
    Reader r{source};
    std::vector<ActorGraphicsDescriptor> out;
    for (size_t p = 0; p < source.size(); p += 8) {
        const int64_t code = signed_word(r.word(p));
        const uint32_t wanted = uint32_t(code < 0 ? -code : code);
        std::array<int32_t, 3> start{-1, -1, -1}, end{};
        size_t cursor = 0;
        for (int group = 0; group < 3; ++group) {
            while (cursor < l.codes.size() && l.codes[cursor] != wanted) ++cursor;
            if (cursor == l.codes.size()) break;
            start[group] = int32_t(cursor);
            const auto flags = l.flags[cursor];
            ++cursor;
            while (cursor < l.codes.size() && l.codes[cursor] == wanted &&
                   (l.flags[cursor] & 6) == (flags & 6))
                ++cursor;
            end[group] = int32_t(cursor);
        }
        if (start[0] < 0) return std::nullopt;
        if (start[1] < 0) {
            start[1] = start[0];
            end[1] = end[0];
            start[0] = start[2] = -1;
        } else if (start[2] < 0) {
            if (l.flags[size_t(start[1])] & 6) {
                if (!(l.flags[size_t(start[1])] & 2)) {
                    start[2] = start[1];
                    start[1] = start[0];
                    start[0] = -1;
                    end[1] = end[0];
                } else {
                    const int32_t old = start[1], ends = end[0] ^ end[1];
                    start[1] = start[0];
                    start[0] = old;
                    end[1] = end[0];
                    end[0] = ends;
                }
            }
        } else {
            if (!(l.flags[size_t(start[0])] & 2)) {
                if (!(l.flags[size_t(start[1])] & 2)) {
                    const int32_t old = start[2], ends = end[0] ^ end[2];
                    start[2] = start[0];
                    start[0] = old;
                    end[2] = end[0];
                    end[0] = ends;
                } else {
                    const int32_t old = start[1], ends = end[0] ^ end[1];
                    start[1] = start[0];
                    start[0] = old;
                    end[1] = end[0];
                    end[0] = ends;
                }
            }
            if (l.flags[size_t(start[1])] & 4) {
                std::swap(start[1], start[2]);
                std::swap(end[1], end[2]);
            }
        }
        const uint32_t middle = uint32_t(start[1]), flags = l.flags.at(middle);
        const auto info = l.info.at(middle);
        ActorGraphicsDescriptor result;
        result.source_frame = middle;
        auto& frame = result.playback;
        frame.flags = flags;
        for (int i = 0; i < 4; ++i) frame.render[i] = uint8_t(middle >> (i * 8));
        frame.render[4] = source[p + 4];
        if (!(flags & 0xf800)) {
            frame.render[9] = 0;
            frame.render[5] = 0;
            frame.render[6] = uint8_t(info[0]);
        } else {
            frame.render[9] = 1;
            frame.render[5] = uint8_t(info[0]);
            frame.render[6] = uint8_t(end[1] - start[1]);
        }
        const auto bit = [&](uint32_t mask, uint32_t value) { return flags & mask ? value : 0u; };
        frame.render[7] = uint8_t(((bit(0x2000, 0xaaaaa) + bit(0x8000, 0x200000) + bit(0x1000, 0x55555) +
                                    bit(0x4000, 0x155555) + bit(0x800, 0x2aaaa)) >>
                                   16) *
                                  2);
        frame.render[8] = uint8_t(((bit(0x400, 0x400000) + bit(0x40, 0x55555) + bit(0x200, 0x2aaaaa) +
                                    bit(0x80, 0xaaaaa) + bit(0x20, 0x2aaaa) + bit(0x100, 0x155555)) >>
                                   16) *
                                  2);
        frame.render[10] = start[0] < 0 ? 0 : uint8_t(start[0] - start[1]);
        frame.render[11] = start[2] < 0 ? 0 : uint8_t(start[2] - start[1]);
        frame.duration = int8_t(uint8_t(info[1] / 20));
        frame.control = source[p + 5];
        out.push_back(result);
    }
    return out;
}
ActorViewSelection select_actor_view(const ActorAnimationFrame& descriptor, const ActorGraphicsGroups& views,
                                     int32_t yaw, int32_t roll) {
    const auto& d = descriptor.render;
    const auto bits = [](uint32_t u) {
        return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
    };
    const auto add = [&](int32_t a, int32_t b) { return bits(uint32_t(a) + uint32_t(b)); };
    const auto mul = [&](int32_t a, int32_t b) { return bits(uint32_t(a) * uint32_t(b)); };
    const auto byte = [](uint8_t u) { return u < 128 ? int32_t(u) : int32_t(u) - 256; };
    const uint32_t base = uint32_t(d[0]) | uint32_t(d[1]) << 8 | uint32_t(d[2]) << 16 | uint32_t(d[3]) << 24;
    const auto& first = views.at(base);
    if (first.empty()) throw std::runtime_error("Empty actor view group");
    if (first[0].flags & 8) yaw = add(yaw, 0x7f0000);
    if (yaw > 0x7effff) yaw = add(yaw, -0xff0000);
    if (roll > 0x7effff) roll = add(roll, -0xff0000);
    int32_t view = 0;
    if (d[8]) {
        if (d[8] < 250) {
            const int32_t factor = (byte(d[6] & 0xfe) * 4096) / d[8];
            view = add(mul(factor, int32_t(d[8]) * 128 + yaw / 256) / 16, 0x8000);
        } else {
            const int32_t count = byte(d[6]);
            if (count == 2 || count == 4 || count == 8)
                view = add(yaw, 0x800000) / (count == 2 ? 128 : count == 4 ? 64 : 32);
            else
                view = mul(add(yaw, 0x800000), count) / 256;
        }
        view >>= 16;
        if (view >= byte(d[6])) view = byte(d[6]) - 1;
        if (view < 0) view = 0;
    }
    int32_t elevation = 0, result_roll = roll;
    if (d[9] && byte(d[5]) >= 2) {
        const int32_t step = (int32_t(d[7]) * 65536) / byte(d[5]);
        if (!step) throw std::runtime_error("Zero actor elevation step");
        elevation = byte(uint8_t(add(int32_t(d[7]) * 32768, roll) / step));
        elevation = std::max(0, std::min(byte(d[5]) - 1, elevation));
        const uint32_t flags = views.at(base + uint32_t(view)).at(size_t(elevation)).flags;
        int32_t correction = (flags & 0x2000 ? 0xaaaaa : 0) + (flags & 0x8000 ? 0x200000 : 0) +
                             (flags & 0x800 ? 0x2aaaa : 0) + (flags & 0x1000 ? 0x55555 : 0) +
                             (flags & 0x4000 ? 0x155555 : 0);
        correction = std::max(-7, std::min(7, correction));
        if (flags & 0x10) correction = -correction;
        result_roll = add(roll, -correction);
    }
    const uint32_t group = base + (d[9] ? uint32_t(view) : 0);
    const uint32_t subframe = uint32_t(byte(d[4]) + (d[9] ? elevation : view));
    (void)views.at(group).at(subframe); // bounds check only; indices recomputed below
    return {group, subframe, result_roll};
}
ActorAnimationLibrary::ActorAnimationLibrary(const std::vector<uint8_t>& exe,
                                             const std::vector<uint8_t>& global) {
    // Address/count schema lifted from 0x00445a60. All descriptor values and
    // graphics remain imported; these identify its runtime relocation slots.
    struct Binding {
        uint32_t bank, source, destination, count;
    };
    static constexpr Binding bindings[] = {
        {0, 0x004746c0, 0x004b92e0, 1}, {0, 0x004746c8, 0x004b93f0, 1}, {0, 0x004746d0, 0x004b9200, 1},
        {0, 0x004746d8, 0x004b9470, 1}, {0, 0x004746e0, 0x004b9a90, 1}, {0, 0x004746e8, 0x004b93c0, 2},
        {0, 0x004746f8, 0x004b9bb0, 2}, {0, 0x00474708, 0x004b9450, 1}, {0, 0x00474710, 0x004b9ef0, 1},
        {0, 0x00474718, 0x004b9b40, 2}, {0, 0x00474728, 0x004b99b0, 5}, {0, 0x00474750, 0x004b96c0, 5},
        {0, 0x00474778, 0x004b9a20, 5}, {0, 0x004747a0, 0x004b9650, 5}, {0, 0x004747c8, 0x004b9100, 2},
        {0, 0x004747d8, 0x004b98f0, 2}, {0, 0x004747e8, 0x004b9130, 3}, {0, 0x00474800, 0x004b9190, 3},
        {0, 0x00474818, 0x004b9020, 3}, {0, 0x00474830, 0x004b9380, 3}, {0, 0x00474868, 0x004b9410, 3},
        {0, 0x00474880, 0x004b9300, 3}, {0, 0x00474898, 0x004b9eb0, 3}, {0, 0x004748b0, 0x004b9b70, 3},
        {0, 0x004748c8, 0x004b91d0, 2}, {0, 0x004748d8, 0x004b92b0, 2}, {0, 0x004748e8, 0x004b9860, 7},
        {0, 0x00474920, 0x004b9540, 8}, {0, 0x00474960, 0x004b97a0, 4}, {0, 0x00474980, 0x004b9920, 4},
        {0, 0x004749a0, 0x004ba060, 4}, {0, 0x004749c0, 0x004b9d00, 5}, {0, 0x004749e8, 0x004b94f0, 4},
        {0, 0x00474a08, 0x004b9b10, 2}, {0, 0x00474a18, 0x004b9060, 5}, {0, 0x00474a40, 0x004b95e0, 4},
        {0, 0x00474a60, 0x004b9d70, 6}, {0, 0x00474a90, 0x004b9fa0, 5}, {0, 0x00474ab8, 0x004b9c20, 5},
        {0, 0x00474ae0, 0x004b9f10, 5}, {0, 0x00474b08, 0x004b97f0, 5}, {0, 0x00474b30, 0x004b9e20, 5},
        {0, 0x00474b58, 0x004ba010, 4}, {0, 0x00474b78, 0x004b9230, 4}, {0, 0x00474b98, 0x004b9970, 3},
        {0, 0x00474bb0, 0x004ba0b0, 3}, {0, 0x00474bc8, 0x004b9f80, 1}, {0, 0x00474bd0, 0x004b9170, 1},
        {1, 0x004746d0, 0x004b9af0, 1}, {1, 0x00474710, 0x004b9e90, 1}, {1, 0x00474718, 0x004b90d0, 2},
        {1, 0x004747e8, 0x004b9ab0, 3}, {1, 0x00474800, 0x004b9340, 3}, {1, 0x004748e8, 0x004ba0f0, 7},
        {1, 0x004749c0, 0x004b9c90, 5}, {1, 0x00474b30, 0x004b9730, 5}, {1, 0x00474bc8, 0x004b9630, 1},
        {1, 0x00474b98, 0x004b9be0, 3}, {1, 0x00474a40, 0x004b9490, 4}};
    std::array<std::vector<uint8_t>, 2> cans;
    std::array<bool, 4> loaded{};
    for (const auto& resource : read_resources(global)) {
        if (resource.tag == "CANS" && resource.id >= 1 && resource.id <= 2)
            cans[resource.id - 1] = {global.begin() + resource.offset,
                                     global.begin() + resource.offset + resource.size};
        if (resource.tag == "ANIM" && resource.id >= 1 && resource.id <= 4) {
            graphics[resource.id - 1] = read_animation(
                {global.begin() + resource.offset, global.begin() + resource.offset + resource.size});
            loaded[resource.id - 1] = true;
        }
    }
    for (size_t bank = 0; bank < views.size(); ++bank) {
        views[bank] = read_actor_graphics(cans[bank]);
        for (const auto& group : views[bank])
            for (const auto& cell : group)
                if (cell.frame >= graphics[bank].frames.size() ||
                    cell.frame >= graphics[bank + 2].frames.size())
                    throw std::runtime_error("Actor graphics reference outside RRAN");
    }
    for (bool present : loaded)
        if (!present) throw std::runtime_error("Missing actor RRAN graphics");
    std::map<uint32_t, std::optional<std::vector<ActorGraphicsDescriptor>>> relocated;
    for (const auto& binding : bindings)
        relocated.emplace(binding.destination,
                          read_actor_sequence(cans.at(binding.bank),
                                              read_exe_data(exe, binding.source, binding.count * 8)));
    const auto word = [](const std::vector<uint8_t>& bytes, size_t p) {
        return uint32_t(bytes[p]) | uint32_t(bytes[p + 1]) << 8 | uint32_t(bytes[p + 2]) << 16 |
               uint32_t(bytes[p + 3]) << 24;
    };
    const auto fallback_bytes = read_exe_data(exe, 0x464430, 52 * 4);
    for (size_t code = 0; code < 52; ++code) {
        const uint8_t value = uint8_t(word(fallback_bytes, code * 4));
        fallback.push_back(value < 128 ? int8_t(value) : int8_t(int(value) - 256));
    }
    const auto table = read_exe_data(exe, 0x474d78, 8);
    for (int type = 0; type < 2; ++type) {
        const auto pointers = read_exe_data(exe, word(table, type * 4), 52 * 4);
        for (int code = 0; code < 52; ++code) {
            const uint32_t pointer = word(pointers, code * 4);
            if (!pointer) {
                descriptors[type].push_back(std::nullopt);
                playback[type].push_back(std::nullopt);
                continue;
            }
            const auto it = relocated.find(pointer);
            if (it == relocated.end()) {
                // These two entries point to BSS descriptors never filled by
                // 0x00445a60; +16 remains zero and direct selection rejects them.
                if (pointer != 0x4b9df0 && pointer != 0x4b9280)
                    throw std::runtime_error("Unresolved actor animation relocation slot");
                descriptors[type].push_back(std::nullopt);
                playback[type].push_back(std::nullopt);
                continue;
            }
            descriptors[type].push_back(it->second);
            if (!it->second) {
                playback[type].push_back(std::nullopt);
                continue;
            }
            ActorSequence sequence;
            for (const auto& frame : *it->second) sequence.push_back(frame.playback);
            playback[type].push_back(std::move(sequence));
        }
    }
}

} // namespace rr
