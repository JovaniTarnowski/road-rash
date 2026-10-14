#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace rr {
// Little-endian reads over a byte span. Every read checks bounds and throws std::runtime_error,
// never std::out_of_range, so parsers keep the project's exception contract.
class ByteReader {
public:
    explicit ByteReader(std::span<const uint8_t> bytes) : bytes_(bytes) {}
    size_t size() const { return bytes_.size(); }
    std::span<const uint8_t> bytes(size_t offset, size_t count) const {
        if (offset > bytes_.size() || count > bytes_.size() - offset) throw std::runtime_error("Read past end of data");
        return bytes_.subspan(offset, count);
    }
    template <class T> T le(size_t offset) const {
        static_assert(std::is_integral_v<T>);
        const auto b = bytes(offset, sizeof(T));
        std::array<uint8_t, sizeof(T)> raw{};
        std::memcpy(raw.data(), b.data(), sizeof(T));
        if constexpr (std::endian::native == std::endian::big) std::reverse(raw.begin(), raw.end());
        return std::bit_cast<T>(raw);
    }
private:
    std::span<const uint8_t> bytes_;
};
}
