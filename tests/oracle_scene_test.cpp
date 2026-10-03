// Natural-frame display-list check: the original's own pending display nodes for a captured race
// frame (oracle dump_scene) are converted to native DrawNodes, resources identified by their mip-0
// hash among the user's decoded CELs, and rendered over the original frame. Every pixel a native node
// writes must equal the original final pixel.
#include "assets.hpp"
#include "cel.hpp"
#include "explode.hpp"
#include "golden.hpp"
#include "rider_sprites.hpp"
#include <memory>
#include "resource.hpp"
#include "view.hpp"
#include "world_renderer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <functional>
#include <map>

namespace {
uint32_t fnv(const uint8_t* p, size_t n) { uint32_t h = 2166136261u; for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; } return h; }
uint32_t le32(const std::vector<uint8_t>& b, size_t p) { return uint32_t(b.at(p)) | uint32_t(b.at(p + 1)) << 8 | uint32_t(b.at(p + 2)) << 16 | uint32_t(b.at(p + 3)) << 24; }
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
const char* const course_files[5] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};   // 0x0046df58
}

int main(int argc, char** argv) {
    const std::string dir = rr::golden::oracle_dir();
    const std::string tsv = argc > 1 ? argv[1] : dir + "/city-scene.tsv";
    rr::Assets assets(argc > 2 ? argv[2] : RR_SOURCE_DIR "/../cdrom/ROADRASH");
    std::ifstream probe(tsv);
    if (!probe || !assets.exists("DATA/GLOBAL.RSC")) { std::cout << "skip: no scene capture or disc\n"; return rr::golden::skip; }
    const auto golden = rr::golden::load(tsv);
    const std::string stem = tsv.substr(0, tsv.size() - 4);
    // Decode every CEL in GLOBAL.RSC and the captured course, keyed by the mip-0 hash.
    std::vector<rr::CelTexture> textures;
    const int course = int(golden.frames.at(0).at("course"));
    auto add_cels = [&](const std::vector<uint8_t>& file) {
        for (const auto& r : rr::read_resources(file)) {
            if (r.tag != "CEL ") continue;
            try {
                const rr::Cel cel = rr::read_cel({file.begin() + r.offset, file.begin() + r.offset + r.size});
                if (cel.texture) textures.push_back(*cel.texture);
            } catch (const std::runtime_error&) {}
        }
    };
    add_cels(assets.read("DATA/GLOBAL.RSC"));
    // FAM records (0x0044aa80): a dword count with relative offsets to nested chunks; CCB leaves are
    // followed by their PLUT and pixel chunks.
    int fam_failures = 0;
    std::function<void(const std::vector<uint8_t>&, size_t, int)> walk = [&](const std::vector<uint8_t>& b, size_t at, int depth) {
        if (depth > 8 || at + 8 > b.size()) return;
        const uint32_t tag = le32(b, at);
        if (tag == 0x43434220) {
            size_t q = at;
            while (q + 8 <= b.size()) {
                const uint32_t t = le32(b, q), e = le32(b, q + 4);
                if (e < 8 || e > b.size() - q) break;
                if (t != 0x43434220 && t != 0x504c5554 && t != 0x50444154 && t != 0x52504454 && t != 0x52525044) break;
                if (t == 0x43434220 && q != at) break;
                q += e;
            }
            try {
                const rr::Cel cel = rr::read_cel({b.begin() + long(at), b.begin() + long(q)});
                if (cel.texture) textures.push_back(*cel.texture);
            } catch (const std::runtime_error&) { ++fam_failures; }
            return;
        }
        if (tag == 0x434c4750) {
            // Cel group (0x0044a660 mode 0 grid, 0x0044a760 mode 1 list): pixel blocks at
            // offsets (relative to +8) in the dword table starting at byte +0x15's index.
            if (at + 0x1c > b.size()) return;
            const int mode = b[at + 0x14] & 3;
            if (std::getenv("RR_SCENE_CLGP")) { std::cerr << "CLGP at " << at << ":"; for (size_t k = 0; k < 0x40 && at + k < b.size(); ++k) fprintf(stderr, " %02x", b[at + k]); std::cerr << "\n"; }
            const size_t table = at + size_t(b[at + 0x15]) * 4;
            std::vector<size_t> blocks;
            if (mode == 0) {
                // 0x0044a660: entry j (j <= byte +0x19) points (relative to the chunk) at a list of
                // byte +0x18 + 1 block offsets (relative to +8).
                for (size_t j = 0; j <= b[at + 0x19] && table + 4 * j + 4 <= b.size(); ++j) {
                    const size_t list = at + le32(b, table + 4 * j);
                    for (size_t i = 0; i <= b[at + 0x18] && list + 4 * i + 4 <= b.size(); ++i) blocks.push_back(at + 8 + le32(b, list + 4 * i));
                }
            } else if (mode == 1) {
                for (size_t i = 0; i <= b[at + 0x18] && table + 4 * i + 4 <= b.size(); ++i) blocks.push_back(at + 8 + le32(b, table + 4 * i));
            }
            for (size_t block : blocks) {
                if (block >= b.size()) { ++fam_failures; continue; }
                try { textures.push_back(rr::read_indexed_texture(b.data() + block, b.size() - block)); }
                catch (const std::runtime_error& e) { ++fam_failures; if (std::getenv("RR_SCENE_NAMES")) std::cerr << "clgp fail " << e.what() << " magic " << std::hex << le32(b, block) << std::dec << "\n"; }
            }
            return;
        }
        if (tag & 0xffff0000u || tag & 0xf000) return;   // other leaf chunk
        for (uint32_t i = 0; i < tag && at + 8 + 4 * i <= b.size(); ++i) {
            const uint32_t off = le32(b, at + 4 + 4 * i);
            if (off) walk(b, at + off, depth + 1);
        }
    };
    {
        const auto fam = assets.read("DATA/FAMILIES.RSC");
        for (const auto& r : rr::read_resources(fam)) {
            if (r.tag != "FAM " || r.size < 8) continue;
            try { walk(rr::unpack_resource(std::vector<uint8_t>(fam.begin() + r.offset, fam.begin() + r.offset + r.size)), 0, 0); }
            catch (const std::runtime_error&) { ++fam_failures; }
        }
    }
    // Animation sets (ANIM/CANS in GLOBAL.RSC and the course .CAR) hold further pixel resources;
    // for identification every 0xbc614e pixel resource in a (possibly packed) payload is decoded.
    auto scan = [&](const std::vector<uint8_t>& file, const char* only) {
        for (const auto& r : rr::read_resources(file)) {
            if (only && r.tag != only) continue;
            std::vector<uint8_t> payload(file.begin() + r.offset, file.begin() + r.offset + r.size);
            try { payload = rr::unpack_resource(payload); } catch (const std::runtime_error&) {}
            for (size_t q = 0; q + 56 <= payload.size(); q += 4)
                if (le32(payload, q) == 0x00bc614e)
                    try { textures.push_back(rr::read_indexed_texture(payload.data() + q, payload.size() - q)); } catch (const std::runtime_error&) {}
        }
    };
    scan(assets.read("DATA/GLOBAL.RSC"), "ANIM");
    scan(assets.read("DATA/GLOBAL.RSC"), "CANS");
    scan(assets.read(std::string("DATA/CARS/") + course_files[course] + ".CAR"), nullptr);
    scan(assets.read("DATA/FAMILIES.RSC"), "FAM ");
    std::cout << textures.size() << " textures (" << fam_failures << " family CCBs not decodable)\n";
    if (std::getenv("RR_SCENE_NAMES")) for (const auto& t : textures) std::cerr << "texture " << t.name << " " << std::hex << (t.mips.empty() ? 0 : fnv(t.mips[0].pixels.data(), t.mips[0].pixels.size())) << std::dec << "\n";
    add_cels(assets.read(std::string("DATA/COURSES/") + course_files[course] + ".CRS"));
    const auto exe = assets.read("ROADRASH.EXE");
    const auto sprites = rr::load_rider_sprites(assets, exe, std::getenv("RR_SCENE_LOBIKES") == nullptr);
    int failures = 0, frames = 0;
    for (const auto& f : golden.frames) {
        if (!f.count("scene.file") || !f.count("pixels.file")) continue;
        const auto scene = read_file(stem + "-scene" + std::to_string(f.at("scene.file")) + ".raw");
        const auto pixels = read_file(stem + "-f" + std::to_string(f.at("pixels.file")) + ".raw");
        if (scene.size() < 32 || pixels.empty()) { ++failures; std::cerr << "missing dump files\n"; continue; }
        const uint32_t count = le32(scene, 12);
        size_t p = 32 + 128 * 0x2f8 + 33 * 0x224 + 0x1320 + 33 * 0x240;
        const bool high = f.at("view.high") != 0;
        const rr::ViewMode mode = rr::view_mode(high);
        const int width = int(f.at("pixels.width")), height = int(f.at("pixels.height"));
        rr::Framebuffer fb(width, height);
        for (int y = 0; y < height; ++y) std::memcpy(fb.row(y), pixels.data() + size_t(y) * size_t(width), size_t(width));
        const auto original = pixels;
        std::vector<rr::DrawNode> nodes;
        std::vector<std::unique_ptr<rr::CelTexture>> dynamic;
        int unmatched = 0, flat = 0;
        std::map<uint32_t, int> unknown_hashes;
        for (uint32_t i = 0; i < count; ++i, p += 0xa8 + 0x60 + 8) {
            if (p + 0xa8 + 0x68 > scene.size()) { ++failures; break; }
            rr::DrawNode n;
            for (int k = 0; k < 4; ++k) n.view_points[size_t(k)] = {int32_t(le32(scene, p + 0x44 + 8 * size_t(k))), int32_t(le32(scene, p + 0x48 + 8 * size_t(k)))};
            n.flip_u = scene[p + 0x64] != 0;
            n.flip_v = scene[p + 0x65] != 0;
            const uint32_t hash = le32(scene, p + 0xa8 + 0x60), size = le32(scene, p + 0xa8 + 0x64);
            if (le32(scene, p + 8) == 0x00466684) {   // dynamic "Bob" resource (0x00466688 holds its address)
                auto t = rr::rider_texture(sprites, int32_t(le32(scene, p + 0x38)), int32_t(le32(scene, p + 0x34)));
                if (!t) continue;
                dynamic.push_back(std::make_unique<rr::CelTexture>(std::move(*t)));
                n.texture = dynamic.back().get();
                nodes.push_back(n);
                continue;
            }
            if (scene[p + 0x66]) { n.flat_index = scene[p + 0x67]; ++flat; }
            else {
                const rr::CelTexture* match = nullptr;
                for (const auto& t : textures)
                    if (!t.mips.empty() && t.mips[0].pixels.size() >= size && size && fnv(t.mips[0].pixels.data(), size) == hash) { match = &t; break; }
                if (!match) { ++unmatched; ++unknown_hashes[hash]; continue; }
                if (std::getenv("RR_SCENE_DEBUG")) {
                    const size_t h = p + 0xa8;
                    auto s16 = [&](size_t q) { return int16_t(uint16_t(scene[q] | scene[q + 1] << 8)); };
                    const bool same = s16(h + 4) == match->bounds[0] && s16(h + 6) == match->bounds[1] && s16(h + 8) == match->bounds[2] &&
                                      s16(h + 10) == match->bounds[3] && le32(scene, h + 12) == match->flags;
                    if (!same) std::cerr << "node " << i << " hash " << std::hex << hash << std::dec << " dump bounds " << s16(h + 4) << "," << s16(h + 6) << "," << s16(h + 8) << "," << s16(h + 10)
                                         << " flags " << std::hex << le32(scene, h + 12) << std::dec << " vs " << match->bounds[0] << "," << match->bounds[1] << "," << match->bounds[2] << "," << match->bounds[3]
                                         << " flags " << std::hex << match->flags << std::dec << " levels " << le32(scene, h + 40) << "/" << match->mips.size() << "\n";
                }
                n.texture = match;
            }
            nodes.push_back(n);
        }
        try {
            rr::render_nodes(fb, nodes, mode, uint32_t(f.count("view.quality_mask") ? f.at("view.quality_mask") : 16));
        } catch (const std::exception& e) { std::cerr << "frame " << f.at("scene.file") << ": " << e.what() << "\n"; ++failures; continue; }
        size_t differ = 0;
        for (size_t i = 0; i < original.size(); ++i) differ += fb.row(int(i / size_t(width)))[i % size_t(width)] != original[i];
        std::cout << "frame " << f.at("scene.file") << ": " << count << " nodes, " << nodes.size() << " rendered (" << flat << " flat), "
                  << unmatched << " unmatched resources (" << unknown_hashes.size() << " distinct), " << differ << " differing pixels\n";
        if (differ) {
            ++failures;
            std::ofstream out(stem + "-scene" + std::to_string(f.at("scene.file")) + "-native.raw", std::ios::binary);
            for (int y = 0; y < height; ++y) out.write(reinterpret_cast<const char*>(fb.row(y)), width);
        }
        ++frames;
    }
    if (!frames) { std::cout << "skip: capture has no scene frames\n"; return rr::golden::skip; }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
