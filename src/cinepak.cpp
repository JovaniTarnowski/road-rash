#include "cinepak.hpp"
#include <algorithm>
#include <stdexcept>
namespace rr {
namespace {
struct Cursor {
    const uint8_t* data;
    size_t size, at = 0;
    unsigned byte() {
        if (at >= size) throw std::runtime_error("Truncated Cinepak packet");
        return data[at++];
    }
    unsigned be16() {
        const unsigned high = byte();
        return (high << 8) | byte();
    }
    unsigned be24() {
        const unsigned high = byte();
        return (high << 16) | be16();
    }
    uint32_t be32() {
        const unsigned high = be16();
        return uint32_t(high) << 16 | be16();
    }
    Cursor take(size_t n) {
        if (n > size - at) throw std::runtime_error("Invalid Cinepak chunk extent");
        Cursor result{data + at, n};
        at += n;
        return result;
    }
};
struct Flags {
    Cursor& cursor;
    uint32_t word = 0;
    unsigned remaining = 0;
    bool bit() {
        if (!remaining) {
            word = cursor.be32();
            remaining = 32;
        }
        return ((word >> --remaining) & 1) != 0;
    }
};
} // namespace
CinepakDecoder::CinepakDecoder(int w, int h) : width_(w), height_(h) {
    if (w < 1 || w > 4096 || h < 1 || h > 4096 || w % 4)
        throw std::runtime_error("Unsupported Cinepak dimensions");
    books_.resize(32);
    pixels_.resize(size_t(w) * h * 3);
}
void CinepakDecoder::decode(const uint8_t* bytes, size_t size) {
    if (!bytes || size < 10) throw std::runtime_error("Invalid Cinepak frame");
    Cursor input{bytes, size};
    const unsigned flags = input.byte(), extent = input.be24();
    if (extent < 10 || extent > size) throw std::runtime_error("Invalid Cinepak frame extent");
    input.size = extent;
    const unsigned w = input.be16(), h = input.be16(), strips = input.be16();
    if (w != unsigned(width_) || h != unsigned(height_) || !strips || strips > books_.size())
        throw std::runtime_error("Invalid Cinepak frame dimensions or strips");
    // Keep the previous reference frame and codebooks intact after bad input.
    auto books = books_;
    auto pixels = pixels_;
    unsigned previous_bottom = 0;
    for (unsigned s = 0; s < strips; ++s) {
        const unsigned id = input.byte(), length = input.be24();
        if ((id != 0x10 && id != 0x11) || length < 12) throw std::runtime_error("Invalid Cinepak strip");
        unsigned top = input.be16();
        const unsigned left = input.be16();
        unsigned bottom = input.be16();
        const unsigned right = input.be16();
        if (!top) {
            top = previous_bottom;
            bottom += top;
        }
        if (left >= right || top >= bottom || right > unsigned(width_) || bottom > unsigned(height_) ||
            left % 4 || right % 4)
            throw std::runtime_error("Invalid Cinepak strip rectangle");
        if (s && !(flags & 1)) books[s] = books[s - 1];
        Cursor strip = input.take(length - 12);
        bool vectors = false;
        while (strip.at < strip.size) {
            const unsigned kind = strip.byte(), chunk_length = strip.be24();
            if (chunk_length < 4) throw std::runtime_error("Invalid Cinepak chunk length");
            Cursor chunk = strip.take(chunk_length - 4);
            if (kind >= 0x20 && kind <= 0x27) {
                auto& book = (kind & 2) ? books[s].v1 : books[s].v4;
                Flags updates{chunk};
                for (size_t i = 0;
                     i < book.size() && (chunk.at < chunk.size || ((kind & 1) && updates.remaining)); ++i) {
                    if ((kind & 1) && !updates.bit()) continue;
                    std::array<int, 4> y{};
                    for (auto& value : y) value = int(chunk.byte());
                    int u = 0, v = 0;
                    if (!(kind & 4)) {
                        const unsigned ub = chunk.byte(), vb = chunk.byte();
                        u = ub < 128 ? int(ub) : int(ub) - 256;
                        v = vb < 128 ? int(vb) : int(vb) - 256;
                    }
                    for (size_t n = 0; n < 4; ++n) {
                        book[i][3 * n] = uint8_t(std::clamp(y[n] + 2 * v, 0, 255));
                        book[i][3 * n + 1] = uint8_t(std::clamp(y[n] - u / 2 - v, 0, 255));
                        book[i][3 * n + 2] = uint8_t(std::clamp(y[n] + 2 * u, 0, 255));
                    }
                }
                if (chunk.at != chunk.size) throw std::runtime_error("Excess Cinepak codebook data");
            } else if (kind >= 0x30 && kind <= 0x32) {
                if (vectors) throw std::runtime_error("Duplicate Cinepak vector chunk");
                vectors = true;
                Flags coding{chunk};
                for (unsigned y = top; y < bottom; y += 4)
                    for (unsigned x = left; x < right; x += 4) {
                        if (kind == 0x31 && !coding.bit()) continue;
                        const bool four = kind != 0x32 && coding.bit();
                        std::array<unsigned, 4> indices{};
                        if (four)
                            for (auto& i : indices) i = chunk.byte();
                        else
                            indices.fill(chunk.byte());
                        for (unsigned dy = 0; dy < 4 && y + dy < unsigned(height_); ++dy)
                            for (unsigned dx = 0; dx < 4; ++dx) {
                                const unsigned quadrant = (dy / 2) * 2 + dx / 2;
                                const auto& code =
                                    four ? books[s].v4[indices[quadrant]] : books[s].v1[indices[0]];
                                const unsigned pixel = four ? (dy % 2) * 2 + dx % 2 : quadrant;
                                const size_t at = (size_t(y + dy) * width_ + x + dx) * 3;
                                std::copy_n(code.data() + pixel * 3, 3, pixels.data() + at);
                            }
                    }
            }
        }
        if (!vectors) throw std::runtime_error("Cinepak strip has no vectors");
        previous_bottom = bottom;
    }
    books_ = std::move(books);
    pixels_ = std::move(pixels);
}
} // namespace rr
