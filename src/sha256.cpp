// SHA-256 as specified in FIPS 180-4. This implementation has no dependencies.
#include "sha256.hpp"
#include <array>
#include <limits>
#include <stdexcept>
namespace rr {
namespace {
constexpr uint32_t k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
uint32_t rotate(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
void compress_block(std::array<uint32_t, 8>& h, const uint8_t block[64]) {
    uint32_t w[64]{};
    for (size_t i = 0; i < 16; ++i)
        w[i] = uint32_t(block[4 * i]) << 24 | uint32_t(block[4 * i + 1]) << 16 |
               uint32_t(block[4 * i + 2]) << 8 | uint32_t(block[4 * i + 3]);
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = rotate(w[i - 15], 7) ^ rotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotate(w[i - 2], 17) ^ rotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto v = h;
    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = rotate(v[4], 6) ^ rotate(v[4], 11) ^ rotate(v[4], 25);
        const uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
        const uint32_t t1 = v[7] + s1 + ch + k[i] + w[i];
        const uint32_t s0 = rotate(v[0], 2) ^ rotate(v[0], 13) ^ rotate(v[0], 22);
        const uint32_t t2 = s0 + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
        v = {t1 + t2, v[0], v[1], v[2], v[3] + t1, v[4], v[5], v[6]};
    }
    for (size_t i = 0; i < 8; ++i) h[i] += v[i];
}
std::string hex(const std::array<uint32_t, 8>& h) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (const auto word : h)
        for (int shift = 28; shift >= 0; shift -= 4) out += digits[(word >> shift) & 15];
    return out;
}
}  // namespace

Sha256Stream::Sha256Stream()
    : h_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256Stream::compress(const uint8_t block[64]) { compress_block(h_, block); }

void Sha256Stream::write(const uint8_t* data, size_t size) {
    if (finished_) throw std::runtime_error("SHA-256 write after finish");
    if (size > std::numeric_limits<uint64_t>::max() - total_) throw std::runtime_error("SHA-256 input too large");
    total_ += size;
    while (size) {
        const size_t n = std::min(size, 64 - buffered_);
        for (size_t i = 0; i < n; ++i) buf_[buffered_ + i] = data[i];
        buffered_ += n;
        data += n;
        size -= n;
        if (buffered_ == 64) {
            compress(buf_);
            buffered_ = 0;
        }
    }
}

std::string Sha256Stream::finish() {
    if (finished_) throw std::runtime_error("SHA-256 finished twice");
    const uint64_t bits = total_ * 8;
    uint8_t pad = 0x80;
    write(&pad, 1);
    pad = 0;
    while (buffered_ != 56) write(&pad, 1);
    uint8_t len[8]{};
    for (int i = 0; i < 8; ++i) len[i] = uint8_t(bits >> ((7 - i) * 8));
    // The length block always fits: buffered_ == 56 leaves exactly 8 bytes.
    for (int i = 0; i < 8; ++i) buf_[buffered_ + i] = len[i];
    compress(buf_);
    buffered_ = 0;
    finished_ = true;
    return hex(h_);
}

std::string sha256(const std::vector<uint8_t>& bytes) {
    if (bytes.size() > std::numeric_limits<uint64_t>::max() / 8 || bytes.size() > SIZE_MAX - 72)
        throw std::runtime_error("SHA-256 input too large");
    Sha256Stream stream;
    if (!bytes.empty()) stream.write(bytes.data(), bytes.size());
    return stream.finish();
}
}  // namespace rr
