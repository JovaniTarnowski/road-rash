#include "bob.hpp"
#include <stdexcept>

namespace rr {
std::vector<Image8> read_bobs(const std::vector<uint8_t>& file, int frames) {
    std::vector<Image8> out;
    size_t p = 0;
    for (int i = 0; i < frames; ++i) {
        if (file.size() - p < 2 || p > file.size()) throw std::runtime_error("Truncated bob frame header");
        Image8 img{file[p], file[p + 1], {}};
        p += 2;
        const size_t n = size_t(img.width) * size_t(img.height);
        if (file.size() - p < n) throw std::runtime_error("Truncated bob frame pixels");
        img.pixels.assign(file.begin() + long(p), file.begin() + long(p + n));
        p += n;
        out.push_back(std::move(img));
    }
    return out;
}

RiderRemaps read_rider_remaps(const std::vector<uint8_t>& exe, int riders) {
    // .data of the reference executable: VA 0x00463000 at file offset 0x61600, 0x16400 bytes.
    constexpr uint32_t data_va = 0x00463000, data_raw = 0x61600, data_size = 0x16400, table_va = 0x0046e158;
    if (riders < 1 || table_va + uint32_t(riders) * 256 > data_va + data_size) throw std::runtime_error("Remap table outside .data");
    const size_t base = data_raw + (table_va - data_va);
    if (exe.size() < base + size_t(riders) * 256) throw std::runtime_error("Executable too small for remap tables");
    RiderRemaps r;
    for (int k = 0; k < riders; ++k) {
        std::array<uint8_t,256> t{};
        for (int i = 0; i < 256; ++i) t[size_t(i)] = exe[base + size_t(k) * 256 + size_t(i)];
        r.tables.push_back(t);
    }
    return r;
}

Image8 remap(const Image8& src, const RiderRemaps& r, int rider) {
    Image8 out = src;
    const auto& t = r.tables.at(size_t(rider));
    for (auto& p : out.pixels) p = t[p];
    return out;
}
}
