// Natural-frame display-list check: the original's own pending display nodes for a captured race
// frame (oracle dump_scene) are converted to native DrawNodes, resources identified by their mip-0
// hash, and rendered over the original frame. Every pixel must equal the original final pixel.
#include "golden.hpp"
#include "rider_sprites.hpp"
#include "scene_dump.hpp"
#include "texture_library.hpp"
#include "view.hpp"
#include "world_renderer.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

namespace {
const char* const course_files[5] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};   // 0x0046df58
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
}

int main(int argc, char** argv) {
    const std::string dir = rr::golden::oracle_dir();
    const std::string tsv = argc > 1 ? argv[1] : dir + "/city-scene.tsv";
    rr::Assets assets(argc > 2 ? argv[2] : RR_SOURCE_DIR "/../cdrom/ROADRASH");
    std::ifstream probe(tsv);
    if (!probe || !assets.exists("DATA/GLOBAL.RSC")) { std::cout << "skip: no scene capture or disc\n"; return rr::golden::skip; }
    const auto golden = rr::golden::load(tsv);
    const std::string stem = tsv.substr(0, tsv.size() - 4);
    const int course = int(golden.frames.at(0).at("course"));
    const auto lib = rr::testlib::load_texture_library(assets, course_files[course]);
    const auto exe = assets.read("ROADRASH.EXE");
    const auto sprites = rr::load_rider_sprites(assets, exe, std::getenv("RR_SCENE_LOBIKES") == nullptr);
    std::cout << lib.textures.size() << " textures\n";
    int failures = 0, frames = 0;
    for (const auto& f : golden.frames) {
        if (!f.count("scene.file") || !f.count("pixels.file")) continue;
        const auto dump = rr::scene::load(stem + "-scene" + std::to_string(f.at("scene.file")) + ".raw");
        const auto pixels = read_file(stem + "-f" + std::to_string(f.at("pixels.file")) + ".raw");
        const int width = int(f.at("pixels.width")), height = int(f.at("pixels.height"));
        if (pixels.size() != size_t(width) * size_t(height)) { ++failures; continue; }
        rr::Framebuffer fb(width, height);
        for (int y = 0; y < height; ++y) std::memcpy(fb.row(y), pixels.data() + size_t(y) * size_t(width), size_t(width));
        std::vector<rr::DrawNode> nodes;
        std::vector<std::unique_ptr<rr::CelTexture>> dynamic;
        int unmatched = 0;
        for (const auto& s : dump.nodes) {
            rr::DrawNode n;
            for (int k = 0; k < 4; ++k) n.view_points[size_t(k)] = {int32_t(s.u32(0x44 + 8 * size_t(k))), int32_t(s.u32(0x48 + 8 * size_t(k)))};
            n.flip_u = s.bytes[0x64] != 0;
            n.flip_v = s.bytes[0x65] != 0;
            if (s.u32(8) == 0x00466684) {   // dynamic "Bob" resource (0x00466688 holds its address)
                auto t = rr::rider_texture(sprites, int32_t(s.u32(0x38)), int32_t(s.u32(0x34)));
                if (!t) continue;
                dynamic.push_back(std::make_unique<rr::CelTexture>(std::move(*t)));
                n.texture = dynamic.back().get();
            } else if (s.bytes[0x66]) n.flat_index = s.bytes[0x67];
            else if (!(n.texture = lib.find(s.hash, s.size))) { ++unmatched; continue; }
            nodes.push_back(n);
        }
        try {
            for(size_t i=0;i<nodes.size();++i) {
                try {rr::render_nodes(fb,{nodes[i]},rr::view_mode(f.at("view.high") != 0),uint32_t(f.count("view.quality_mask") ? f.at("view.quality_mask") : 16));}
                catch(const std::exception& e) {
                    const auto* t=nodes[i].texture;
                    if(t)std::cerr<<"texture "<<t->name<<" bounds "<<t->bounds[0]<<','<<t->bounds[1]<<','<<t->bounds[2]<<','<<t->bounds[3]<<" mip0 "<<t->mips[0].width<<'x'<<t->mips[0].height<<'\n';
                    throw std::runtime_error("node "+std::to_string(i)+": "+e.what());
                }
            }
        } catch (const std::exception& e) { std::cerr << "frame " << f.at("scene.file") << ": " << e.what() << "\n"; ++failures; continue; }
        size_t differ = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) differ += fb.row(y)[x] != pixels[size_t(y) * size_t(width) + size_t(x)];
        std::cout << "frame " << f.at("scene.file") << ": " << dump.nodes.size() << " nodes, " << nodes.size() << " rendered, "
                  << unmatched << " unmatched, " << differ << " differing pixels\n";
        if (differ || unmatched) {
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
