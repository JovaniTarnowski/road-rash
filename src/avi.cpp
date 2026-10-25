#include "avi.hpp"
#include <algorithm>
#include <functional>
#include <stdexcept>
namespace rr {
AviFile read_avi(const std::vector<uint8_t>& b) {
    auto range = [&](size_t p, size_t n) {
        if (p > b.size() || n > b.size() - p) throw std::runtime_error("Truncated AVI header");
    };
    auto u16 = [&](size_t p) {
        range(p, 2);
        return unsigned(b[p]) | unsigned(b[p + 1]) << 8;
    };
    auto u32 = [&](size_t p) {
        range(p, 4);
        return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16;
    };
    if (b.size() < 12 || u32(0) != fourcc('R', 'I', 'F', 'F') || u32(8) != fourcc('A', 'V', 'I', ' '))
        throw std::runtime_error("Not RIFF AVI");
    AviFile out;
    auto stream = [&](size_t start, size_t end) {
        AviStreamInfo info;
        size_t format = 0, format_size = 0;
        bool header = false;
        for (size_t p = start; p < end;) {
            if (end - p < 8) throw std::runtime_error("Truncated AVI stream chunk");
            const auto id = u32(p);
            const size_t n = u32(p + 4), data = p + 8;
            if (n > end - data || n + (n & 1) > end - data)
                throw std::runtime_error("Invalid AVI stream chunk extent");
            if (id == fourcc('s', 't', 'r', 'h')) {
                if (header || n < 48) throw std::runtime_error("Invalid AVI stream header");
                header = true;
                info.type = u32(data);
                info.fourcc = u32(data + 4);
                info.scale = u32(data + 20);
                info.rate = u32(data + 24);
            } else if (id == fourcc('s', 't', 'r', 'f')) {
                if (format) throw std::runtime_error("Duplicate AVI stream format");
                format = data;
                format_size = n;
            }
            p = data + n + (n & 1);
        }
        if (!header || !format || !info.scale || !info.rate)
            throw std::runtime_error("Incomplete AVI stream");
        if (info.type == fourcc('v', 'i', 'd', 's')) {
            if (format_size < 40 || u32(format) < 40 || u32(format) > format_size)
                throw std::runtime_error("Invalid AVI bitmap format");
            const uint32_t w = u32(format + 4), h = u32(format + 8);
            if (!w || w > 8192 || !h || h > 8192) throw std::runtime_error("Unsupported AVI dimensions");
            info.width = int(w);
            info.height = int(h);
            if (u16(format + 14) == 8) {
                uint32_t colours = u32(format + 32);
                if (!colours) colours = 256;
                const size_t at = format + u32(format);
                if (colours > 256 || size_t(colours) * 4 > format + format_size - at)
                    throw std::runtime_error("Invalid AVI palette");
                out.palette.assign(b.begin() + at, b.begin() + at + colours * 4);
            }
        } else if (info.type == fourcc('a', 'u', 'd', 's')) {
            if (format_size < 16) throw std::runtime_error("Invalid AVI audio format");
            info.audio_format = int(u16(format));
            info.audio_channels = int(u16(format + 2));
            const uint32_t rate = u32(format + 4);
            if (rate < 1 || rate > 192000 || info.audio_channels < 1 || info.audio_channels > 2)
                throw std::runtime_error("Unsupported AVI audio layout");
            info.audio_rate = int(rate);
            info.block_align = int(u16(format + 12));
            info.audio_bits = int(u16(format + 14));
            if (!info.block_align) throw std::runtime_error("Invalid AVI audio block size");
            if (info.audio_format == 17) {
                if (format_size < 20 || u16(format + 16) < 2 || size_t(u16(format + 16)) + 18 > format_size ||
                    info.audio_bits != 4)
                    throw std::runtime_error("Invalid AVI IMA format");
                info.samples_per_block = int(u16(format + 18));
                if (info.block_align < 4 * info.audio_channels ||
                    (info.block_align - 4 * info.audio_channels) % (4 * info.audio_channels) ||
                    info.samples_per_block !=
                        1 + 2 * (info.block_align - 4 * info.audio_channels) / info.audio_channels)
                    throw std::runtime_error("Invalid AVI IMA samples per block");
            }
        }
        if (out.streams.size() >= 100) throw std::runtime_error("Too many AVI streams");
        out.streams.push_back(info);
    };
    std::function<void(size_t, size_t, bool, int)> scan;
    scan = [&](size_t start, size_t end, bool movie, int depth) {
        if (depth > 16) throw std::runtime_error("AVI list nesting limit");
        for (size_t p = start; p < end;) {
            if (end - p < 8) {
                out.truncated = true;
                break;
            }
            const uint32_t id = u32(p);
            const size_t n = u32(p + 4), data = p + 8;
            const bool incomplete = n > end - data;
            const size_t stop = incomplete ? end : data + n;
            if (id == fourcc('L', 'I', 'S', 'T')) {
                if (stop - data < 4) {
                    if (incomplete) {
                        out.truncated = true;
                        break;
                    }
                    throw std::runtime_error("Invalid AVI list");
                }
                const auto kind = u32(data);
                if (kind == fourcc('s', 't', 'r', 'l')) {
                    if (incomplete) throw std::runtime_error("Truncated AVI stream description");
                    stream(data + 4, stop);
                } else
                    scan(data + 4, stop, movie || kind == fourcc('m', 'o', 'v', 'i'), depth + 1);
            } else if (movie && !incomplete) {
                const unsigned a = id & 255, c = (id >> 8) & 255;
                const uint32_t suffix = id >> 16;
                if (a >= '0' && a <= '9' && c >= '0' && c <= '9' &&
                    (suffix == (fourcc('d', 'c', 0, 0) & 65535) ||
                     suffix == (fourcc('d', 'b', 0, 0) & 65535) ||
                     suffix == (fourcc('w', 'b', 0, 0) & 65535))) {
                    const int index = int((a - '0') * 10 + c - '0');
                    if (size_t(index) >= out.streams.size())
                        throw std::runtime_error("AVI packet refers to missing stream");
                    out.chunks.push_back({index, data, n, suffix == (fourcc('d', 'b', 0, 0) & 65535)});
                }
            }
            if (incomplete || n + (n & 1) > end - data) {
                out.truncated = true;
                break;
            }
            p = data + n + (n & 1);
        }
    };
    const size_t extent = u32(4);
    if (extent < 4) throw std::runtime_error("Invalid AVI RIFF size");
    out.truncated = extent > b.size() - 8;
    scan(12, std::min(b.size(), 8 + extent), false, 0);
    if (out.streams.empty()) throw std::runtime_error("AVI has no streams");
    return out;
}
} // namespace rr
