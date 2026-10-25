#include "assets.hpp"
#include "pe_strings.hpp"
#include <iostream>
#include <array>
#include <map>
#include <set>

// Minimal PE32 with one .rsrc section holding RT_STRING blocks {block id -> 16 strings}.
static std::vector<uint8_t> make_pe(const std::map<uint32_t, std::array<std::u16string, 16>>& blocks) {
    std::vector<uint8_t> rsrc;
    auto put16 = [](std::vector<uint8_t>& v, size_t p, uint32_t x) {
        if (v.size() < p + 2) v.resize(p + 2);
        v[p] = uint8_t(x);
        v[p + 1] = uint8_t(x >> 8);
    };
    auto put32 = [&](std::vector<uint8_t>& v, size_t p, uint32_t x) {
        put16(v, p, x & 0xffff);
        put16(v, p + 2, x >> 16);
    };
    const uint32_t section_rva = 0x1000;
    const size_t n = blocks.size();
    // Root (1 entry) -> type dir (n entries) -> n language dirs (1 entry each) -> n data entries -> data.
    const size_t type_dir = 24, lang_dirs = type_dir + 16 + 8 * n, data_entries = lang_dirs + 24 * n,
                 data = data_entries + 16 * n;
    rsrc.resize(data);
    put16(rsrc, 14, 1);
    put32(rsrc, 16, 6);
    put32(rsrc, 20, 0x80000000u | uint32_t(type_dir));
    put16(rsrc, type_dir + 14, uint32_t(n));
    size_t i = 0, pos = data;
    for (const auto& [id, strings] : blocks) {
        put32(rsrc, type_dir + 16 + 8 * i, id);
        put32(rsrc, type_dir + 20 + 8 * i, 0x80000000u | uint32_t(lang_dirs + 24 * i));
        put16(rsrc, lang_dirs + 24 * i + 14, 1);
        put32(rsrc, lang_dirs + 24 * i + 16, 0x409);
        put32(rsrc, lang_dirs + 24 * i + 20, uint32_t(data_entries + 16 * i));
        const size_t start = pos;
        for (const auto& s : strings) {
            put16(rsrc, pos, uint32_t(s.size()));
            pos += 2;
            for (char16_t c : s) {
                put16(rsrc, pos, c);
                pos += 2;
            }
        }
        put32(rsrc, data_entries + 16 * i, section_rva + uint32_t(start));
        put32(rsrc, data_entries + 16 * i + 4, uint32_t(pos - start));
        ++i;
    }
    std::vector<uint8_t> pe(0x200);
    pe[0] = 'M';
    pe[1] = 'Z';
    put32(pe, 60, 0x40);
    put32(pe, 0x40, 0x4550);
    put16(pe, 0x44, 0x14c);
    put16(pe, 0x46, 1);
    put16(pe, 0x54, 0xe0);
    const size_t opt = 0x58;
    put16(pe, opt, 0x10b);
    put32(pe, opt + 92, 16);
    put32(pe, opt + 96 + 16, section_rva);
    put32(pe, opt + 96 + 20, uint32_t(rsrc.size()));
    const size_t sec = opt + 0xe0;
    put32(pe, sec + 8, uint32_t(rsrc.size()));
    put32(pe, sec + 12, section_rva);
    put32(pe, sec + 16, uint32_t(rsrc.size()));
    put32(pe, sec + 20, 0x200);
    pe.insert(pe.end(), rsrc.begin(), rsrc.end());
    return pe;
}

int main(int argc, char** argv) {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    std::array<std::u16string, 16> b1{};
    b1[0] = u"A";
    b1[2] = u"Hé";
    const auto pe = make_pe({{1, b1}});
    const auto t = rr::read_string_table(pe);
    check(t.size() == 2 && t.at(0) == "A" && t.at(2) == "H\xC3\xA9", "synthetic table");
    for (size_t len = 0; len < pe.size(); ++len) {
        try {
            rr::read_string_resources({pe.begin(), pe.begin() + long(len)});
            ++failures;
            std::cerr << "accepted truncation " << len << "\n";
        } catch (const std::runtime_error&) {
        }
    }
    auto bad = pe;
    bad[0x40] = 0;
    try {
        rr::read_string_resources(bad);
        ++failures;
    } catch (const std::runtime_error&) {
    }
    check(rr::to_cp1252(u"€éx一") == "\x80\xe9x?", "cp1252 conversion");
    check(rr::cp1252_to_utf8("\x80") == "\xe2\x82\xac", "cp1252 to utf8");

    // 0x0042f5d0 table assembly: header "%d %d", descending load, 0xff continuation from 3000+index.
    std::map<uint32_t, std::u16string> res{
        {0, u"64 3"}, {1, u"one"}, {2, u"ab\u0002ÿ"}, {3, u"x"}, {3002, u"cd"}};
    const auto g = rr::load_game_strings(res);
    check(g.size() == 4 && g[1] == "one" && g[2] == "abcd" && g[3] == "x", "string continuation");
    std::map<uint32_t, std::u16string> tight{{0, u"6 2"}, {1, u"abcdef"}, {2, u"zz"}};
    const auto tt = rr::load_game_strings(tight); // LoadStringA truncates to the remaining buffer
    check(tt[2] == "zz" && tt[1] == "ab", "buffer truncation");
    try {
        rr::load_game_strings({{1, u"x"}});
        ++failures;
    } catch (const std::runtime_error&) {
    }
    try {
        rr::load_game_strings({{0, u"64 1"}, {1, u"a\u0005ÿ"}});
        ++failures;
    } catch (const std::runtime_error&) {
    }
    check(rr::language_from_code("de") == rr::Language::german &&
              rr::language_from_code("pt") == rr::Language::english,
          "language rule");

    if (argc > 1 && std::string(argv[1]) == "--corpus") {
        const std::string root = argc > 2 ? argv[2] : RR_SOURCE_DIR "/../cdrom/ROADRASH";
        rr::Assets assets(root);
        if (!assets.exists("TEXT/ENU.DLL")) {
            std::cout << "skip: no disc\n";
            return 77;
        }
        std::set<uint32_t> english_ids;
        size_t english_count = 0;
        for (auto l : {rr::Language::english, rr::Language::german, rr::Language::french,
                       rr::Language::spanish, rr::Language::italian}) {
            const auto res2 = rr::read_string_resources(assets.read(rr::language_dll(l)));
            const auto table = rr::load_game_strings(res2);
            std::set<uint32_t> ids;
            for (auto& [id, s] : res2) ids.insert(id);
            size_t used = 0;
            for (auto& s : table) used += !s.empty();
            std::cout << rr::language_dll(l) << ": " << res2.size() << " resources, " << table.size() - 1
                      << " slots, " << used << " non-empty\n";
            if (l == rr::Language::english) {
                english_ids = ids;
                english_count = table.size();
                check(table.size() > 300, "english table size");
            } else
                check(table.size() == english_count, "same slot count as English");
        }
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
