#include "bob.hpp"
#include "assets.hpp"
#include "rider_sprites.hpp"
#include <filesystem>
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << '\n';
        }
    };
    // Two frames: 2x1 then an empty 0x0 frame (0x004436a0 stores a null pointer for it).
    const std::vector<uint8_t> file = {2, 1, 7, 9, 0, 0};
    auto frames = rr::read_bobs(file, 2);
    check(frames.size() == 2 && frames[0].width == 2 && frames[0].height == 1, "frame sizes");
    check(frames[0].pixels == std::vector<uint8_t>({7, 9}), "frame pixels");
    check(frames[1].width == 0 && frames[1].pixels.empty(), "empty frame");
    rr::RiderSprites sprites;
    sprites.shadow = frames;
    check(!rr::rider_texture(sprites, 0xe0, 1), "empty shadow does not produce invalid texture bounds");
    for (size_t n = 0; n < file.size() - 2; ++n) {
        try {
            rr::read_bobs(std::vector<uint8_t>(file.begin(), file.begin() + long(n)), 2);
            check(false, "truncated accepted");
        } catch (const std::runtime_error&) {
        }
    }
    try {
        rr::read_bobs(file, 3);
        check(false, "too many frames accepted");
    } catch (const std::runtime_error&) {
    }

    // Remap applies a per-rider 256-entry table (0x00443820).
    rr::RiderRemaps remaps;
    remaps.tables.assign(2, {});
    for (int i = 0; i < 256; ++i) {
        remaps.tables[0][size_t(i)] = uint8_t(i);
        remaps.tables[1][size_t(i)] = uint8_t(255 - i);
    }
    auto r = rr::remap(frames[0], remaps, 1);
    check(r.pixels == std::vector<uint8_t>({248, 246}), "rider remap");

    // Corpus (skips without the disc): frame counts and exact file consumption from 0x00443610.
    const std::string game = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (std::filesystem::exists(game + "/DATA/BIKERS/HIBOB.DAT")) {
        rr::Assets a(game);
        auto hi = rr::read_bobs(a.read("DATA/BIKERS/HIBOB.DAT"), rr::bob_frames);
        check(hi.size() == 326 && hi[3].width == 56 && hi[3].height == 128, "HIBOB straight frame");
        check(rr::read_bobs(a.read("DATA/BIKERS/LOCOP.DAT"), rr::cop_frames).size() == 45, "LOCOP");
        auto t = rr::read_rider_remaps(a.read("ROADRASH.EXE"), 9);
        check(t.tables.size() == 9 && t.tables[0][2] == 80, "remap table from executable");
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
