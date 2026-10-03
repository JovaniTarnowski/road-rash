#pragma once
#include <cstdint>
#include <vector>

namespace rr {
// PKWARE Data Compression Library "implode" streams, as decompressed by the original's
// explode engine (0x004555e0 via 0x0040e4d0). Throws std::runtime_error on malformed input or
// when the output would differ from expected_size.
std::vector<uint8_t> explode(const uint8_t* data, size_t size, size_t expected_size);
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t size);   // reflected 0xedb88320

// Resource payloads read by 0x004157b0 start with {unpacked size, stored size, crc, compressed}.
// Returns the unpacked bytes, checking the size and (for compressed data) the CRC (0x0040e4d0).
std::vector<uint8_t> unpack_resource(const std::vector<uint8_t>& payload);
}
