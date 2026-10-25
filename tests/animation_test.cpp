#include "animation.hpp"
#include "assets.hpp"
#include "family.hpp"
#include "resource.hpp"
#include <iostream>
int main() {
    try {
        std::vector<uint8_t> synthetic(220);
        auto word = [&](size_t p, uint32_t v) {
            for (int i = 0; i < 4; ++i) synthetic[p + i] = uint8_t(v >> (8 * i));
        };
        word(0, 0x5252414e);
        word(4, 20);
        word(8, 56);
        word(12, 104);
        word(20, 0x52524644);
        word(24, 36);
        word(28, 116);
        word(32, 184);
        word(36, 196);
        word(40, 204);
        word(44, 7);
        word(48, 3);
        word(52, 4);
        word(56, 0x414e494d);
        word(60, 48);
        word(72, 1);
        word(104, 0x43434220);
        word(184, 0x52525044);
        word(188, 12);
        word(192, 0x17001e00);
        word(196, uint32_t(-2));
        word(200, 3);
        const auto decoded = rr::read_animation(synthetic);
        if (decoded.frames.size() != 1 || decoded.frames[0].hotspot != std::array<int32_t, 2>{-2, 3} ||
            decoded.frames[0].flags != 7 || decoded.frames[0].legacy_pixel_word != 0x17001e00)
            throw std::runtime_error("Synthetic animation fields");
        for (size_t length = 0; length < synthetic.size(); ++length) {
            bool rejected = false;
            try {
                rr::read_animation({synthetic.begin(), synthetic.begin() + length});
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            if (!rejected) throw std::runtime_error("Truncated animation accepted");
        }
        word(32, UINT32_MAX);
        bool rejected = false;
        try {
            rr::read_animation(synthetic);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected) throw std::runtime_error("Invalid pixel offset accepted");
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if (!assets.exists("DATA/FAMILIES.RSC")) {
            std::cout << "Imported animation corpus absent\n";
            return 0;
        }
        auto bytes = assets.read("DATA/FAMILIES.RSC");
        size_t animations = 0, frames = 0, indexed = 0;
        for (const auto& entry : rr::read_resources(bytes)) {
            if (entry.size <= 24) continue;
            const auto family =
                rr::read_family({bytes.begin() + entry.offset, bytes.begin() + entry.offset + entry.size});
            for (const auto& node : family.nodes)
                if (node.second.tag == 0x5252414e) {
                    try {
                        const auto animation = rr::read_animation(family.bytes, node.first);
                        ++animations;
                        frames += animation.frames.size();
                        for (const auto& frame : animation.frames) indexed += bool(frame.texture);
                    } catch (const std::exception& e) {
                        throw std::runtime_error("Family " + std::to_string(entry.id) + " animation " +
                                                 std::to_string(node.first) + ": " + e.what());
                    }
                }
        }
        std::cout << animations << " family animations, " << frames << " frames, " << indexed
                  << " indexed frames\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
