#include "wav.hpp"
#include "ima_adpcm.hpp"
#include <cstring>
#include <stdexcept>
namespace rr {
PcmSound read_wav(const std::vector<uint8_t>& bytes) {
    auto range = [&](size_t at, size_t n) {
        if (at > bytes.size() || n > bytes.size() - at) throw std::runtime_error("Truncated WAV");
    };
    auto u16 = [&](size_t at) {
        range(at, 2);
        return uint16_t(uint16_t(bytes[at]) | uint16_t(bytes[at + 1]) << 8);
    };
    auto u32 = [&](size_t at) { return uint32_t(u16(at)) | uint32_t(u16(at + 2)) << 16; };
    range(0, 12);
    if (std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
        throw std::runtime_error("Not RIFF WAVE");
    const uint32_t extent = u32(4);
    if (extent < 4) throw std::runtime_error("Invalid WAV RIFF extent");
    range(8, extent);
    const size_t end = 8 + size_t(extent);
    size_t format = 0, format_size = 0, data = 0, data_size = 0;
    uint32_t fact_frames = 0;
    bool has_fact = false;
    for (size_t at = 12; at < end;) {
        if (end - at < 8) throw std::runtime_error("Truncated WAV chunk header");
        const size_t size = u32(at + 4);
        if (size > end - at - 8) throw std::runtime_error("WAV chunk exceeds RIFF extent");
        if (!std::memcmp(bytes.data() + at, "fmt ", 4)) {
            if (format) throw std::runtime_error("Duplicate WAV format");
            format = at + 8;
            format_size = size;
        } else if (!std::memcmp(bytes.data() + at, "data", 4)) {
            if (data) throw std::runtime_error("Duplicate WAV data");
            data = at + 8;
            data_size = size;
        } else if (!std::memcmp(bytes.data() + at, "fact", 4)) {
            if (has_fact || size < 4) throw std::runtime_error("Invalid WAV fact chunk");
            has_fact = true;
            fact_frames = u32(at + 8);
        }
        const size_t padded = size + (size & 1);
        if (padded > end - at - 8) throw std::runtime_error("Missing WAV chunk padding");
        at += 8 + padded;
    }
    if (!format || format_size < 16 || !data) throw std::runtime_error("WAV requires fmt and data chunks");
    const int encoding = u16(format), channels = u16(format + 2), bits = u16(format + 14);
    const uint32_t rate = u32(format + 4);
    if (channels < 1 || channels > 2 || rate < 1 || rate > 192000)
        throw std::runtime_error("Invalid WAV channel count or rate");
    if (encoding == 17) {
        if (bits != 4 || format_size < 20 || u16(format + 16) < 2 ||
            size_t(u16(format + 16)) + 18 > format_size)
            throw std::runtime_error("Invalid WAV IMA format extension");
        const int align = u16(format + 12);
        if (align < 4 * channels || (align - 4 * channels) % (4 * channels) ||
            u16(format + 18) != 1 + 2 * (align - 4 * channels) / channels)
            throw std::runtime_error("Invalid WAV IMA samples per block");
        PcmSound result{int(rate), channels,
                        decode_ima_adpcm(bytes.data() + data, data_size, channels, align)};
        if (has_fact) {
            if (fact_frames > result.samples.size() / size_t(channels))
                throw std::runtime_error("WAV fact exceeds decoded audio");
            result.samples.resize(size_t(fact_frames) * channels);
        }
        return result;
    }
    if (encoding != 1 || (bits != 8 && bits != 16)) throw std::runtime_error("Unsupported WAV encoding");
    const size_t align = size_t(channels) * size_t(bits / 8);
    if (u16(format + 12) != align || u32(format + 8) != uint64_t(rate) * align || data_size % align)
        throw std::runtime_error("Invalid WAV alignment or byte rate");
    PcmSound result{int(rate), channels, {}};
    result.samples.reserve(data_size / size_t(bits / 8));
    for (size_t at = data; at < data + data_size; at += size_t(bits / 8)) {
        if (bits == 8)
            result.samples.push_back(int16_t((int(bytes[at]) - 128) * 256));
        else {
            const uint16_t value = u16(at);
            result.samples.push_back(int16_t(value < 32768 ? int(value) : int(value) - 65536));
        }
    }
    return result;
}
} // namespace rr
