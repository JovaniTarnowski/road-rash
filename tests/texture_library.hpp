#pragma once
// Every indexed pixel resource on the disc, for identifying oracle display-node resources by the
// FNV-1a hash of mip 0 (test-only identification; the game resolves resources structurally).
#include "assets.hpp"
#include "cel.hpp"
#include "explode.hpp"
#include "resource.hpp"
#include <map>
#include <string>
#include <vector>

namespace rr::testlib {
inline uint32_t fnv(const uint8_t* p, size_t n) { uint32_t h = 2166136261u; for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; } return h; }

struct TextureLibrary {
    std::vector<CelTexture> textures;
    std::multimap<uint32_t, size_t> by_hash;   // hash of the first `size` bytes is checked on lookup
    const CelTexture* find(uint32_t hash, uint32_t size) const {
        for (const auto& t : textures)
            if (!t.mips.empty() && size && t.mips[0].pixels.size() >= size && fnv(t.mips[0].pixels.data(), size) == hash) return &t;
        return nullptr;
    }
};

inline TextureLibrary load_texture_library(const Assets& assets, const std::string& course_file) {
    TextureLibrary lib;
    auto le32 = [](const std::vector<uint8_t>& b, size_t p) { return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24; };
    auto scan = [&](const std::string& path) {
        if (!assets.exists(path)) return;
        const auto file = assets.read(path);
        for (const auto& r : read_resources(file)) {
            std::vector<uint8_t> payload(file.begin() + r.offset, file.begin() + r.offset + r.size);
            if (r.tag == "CEL ") {
                try { const Cel c = read_cel(payload); if (c.texture) lib.textures.push_back(*c.texture); } catch (const std::runtime_error&) {}
                continue;
            }
            try { payload = unpack_resource(payload); } catch (const std::runtime_error&) {}
            for (size_t q = 0; q + 56 <= payload.size(); q += 4)
                if (le32(payload, q) == 0x00bc614e)
                    try { lib.textures.push_back(read_indexed_texture(payload.data() + q, payload.size() - q)); } catch (const std::runtime_error&) {}
        }
    };
    scan("DATA/GLOBAL.RSC");
    scan("DATA/COURSES/" + course_file + ".CRS");
    scan("DATA/CARS/" + course_file + ".CAR");
    scan("DATA/FAMILIES.RSC");
    return lib;
}
}
