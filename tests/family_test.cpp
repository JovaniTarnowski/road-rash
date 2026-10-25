#include "family.hpp"
#include "assets.hpp"
#include "resource.hpp"
#include "exe_tables.hpp"
#include "golden.hpp"
#include <iostream>

using Bytes = std::vector<uint8_t>;
static void word(Bytes& b, size_t p, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.at(p + size_t(i)) = uint8_t(v >> (8 * i));
}
static Bytes packed(const Bytes& b) {
    Bytes result(16);
    word(result, 0, uint32_t(b.size()));
    word(result, 4, uint32_t(b.size()));
    result.insert(result.end(), b.begin(), b.end());
    return result;
}
static Bytes fixture() {
    Bytes b(384);
    word(b, 0, 0x434c4750);
    word(b, 4, 36);
    word(b, 8, 52);
    word(b, 12, 120);
    word(b, 16, 128);
    b[20] = 0x81;
    b[21] = 7;
    b[22] = 10;
    b[24] = 2;
    for (unsigned i = 0; i < 3; ++i) {
        b[40 + 2 * i] = uint8_t(4u << i);
        b[41 + 2 * i] = uint8_t(2u << i);
        const size_t p = 160 + 76 * i;
        word(b, 28 + 4 * i, uint32_t(p - 8));
        word(b, p, 0xbc614e);
        word(b, p + 8, 0x00020004);
        word(b, p + 32, 4);
        word(b, p + 36, 2);
        word(b, p + 40, 1);
        word(b, p + 56, 36);
        for (unsigned j = 0; j < 8; ++j) b[p + 64 + j] = uint8_t(i * 10 + j);
    }
    return b;
}
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            ++failures;
            std::cerr << "FAIL " << what << '\n';
        }
    };
    auto reject_group = [&](const Bytes& b) {
        try {
            rr::read_cel_group(b, 0);
            check(false, "invalid group accepted");
        } catch (const std::runtime_error&) {
        }
    };
    auto b = fixture();
    auto group = rr::read_cel_group(b, 0);
    check(group.cells.size() == 3 && group.cells[2].width == 16 && group.cells[2].height == 8,
          "group dimensions");
    check(group.select(7 * 65536, 65536, {}).texture.mips[0].pixels[0] == 0,
          "width threshold below boundary");
    check(group.select(8 * 65536, 65536, {}).texture.mips[0].pixels[0] == 10,
          "width threshold inclusive boundary");
    check(group.select(100 * 65536, 65536, {}).texture.mips[0].pixels[0] == 20, "last group variant");
    auto height = b;
    height[20] |= 0x20;
    group = rr::read_cel_group(height, 0);
    check(group.select(100 * 65536, 4 * 65536, {}).texture.mips[0].pixels[0] == 10,
          "height-directed group selection");
    auto logarithmic = b;
    logarithmic[20] |= 0x40;
    for (unsigned i = 0; i < 3; ++i) {
        logarithmic[40 + 2 * i] = uint8_t(i + 2);
        logarithmic[41 + 2 * i] = uint8_t(i + 1);
    }
    group = rr::read_cel_group(logarithmic, 0);
    Bytes logs(256, 0);
    logs[8] = 3;
    check(group.select(8 * 65536, 65536, logs).width == 8, "imported logarithm selection");
    check(group.select(256 * 65536, 65536, {}).width == 16, "large logarithmic size uses level eight");
    auto unused = logarithmic;
    unused[40] = 255;
    unused[20] |= 0x20;
    check(rr::read_cel_group(unused, 0).cells[0].width == 0, "unused group axis sentinel");
    try {
        group.select(8 * 65536, 65536, {});
        check(false, "missing lookup accepted");
    } catch (const std::runtime_error&) {
    }
    auto grid = b;
    grid.resize(232);
    grid[20] = 0x80;
    grid[24] = 0;
    grid[26] = 2;
    grid[27] = 1;
    word(grid, 28, 32);
    word(grid, 32, 152);
    group = rr::read_cel_group(grid, 0);
    check(group.select(0, 0, {}).width == 4 && group.select(0, 0, {}).height == 2, "fixed group selection");
    for (size_t n = 0; n < b.size(); ++n) reject_group(Bytes(b.begin(), b.begin() + long(n)));
    auto bad = b;
    word(bad, 28, 0xffffffff);
    reject_group(bad);
    bad = b;
    bad[20] = 0x83;
    reject_group(bad);
    bad = b;
    bad[21] = 0;
    reject_group(bad);
    bad = b;
    bad[22] = 0;
    reject_group(bad);
    bad = logarithmic;
    bad[40] = 32;
    reject_group(bad);
    // root -> category 1 -> selector 0 -> frame 0 -> CLGP
    Bytes tree(36);
    word(tree, 0, 2);
    word(tree, 4, 12);
    word(tree, 8, 20);
    word(tree, 20, 1);
    word(tree, 24, 8);
    word(tree, 28, 1);
    word(tree, 32, 8);
    tree.insert(tree.end(), b.begin(), b.end());
    const auto family = rr::read_family(packed(tree));
    check(family.resolve(1, 0, 0) == 36 && family.groups.count(36), "bounded family selector resolution");
    check(!family.resolve(0, 0) && !family.resolve(1, 1) && !family.resolve(1, 0, 1),
          "missing selectors return empty");
    for (size_t n = 0; n < tree.size(); ++n) {
        try {
            rr::read_family(packed(Bytes(tree.begin(), tree.begin() + long(n))));
            check(false, "truncated family accepted");
        } catch (const std::runtime_error&) {
        }
    }
    bad = tree;
    word(bad, 8, 0);
    try {
        rr::read_family(packed(bad));
        check(false, "cyclic family accepted");
    } catch (const std::runtime_error&) {
    }
    bad = tree;
    word(bad, 8, 0xffffffff);
    try {
        rr::read_family(packed(bad));
        check(false, "overflowing family accepted");
    } catch (const std::runtime_error&) {
    }
    rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
    if (assets.exists("DATA/FAMILIES.RSC")) {
        const auto file = assets.read("DATA/FAMILIES.RSC");
        size_t families = 0, groups = 0, cells = 0;
        std::map<uint32_t, rr::Family> decoded;
        for (const auto& r : rr::read_resources(file)) {
            if (r.size <= 24) continue; // empty records carry no hierarchy
            try {
                auto f = rr::read_family(Bytes(file.begin() + r.offset, file.begin() + r.offset + r.size));
                ++families;
                groups += f.groups.size();
                for (const auto& g : f.groups) cells += g.second.cells.size();
                decoded.emplace(r.id, std::move(f));
            } catch (const std::exception& e) {
                ++failures;
                std::cerr << "FAM " << r.id << ": " << e.what() << '\n';
            }
        }
        std::cout << families << " families, " << groups << " groups, " << cells << " indexed variants\n";
        const std::string path = rr::golden::oracle_dir() + "/napa-groups.tsv";
        std::ifstream probe(path);
        if (probe) {
            const auto capture = rr::golden::load(path);
            const auto logarithms = rr::read_exe_data(assets.read("ROADRASH.EXE"), 0x00476af8, 256);
            const std::array<std::array<int32_t, 2>, 8> sizes = {{{0, 0},
                                                                  {65536, 65536},
                                                                  {7 * 65536, 4 * 65536},
                                                                  {16 * 65536, 8 * 65536},
                                                                  {32 * 65536, 16 * 65536},
                                                                  {64 * 65536, 32 * 65536},
                                                                  {128 * 65536, 64 * 65536},
                                                                  {256 * 65536, 512 * 65536}}};
            auto hash = [](const Bytes& p) {
                uint32_t h = 2166136261u;
                for (auto v : p) {
                    h ^= v;
                    h *= 16777619u;
                }
                return h;
            };
            const auto& fields = capture.frames.at(0);
            const int count = int(fields.at("group.count"));
            check(count > 0, "oracle captured cel groups");
            for (int i = 0; i < count; ++i) {
                const std::string key = "group." + std::to_string(i) + ".";
                const auto& f = decoded.at(uint32_t(fields.at(key + "family")));
                const auto& g = f.groups.at(uint32_t(fields.at(key + "offset")));
                for (size_t s = 0; s < sizes.size(); ++s) {
                    const auto& c = g.select(sizes[s][0], sizes[s][1], logarithms);
                    const std::string k = key + "size." + std::to_string(s) + ".";
                    check(c.width == fields.at(k + "width") && c.height == fields.at(k + "height"),
                          "original group selected dimensions");
                    check(hash(c.texture.mips[0].pixels) == fields.at(k + "hash") &&
                              int64_t(c.texture.mips[0].pixels.size()) == fields.at(k + "bytes"),
                          "original group selected indexed pixels");
                }
            }
            std::cout << count * 8 << " original cel-group selections compared\n";
        }
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
