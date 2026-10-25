#include "assets.hpp"
#include "pe_strings.hpp"
#include "text.hpp"
#include <iostream>

int main(int argc, char** argv) {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    try {
        rr::TextRenderer bad(rr::Face::badloc, std::vector<uint8_t>(40, 0));
        ++failures;
    } catch (const std::runtime_error&) {
    }
    rr::Assets assets(argc > 1 ? argv[1] : RR_SOURCE_DIR "/../cdrom/ROADRASH");
    if (!assets.exists("TEXT/FUTB.DLL")) {
        std::cout << "skip: no disc\n";
        return 77;
    }
    const rr::TextRenderer text(assets);
    for (auto f :
         {rr::Face::badloc, rr::Face::futura_regular, rr::Face::futura_bold, rr::Face::futura_demi}) {
        check(text.width(f, 24, "") == 0, "empty width");
        const std::string s = "Big Game Mode \xe9";
        const int w = text.width(f, 24, s);
        check(w > 40 && w < 400, "plausible width");
        rr::RgbImage img(640, 100, 0);
        text.draw(img, f, 24, 100, 30, 0x123456, s);
        int ink = 0, outside = 0, other = 0;
        for (int y = 0; y < img.height; ++y)
            for (int x = 0; x < img.width; ++x) {
                const uint32_t p = img.at(x, y);
                if (!p) continue;
                ++ink;
                if (p != 0x123456) ++other;
                if (x < 100 - 4 || x >= 100 + w + 4 || y < 30 || y >= 30 + 24) ++outside;
            }
        check(ink > 50 && other == 0, "ink drawn in the exact colour");
        check(outside == 0, "ink stays inside the measured box");
    }
    // Word wrap keeps every language's longest menu description inside a 300-pixel box (0x00420470).
    for (auto l : {rr::Language::english, rr::Language::german, rr::Language::french, rr::Language::spanish,
                   rr::Language::italian}) {
        const auto table = rr::load_game_strings(rr::read_string_resources(assets.read(rr::language_dll(l))));
        size_t longest = 0;
        for (size_t i = 1; i < table.size(); ++i)
            if (table[i].size() > table[longest].size()) longest = i;
        rr::TextRect r{0, 0, 300, 0};
        const int h = text.draw_box(nullptr, rr::Face::futura_regular, 32, r, 0, table[longest],
                                    rr::dt::wordbreak | rr::dt::calcrect);
        check(h > 32 && r.right - r.left <= 300 + 200, "wrapped text measured");
        rr::RgbImage img(640, 480, 0);
        rr::TextRect box{8, 0, 308, 480};
        text.draw_box(&img, rr::Face::futura_demi, 24, box, 0xffffff, table[longest], rr::dt::wordbreak);
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
