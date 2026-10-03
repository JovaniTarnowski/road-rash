#pragma once
// Reader for oracle dump_scene files (aweman_proxy.c). Version 1: fixed ring/projected/scratch
// regions; version 2: .data/.bss image plus heap blocks. Addresses resolve through Memory.
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace rr::scene {
struct Region { uint32_t base = 0; std::vector<uint8_t> bytes; };

struct Memory {
    std::vector<Region> regions;
    const uint8_t* find(uint32_t a, uint32_t n) const {
        for (const auto& r : regions)
            if (a >= r.base && n <= r.bytes.size() && a - r.base <= r.bytes.size() - n) return r.bytes.data() + (a - r.base);
        return nullptr;
    }
    bool has(uint32_t a, uint32_t n = 4) const { return find(a, n) != nullptr; }
    uint32_t u32(uint32_t a) const {
        const uint8_t* p = find(a, 4);
        if (!p) throw std::runtime_error("Address not captured: " + std::to_string(a));
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }
    int32_t i32(uint32_t a) const { return int32_t(u32(a)); }
    uint8_t u8(uint32_t a) const { const uint8_t* p = find(a, 1); if (!p) throw std::runtime_error("Address not captured"); return *p; }
    int8_t i8(uint32_t a) const { return int8_t(u8(a)); }
};

struct Node {
    std::vector<uint8_t> bytes;      // 0xa8 source node
    std::vector<uint8_t> resource;   // 0x60 resource header
    uint32_t hash = 0, size = 0;
    uint32_t u32(size_t p) const { return uint32_t(bytes[p]) | uint32_t(bytes[p + 1]) << 8 | uint32_t(bytes[p + 2]) << 16 | uint32_t(bytes[p + 3]) << 24; }
};

struct Dump {
    uint32_t version = 0, frame = 0, node_base = 0, capacity = 0;
    Memory memory;   // state when the display list was complete (v1/v2/v3 post)
    Memory before;   // v3: state before 0x00443d70 projected the frame (empty if not captured)
    std::vector<Node> nodes;
};

inline Dump load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open " + path);
    const std::vector<uint8_t> b{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    size_t p = 0;
    auto take = [&](size_t n) { if (n > b.size() - p) throw std::runtime_error("Truncated scene dump"); const size_t at = p; p += n; return at; };
    auto word = [&]() { const size_t at = take(4); return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24; };
    Dump d;
    if (word() != 0x52524e53u) throw std::runtime_error("Not a scene dump");
    d.version = word(); d.frame = word();
    const uint32_t count = word();
    d.node_base = word(); d.capacity = word();
    const uint32_t a = word(), c = word();
    auto region_into = [&](Memory& m, uint32_t base, size_t size) { const size_t at = take(size); m.regions.push_back({base, {b.begin() + long(at), b.begin() + long(at + size)}}); };
    auto region = [&](uint32_t base, size_t size) { region_into(d.memory, base, size); };
    if (d.version == 1) {
        region(a, 128 * 0x2f8); region(c, 33 * 0x224); region(0x004ba190, 0x1320); region(0x004bb4b0, 33 * 0x240);
    } else if (d.version == 2) {
        for (uint32_t i = 0; i < a; ++i) { const uint32_t base = word(), size = word(); region(base, size); }
    } else if (d.version == 3) {
        for (uint32_t i = 0; i < a; ++i) { const uint32_t base = word(), size = word(); region_into(d.before, base, size); }
        for (uint32_t i = 0; i < c; ++i) { const uint32_t base = word(), size = word(); region(base, size); }
    } else throw std::runtime_error("Unknown scene dump version");
    for (uint32_t i = 0; i < count; ++i) {
        Node n;
        size_t at = take(0xa8); n.bytes.assign(b.begin() + long(at), b.begin() + long(at + 0xa8));
        at = take(0x60); n.resource.assign(b.begin() + long(at), b.begin() + long(at + 0x60));
        n.hash = word(); n.size = word();
        d.nodes.push_back(std::move(n));
    }
    return d;
}
}
