#include "cel.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace rr {
namespace {
struct Reader {
    const std::vector<uint8_t>& b;
    void range(size_t p, size_t n) const {
        if (p > b.size() || n > b.size() - p) throw std::runtime_error("Truncated cel payload");
    }
    uint32_t word(size_t p) const {
        range(p, 4);
        return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24;
    }
};
int dimension(uint32_t encoded) {
    if (encoded & 0x80000000u) {
        unsigned shift = encoded & 31;
        if (shift > 9) throw std::runtime_error("Cel dimension exponent too large");
        return int(1u << shift);
    }
    const int n = int(encoded & 511);
    if (!n) throw std::runtime_error("Zero cel dimension");
    return n;
}
} // namespace
Plut read_plut(const std::vector<uint8_t>& b) {
    Reader r{b};
    r.range(0, 12);
    if (r.word(0) != 0x504c5554 || r.word(4) != b.size()) throw std::runtime_error("Invalid PLUT chunk");
    const auto n = r.word(8);
    if (!n || n > 256) throw std::runtime_error("Invalid PLUT count");
    r.range(12, size_t(n) * 2);
    Plut p;
    for (uint32_t i = 0; i < n; ++i)
        p.entries.push_back(uint16_t(b[12 + 2 * i] | uint16_t(b[13 + 2 * i]) << 8));
    return p;
}
CelTexture read_indexed_texture(const uint8_t* data, size_t size) {
    const std::vector<uint8_t> b(data, data + size);
    Reader r{b};
    const size_t body = 0;
    if (size < 4 || r.word(0) != 0x00bc614e) throw std::runtime_error("Not an indexed texture");
    if (size < 56) throw std::runtime_error("Truncated indexed texture header");
    const auto width = r.word(body + 32), height = r.word(body + 36), levels = r.word(body + 40);
    if (!width || width > 256 || (width & (width - 1)) || !height || height > 512 || !levels || levels > 9)
        throw std::runtime_error("Invalid indexed texture dimensions");
    unsigned shift = 0;
    while ((1u << shift) < width) ++shift;
    if (levels > shift + 1) throw std::runtime_error("Too many texture mip levels");
    CelTexture texture;
    for (size_t i = 0; i < 4; ++i) {
        const uint16_t bits = uint16_t(b[body + 4 + 2 * i] | uint16_t(b[body + 5 + 2 * i]) << 8);
        int16_t value;
        std::memcpy(&value, &bits, 2);
        texture.bounds[i] = value;
    }
    texture.flags = r.word(body + 12);
    texture.flat_index = uint8_t(r.word(body + 44));
    for (size_t i = 0; i < 16 && b[body + 16 + i]; ++i) texture.name.push_back(char(b[body + 16 + i]));
    for (uint32_t level = 0; level < levels; ++level) {
        const size_t entry = 56 + 12 * level;
        if (entry > size || 4 > size - entry) throw std::runtime_error("Truncated mip pointer table");
        const uint64_t start = uint64_t(r.word(body + entry)) + 28;
        // Non-power-of-two scenery images retain their final partial row at
        // every mip. Flooring the height discards a row that the original UV
        // bounds can sample (for example GRVP2SM2's 18 -> 9 -> 5 -> 3 -> 2).
        const int w = int(width >> level), h = int((height + (1u << level) - 1) >> level);
        const size_t bytes = size_t(w) * h;
        const size_t header_end = 56 + 12 * (levels - 1) + 4;
        if (start < header_end || start > size || bytes > size - start)
            throw std::runtime_error("Mip pixels outside cel chunk");
        CelMip mip;
        mip.width = w;
        mip.height = h;
        mip.stride_shift = shift - level;
        mip.pixels.assign(b.begin() + body + size_t(start), b.begin() + body + size_t(start) + bytes);
        const size_t available = std::min<size_t>(65536, size - size_t(start));
        const size_t storage = available - (available % size_t(w));
        if (size > header_end && bytes < 65536) {
            mip.sampling_storage.assign(65536, 0);
            std::copy_n(b.begin() + size_t(start), storage, mip.sampling_storage.begin());
        }
        texture.mips.push_back(std::move(mip));
    }
    return texture;
}
Cel read_cel(const std::vector<uint8_t>& b) {
    Reader r{b};
    Cel cel;
    bool ccb = false, pdat = false, plut = false;
    for (size_t p = 0; p < b.size();) {
        r.range(p, 8);
        const auto tag = r.word(p), extent = r.word(p + 4);
        if (extent < 8) throw std::runtime_error("Invalid cel chunk size");
        r.range(p, extent);
        if (tag == 0x43434220) {
            if (ccb || extent < 80) throw std::runtime_error("Invalid/duplicate CCB");
            ccb = true;
            cel.flags = r.word(p + 12);
            cel.width = dimension(r.word(p + 72));
            cel.height = dimension(r.word(p + 76));
        } else if (tag == 0x504c5554) {
            if (plut) throw std::runtime_error("Duplicate PLUT");
            plut = true;
            cel.palette = read_plut(std::vector<uint8_t>(b.begin() + p, b.begin() + p + extent));
        } else if (tag == 0x50444154 || tag == 0x52504454 || tag == 0x52525044) {
            if (pdat || !ccb || extent < 12) throw std::runtime_error("Invalid/duplicate cel pixel chunk");
            pdat = true;
            const size_t body = p + 8, size = extent - 8;
            if (r.word(body) != 0x00bc614e) {
                if (size != 4) throw std::runtime_error("Unsupported nonindexed cel pixel data");
                cel.legacy_pixel_word = r.word(body);
            } else {
                cel.texture = read_indexed_texture(b.data() + body, size);
            }
        }
        p += extent;
    }
    if (!ccb || !pdat) throw std::runtime_error("Missing CCB/pixel chunk");
    return cel;
}
} // namespace rr
