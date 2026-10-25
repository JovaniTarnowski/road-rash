#include "golden.hpp"
#include "raster.hpp"
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
int main(int argc, char** argv) {
    const std::filesystem::path path = argc > 1 ? argv[1] : rr::golden::oracle_dir() + "/raster-fixtures.tsv";
    if (!std::filesystem::exists(path)) {
        std::cout << "SKIP raster oracle capture absent\n";
        return 77;
    }
    try {
        const auto file = rr::golden::load(path.string());
        if (file.exe != "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" ||
            file.frames.empty())
            throw std::runtime_error("Invalid raster reference metadata");
        if (file.scenario.empty() ||
            file.scenario.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
            throw std::runtime_error("Invalid raster scenario name");
        const auto& frame = file.frames.front();
        auto value = [&](const std::string& key, int64_t low, int64_t high) {
            const auto it = frame.find(key);
            if (it == frame.end() || it->second < low || it->second > high)
                throw std::runtime_error("Invalid/missing raster field: " + key);
            return int(it->second);
        };
        const int count = value("raster.count", 1, 64);
        size_t mismatches = 0;
        for (int c = 0; c < count; ++c) {
            const std::string prefix = "raster." + std::to_string(c) + ".";
            const int width = value(prefix + "width", 1, 1280), height = value(prefix + "height", 1, 960);
            rr::RasterQuad quad;
            quad.colour = uint8_t(value(prefix + "colour", 0, 255));
            for (size_t v = 0; v < 4; ++v) {
                quad.points[v].x = value(prefix + "vertex." + std::to_string(v * 2), -65536, 65535);
                quad.points[v].y = value(prefix + "vertex." + std::to_string(v * 2 + 1), -65536, 65535);
            }
            rr::RasterClip clip{value(prefix + "clip.0", 0, width), value(prefix + "clip.1", 0, height - 1),
                                value(prefix + "clip.2", 0, width), value(prefix + "clip.3", 0, height - 1)};
            const auto raw = path.parent_path() / (file.scenario + "-raster" + std::to_string(c) + ".raw");
            std::ifstream input(raw, std::ios::binary);
            if (!input) throw std::runtime_error("Missing raster pixels: " + raw.string());
            const std::vector<uint8_t> expected{std::istreambuf_iterator<char>(input),
                                                std::istreambuf_iterator<char>()};
            if (expected.size() != size_t(width) * height)
                throw std::runtime_error("Invalid raster image extent");
            rr::Framebuffer fb(width, height);
            fb.clear(0);
            rr::fill_quad(fb, quad, clip);
            rr::Framebuffer complete(width, height);
            complete.clear(0);
            for (size_t v = 0; v < 4; ++v) {
                quad.points[v].x = value(prefix + "input." + std::to_string(v * 2), -65536, 65535);
                quad.points[v].y = value(prefix + "input." + std::to_string(v * 2 + 1), -65536, 65535);
            }
            rr::draw_flat_quad(complete, quad, clip);
            size_t pixels = 0;
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    if (fb.row(y)[x] != expected[size_t(y) * width + x] ||
                        complete.row(y)[x] != expected[size_t(y) * width + x]) {
                        if (!pixels)
                            std::cerr << "case " << c << " first mismatch (" << x << "," << y
                                      << "): " << int(fb.row(y)[x]) << " vs "
                                      << int(expected[size_t(y) * width + x]) << '\n';
                        ++pixels;
                    }
            if (pixels) {
                ++mismatches;
                std::cerr << "case " << c << ": " << pixels << " mismatched pixels\n";
            }
        }
        std::cout << count << " raster fixtures compared, " << mismatches << " mismatched\n";
        return mismatches ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
