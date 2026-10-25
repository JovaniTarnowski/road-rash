#include "cinepak.hpp"
#include "avi.hpp"
#include "assets.hpp"
#include "video_player.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using Bytes = std::vector<uint8_t>;
void be16(Bytes& b, unsigned v) {
    b.push_back(uint8_t(v >> 8));
    b.push_back(uint8_t(v));
}
void be24(Bytes& b, unsigned v) {
    b.push_back(uint8_t(v >> 16));
    be16(b, v);
}
Bytes chunk(unsigned id, const Bytes& data) {
    Bytes b{uint8_t(id)};
    be24(b, unsigned(data.size() + 4));
    b.insert(b.end(), data.begin(), data.end());
    return b;
}
Bytes frame(const Bytes& data) {
    Bytes b{0};
    be24(b, unsigned(data.size() + 22));
    be16(b, 4);
    be16(b, 4);
    be16(b, 1);
    b.push_back(0x10);
    be24(b, unsigned(data.size() + 12));
    be16(b, 0);
    be16(b, 0);
    be16(b, 4);
    be16(b, 4);
    b.insert(b.end(), data.begin(), data.end());
    return b;
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 3) {
            rr::Assets assets(std::filesystem::path(argv[1]).parent_path().string());
            const auto bytes = assets.read(std::filesystem::path(argv[1]).filename().string());
            const auto avi = rr::read_avi(bytes);
            int video = -1;
            for (size_t i = 0; i < avi.streams.size(); ++i)
                if (avi.streams[i].type == rr::fourcc('v', 'i', 'd', 's')) video = int(i);
            if (video < 0 || avi.streams[video].fourcc != rr::fourcc('c', 'v', 'i', 'd'))
                throw std::runtime_error("Expected Cinepak video");
            rr::CinepakDecoder decoder(avi.streams[video].width, avi.streams[video].height);
            std::ofstream out(argv[2], std::ios::binary);
            if (!out) throw std::runtime_error("Cannot write RGB");
            size_t frames = 0;
            for (const auto& c : avi.chunks)
                if (c.stream == video) {
                    if (c.size) decoder.decode(bytes.data() + c.offset, c.size);
                    const auto& rgb = decoder.rgb24();
                    out.write(reinterpret_cast<const char*>(rgb.data()), std::streamsize(rgb.size()));
                    ++frames;
                }
            if (!out) throw std::runtime_error("RGB write failed");
            std::cout << frames << " frames\n";
            return 0;
        }
        auto book = chunk(0x26, {10, 20, 30, 40});
        auto vectors = chunk(0x32, {0});
        book.insert(book.end(), vectors.begin(), vectors.end());
        const auto encoded = frame(book);
        rr::CinepakDecoder decoder(4, 4);
        decoder.decode(encoded.data(), encoded.size());
        const auto expected = decoder.rgb24();
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                for (int c = 0; c < 3; ++c)
                    if (expected[(y * 4 + x) * 3 + c] != 10 + 10 * ((y / 2) * 2 + x / 2))
                        throw std::runtime_error("V1 quadrant expansion failed");
        for (size_t length = 0; length < encoded.size(); ++length) {
            try {
                decoder.decode(encoded.data(), length);
                throw std::logic_error("Truncated frame accepted");
            } catch (const std::runtime_error&) {
            }
            if (decoder.rgb24() != expected)
                throw std::runtime_error("Failed frame changed reference pixels");
        }
        const auto skip = frame(chunk(0x31, {0, 0, 0, 0}));
        decoder.decode(skip.data(), skip.size());
        if (decoder.rgb24() != expected) throw std::runtime_error("Inter-frame skip failed");
        auto le32 = [](Bytes& b, uint32_t n) {
            for (int i = 0; i < 4; ++i) b.push_back(uint8_t(n >> (i * 8)));
        };
        auto riff_chunk = [&](const char* id, const Bytes& payload) {
            Bytes b(id, id + 4);
            le32(b, uint32_t(payload.size()));
            b.insert(b.end(), payload.begin(), payload.end());
            if (payload.size() & 1) b.push_back(0);
            return b;
        };
        auto append = [](Bytes& a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); };
        auto list = [&](const char* kind, const Bytes& payload) {
            Bytes b(kind, kind + 4);
            append(b, payload);
            return riff_chunk("LIST", b);
        };
        Bytes stream_header;
        le32(stream_header, rr::fourcc('v', 'i', 'd', 's'));
        le32(stream_header, rr::fourcc('c', 'v', 'i', 'd'));
        while (stream_header.size() < 20) le32(stream_header, 0);
        le32(stream_header, 1);
        le32(stream_header, 15);
        while (stream_header.size() < 56) le32(stream_header, 0);
        Bytes format;
        le32(format, 40);
        le32(format, 4);
        le32(format, 4);
        format.insert(format.end(), {1, 0, 24, 0});
        le32(format, rr::fourcc('c', 'v', 'i', 'd'));
        while (format.size() < 40) le32(format, 0);
        Bytes stream = riff_chunk("strh", stream_header);
        append(stream, riff_chunk("strf", format));
        Bytes avi_root{'A', 'V', 'I', ' '};
        append(avi_root, list("hdrl", list("strl", stream)));
        Bytes movie_packets;
        for (int i = 0; i < 60; ++i) append(movie_packets, riff_chunk("00dc", encoded));
        append(avi_root, list("movi", movie_packets));
        const auto synthetic = riff_chunk("RIFF", avi_root);
        const auto temp =
            std::filesystem::temp_directory_path() / ("rr-video-clock-" + std::to_string(SDL_GetTicksNS()));
        if (!std::filesystem::create_directory(temp))
            throw std::runtime_error("Cannot create test directory");
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() {
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
            }
        } cleanup{temp};
        std::ofstream file(temp / "clock.avi", std::ios::binary);
        file.write(reinterpret_cast<const char*>(synthetic.data()), std::streamsize(synthetic.size()));
        file.close();
        {
            rr::Mixer mixer(2);
            rr::VideoPlayer movie(rr::Assets(temp.string()), "clock.avi", mixer);
            movie.update(0);
            movie.update(1600000000ull);
            if (movie.frame() != 24) throw std::runtime_error("Synthetic video clock failed");
            movie.pause(true);
            movie.update(6600000000ull);
            movie.pause(false);
            movie.update(6600000000ull);
            if (movie.frame() != 24) throw std::runtime_error("Synthetic video pause drift");
            movie.update(6666666667ull);
            if (movie.frame() != 25) throw std::runtime_error("Synthetic video resume drift");
            if (movie.update(10000000000ull) || !movie.finished())
                throw std::runtime_error("Synthetic video did not finish");
        }
        const auto root = std::filesystem::path(RR_SOURCE_DIR) / "../cdrom/ROADRASH";
        if (std::filesystem::exists(root)) {
            rr::Mixer mixer(4);
            rr::Assets assets(root.string());
            rr::VideoPlayer movie(assets, "VIDEO/RAT/BANZAI.AVI", mixer);
            movie.update(0);
            movie.update(4000000000ull);
            if (movie.frame() != 60) throw std::runtime_error("Video clock frame selection failed");
            movie.pause(true);
            movie.update(9000000000ull);
            if (movie.frame() != 60) throw std::runtime_error("Paused video advanced");
            movie.pause(false);
            movie.update(9000000000ull);
            movie.update(9066666667ull);
            if (movie.frame() != 61) throw std::runtime_error("Video resume lost time or jumped");
            movie.pause(true);
            movie.pause(false);
            movie.update(19000000000ull);
            if (movie.frame() != 61) throw std::runtime_error("Unpolled pause interval advanced video");
            movie.skip();
            if (!movie.finished() || movie.update(20000000000ull))
                throw std::runtime_error("Video skip failed");
        }
        std::cout << "Cinepak synthetic cases and available video clock checks pass\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
