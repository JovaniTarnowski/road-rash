#include "rider_sprites.hpp"

namespace rr {
RiderSprites load_rider_sprites(const Assets& assets, const std::vector<uint8_t>& exe, bool high) {
    const std::string p = std::string("DATA/BIKERS/") + (high ? "HI" : "LO");
    RiderSprites s;
    s.bob = read_bobs(assets.read(p + "BOB.DAT"), bob_frames);
    if (assets.exists(p + "COP.DAT"))
        s.cop = read_bobs(assets.read(p + "COP.DAT"), cop_frames);
    else if (assets.exists("DATA/BIKERS/LOCOP.DAT"))
        s.cop = read_bobs(assets.read("DATA/BIKERS/LOCOP.DAT"), cop_frames);
    s.shadow = read_bobs(assets.read(p + "SHADOW.DAT"), bob_frames);
    s.shadoc = read_bobs(assets.read(p + "SHADOC.DAT"), cop_frames);
    s.remaps = read_rider_remaps(exe, 15);
    return s;
}

std::optional<CelTexture> rider_texture(const RiderSprites& s, int32_t colour, int32_t frame) {
    // 0x00410080 validity rule (riders = 0x004642d4 - 1 = 15).
    if (colour < 0 || frame < 0) return std::nullopt;
    if ((colour == 0xde || colour == 0xdf) && frame >= 0x2d) return std::nullopt;
    if ((colour < 15 || colour == 0xe0) && frame >= 0x146) return std::nullopt;
    const std::vector<Image8>* sheet = nullptr;
    if (colour < 15)
        sheet = &s.bob;
    else if (colour == 0xde)
        sheet = &s.cop;
    else if (colour == 0xdf)
        sheet = &s.shadoc;
    else if (colour == 0xe0)
        sheet = &s.shadow;
    if (!sheet || size_t(frame) >= sheet->size()) return std::nullopt;
    const Image8& f = (*sheet)[size_t(frame)];
    if (!f.width || !f.height) return std::nullopt;
    CelTexture t;
    t.bounds = {0, 0, f.width, f.height};
    t.flags = 2; // transparent index 0
    CelMip mip;
    mip.width = 256;
    mip.height = std::max(1, f.height);
    mip.stride_shift = 8;
    mip.pixels.assign(size_t(256) * size_t(mip.height), 0);
    for (int y = 0; y < f.height; ++y)
        for (int x = 0; x < f.width; ++x) {
            const uint8_t v = f.pixels[size_t(y) * size_t(f.width) + size_t(x)];
            mip.pixels[size_t(y) * 256 + size_t(x)] = colour < 15 ? s.remaps.tables.at(size_t(colour))[v] : v;
        }
    t.mips.push_back(std::move(mip));
    return t;
}
} // namespace rr
