#include "road_materials.hpp"
#include "assets.hpp"
#include <filesystem>
#include <iostream>
static void word(std::vector<uint8_t>& b, size_t p, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[p + i] = uint8_t(v >> (8 * i));
}
int main() {
    std::vector<uint8_t> exe(812);
    word(exe, 0, 0x5a4d);
    word(exe, 60, 128);
    word(exe, 128, 0x4550);
    word(exe, 132, 0x0001014c);
    word(exe, 148, 32);
    word(exe, 152, 0x10b);
    word(exe, 180, 0x400000);
    word(exe, 196, 0x6dfb0);
    word(exe, 200, 300);
    word(exe, 204, 512);
    const std::array<uint32_t, 7> destinations = {0x4c3cc0, 0x4c3cb0, 0x4c3cd0, 0x4c3cd8,
                                                  0x4c3ce0, 0x4c3ce4, 0x4c3ce8};
    std::vector<uint8_t> global(264 + 7 * 216);
    word(global, 0, 0x52535243);
    word(global, 12, 0x5254424c);
    word(global, 16, 24);
    word(global, 24, 0x5254424c);
    word(global, 32, 7);
    for (int i = 0; i < 7; ++i) {
        word(exe, 512 + i * 20, destinations[i]);
        word(exe, 516 + i * 20, 0x43454c20);
        word(exe, 520 + i * 20, uint32_t(i));
        const size_t row = 40 + i * 32, p = 264 + i * 216;
        word(global, row, 0x43454c20);
        word(global, row + 4, i + 1);
        word(global, row + 8, uint32_t(p));
        word(global, row + 12, 216);
        word(global, p, 0x43434220);
        word(global, p + 4, 80);
        word(global, p + 72, 8);
        word(global, p + 76, 8);
        word(global, p + 80, 0x50444154);
        word(global, p + 84, 136);
        word(global, p + 88, 0xbc614e);
        word(global, p + 96, 0x00070007);
        word(global, p + 120, 8);
        word(global, p + 124, 8);
        word(global, p + 128, 1);
        word(global, p + 144, 36);
        std::fill(global.begin() + p + 152, global.begin() + p + 216, uint8_t(20 + i));
    }
    try {
        const auto materials = rr::read_road_materials(exe, global);
        if (materials.entries[3].texture->mips[0].pixels != materials.entries[2].texture->mips[0].pixels)
            return 1;
        rr::RoadPanel panel;
        panel.material = rr::RoadPanelMaterial::normal2;
        panel.points = {{{2 * 65536, 2 * 65536},
                         {10 * 65536, 2 * 65536},
                         {10 * 65536, 10 * 65536},
                         {2 * 65536, 10 * 65536}}};
        const auto nodes = rr::road_draw_nodes({panel}, materials);
        rr::Framebuffer fb(320, 240);
        fb.clear(125);
        rr::render_nodes(fb, nodes, rr::view_mode(false), 16);
        if (fb.row(6)[6] != 26) return 1;
        auto corrupt = exe;
        word(corrupt, 520, 99);
        try {
            rr::read_road_materials(corrupt, global);
            return 1;
        } catch (const std::runtime_error&) {
        }
        try {
            rr::read_road_materials(exe, {global.begin(), global.end() - 1});
            return 1;
        } catch (const std::runtime_error&) {
        }
        const auto root = std::filesystem::path(RR_SOURCE_DIR) / "../cdrom/ROADRASH";
        if (std::filesystem::exists(root)) {
            rr::Assets assets(root.string());
            const auto real =
                rr::read_road_materials(assets.read("ROADRASH.EXE"), assets.read("DATA/GLOBAL.RSC"));
            for (const auto& cel : real.entries)
                if (!cel.texture) return 1;
            std::cout << "Eight imported road material slots bound\n";
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "Road material binding and synthetic rendering pass\n";
    return 0;
}
