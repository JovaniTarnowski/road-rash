#include "texture.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
#include <cstdlib>
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            ++failures;
            std::cerr << what << '\n';
        }
    };
    std::vector<uint8_t> pixels(8192);
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = uint8_t(i * 37 + i / 7);
    std::vector<uint8_t> dest(64, 125);
    rr::texture_span(dest.data(), 4, std::vector<uint8_t>{0, 12, 0, 34}, 2, 0, 0, 0, 65536, true);
    if (dest[0] != 125 || dest[1] != 12 || dest[2] != 125 || dest[3] != 34) ++failures;
    try {
        rr::texture_span(dest.data(), 4, pixels, 9, 0, 0, 0, 0, false);
        ++failures;
    } catch (const std::runtime_error&) {
    }
    // Signed mip selection: negative addresses upscaled levels above mip 0;
    // non-negative is the original index. Never shift UVs by a negative.
    check(rr::texture_mip_level_signed(8, 8, 32, 32, 3, 2) == 1, "quarter-size quad picks original mip 1");
    check(rr::texture_mip_level_signed(16, 16, 32, 32, 3, 2) == 0, "half-size quad picks mip 0");
    check(rr::texture_mip_level_signed(64, 64, 32, 32, 3, 2) == -1,
          "2x-projected quad picks first upscaled level");
    check(rr::texture_mip_level_signed(128, 128, 32, 32, 3, 2) == -2,
          "4x-projected quad picks second upscaled level");
    check(rr::texture_mip_level_signed(128, 128, 32, 32, 3, 0) == 0, "no upscaled levels keeps mip 0");
    try {
        rr::texture_mip_level_signed(8, 8, 32, 32, 0, 0);
        check(false, "empty levels throw");
    } catch (const std::runtime_error&) {
    }
    // Per-worker scratch: sized to height, rejects unsized use. The wide
    // path uses 64-bit edges: int32 Q16 cannot hold |x|>32768.
    {
        rr::RasterScratch64 scratch(1440);
        check(scratch.rows.size() == 1440, "scratch sized to surface height");
        rr::TextureVertexWide a{10, 0, 0, 0}, b{20, 1439, 64, 64};
        rr::texture_edge_wide(scratch, a, b);
        check(scratch.rows[0].x == (int64_t(10) << 16) &&
                  std::llabs(scratch.rows[1439].x - (int64_t(20) << 16)) < 65536,
              "wide edge spans all rows");
        rr::RasterScratch64 empty;
        try {
            rr::texture_edge_wide(empty, a, b);
            check(false, "unsized scratch throws");
        } catch (const std::runtime_error&) {
        }
        try {
            rr::RasterScratch64 bad(-1);
            check(false, "negative height throws");
        } catch (const std::runtime_error&) {
        }
    }
    // Wide path at extreme vertices: x = +/-40000 rasterises clipped
    // without int16 wrap. Wide UVs are texels (u16, not Q16).
    {
        std::vector<uint8_t> tex(256 * 256, 0);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x) tex[size_t(y) * 256 + x] = uint8_t(((x + y) & 0x7f) + 1);
        rr::Framebuffer fb(640, 480);
        fb.clear(0);
        rr::RasterScratch64 scratch(480);
        std::array<rr::TextureVertexWide, 4> wide{
            {{40000, 10, 0, 0}, {40100, 10, 255, 0}, {40100, 110, 255, 255}, {40000, 110, 0, 255}}};
        rr::draw_textured_quad_wide(fb, wide, tex, 256, 8, false, {0, 0, 640, 479}, false, scratch);
        int drawn = 0;
        for (int y = 0; y < 480; ++y)
            for (int x = 0; x < 640; ++x) drawn += fb.row(y)[x] != 0;
        std::array<rr::TextureVertexWide, 4> cross{{{int32_t(-40000), 10, 0, 0},
                                                    {40000, 10, 255, 0},
                                                    {40000, 110, 255, 255},
                                                    {int32_t(-40000), 110, 0, 255}}};
        rr::draw_textured_quad_wide(fb, cross, tex, 256, 8, false, {0, 0, 640, 479}, false, scratch);
        drawn = 0;
        for (int y = 0; y < 480; ++y)
            for (int x = 0; x < 640; ++x) drawn += fb.row(y)[x] != 0;
        check(drawn > 0, "crossing wide quad draws without wrap");
        // Upscaled storage path: width-addressed (strides above 256 cannot
        // use the 16-bit sampling-storage path).
        std::vector<uint8_t> big(size_t(1024) * 64, 7);
        rr::Framebuffer fb2(64, 64);
        fb2.clear(0);
        rr::RasterScratch64 s2(64);
        std::array<rr::TextureVertexWide, 4> q{
            {{0, 0, 0, 0}, {64, 0, 1024, 0}, {64, 64, 1024, 64}, {0, 64, 0, 64}}};
        rr::draw_textured_quad_wide(fb2, q, big, 1024, 10, false, {0, 0, 64, 63}, true, s2);
        check(fb2.row(32)[32] == 7, "upscaled storage path samples plain addressing");
    }
    const auto path = rr::golden::oracle_dir() + "/raster-fixtures.tsv";
    if (std::filesystem::exists(path)) {
        try {
            const auto golden = rr::golden::load(path);
            const auto& frame = golden.frames.front();
            for (int c = 0; c < 1008; ++c) {
                const std::string prefix = "texture.span." + std::to_string(c) + ".";
                auto field = [&](const std::string& key) {
                    const auto it = frame.find(prefix + key);
                    if (it == frame.end() || it->second < INT32_MIN || it->second > INT32_MAX)
                        throw std::runtime_error("Missing span field: " + prefix + key);
                    return int32_t(it->second);
                };
                const int count = field("count"), alignment = field("alignment"), shift = field("shift");
                if (count < 0 || count > 17 || alignment < 0 || alignment > 3 || shift < 0 || shift > 8)
                    throw std::runtime_error("Invalid span extent");
                std::fill(dest.begin(), dest.end(), 125);
                rr::texture_span(dest.data() + alignment, size_t(count), pixels, unsigned(shift),
                                 field("row"), field("column"), field("row_step"), field("column_step"),
                                 field("transparent") != 0);
                for (int i = 0; i < 32; ++i)
                    if (dest[i] != field("output." + std::to_string(i))) {
                        if (failures < 12) std::cerr << prefix + "output." << i << '\n';
                        ++failures;
                    }
            }
            std::cout << "1008 original texture spans compared\n";
            std::array<std::vector<uint8_t>, 3> textures;
            for (int level = 0; level < 3; ++level) {
                textures[level].resize(size_t(1024) >> (level * 2));
                for (size_t i = 0; i < textures[level].size(); ++i)
                    textures[level][i] = uint8_t(i * 37 + i / 7 + level * 53);
            }
            for (int c = 0; c < 64; ++c) {
                const std::string prefix = "texture.quad." + std::to_string(c) + ".";
                std::array<rr::TextureVertex, 4> vertices;
                for (int i = 0; i < 4; ++i) {
                    auto get = [&](int j) {
                        return uint16_t(frame.at(prefix + "vertex." + std::to_string(i * 4 + j)));
                    };
                    vertices[i] = {int16_t(get(0)), int16_t(get(1)), get(2), get(3)};
                }
                int left = vertices[0].x, right = left, top = vertices[0].y, bottom = top;
                for (const auto& vertex : vertices) {
                    left = std::min(left, int(vertex.x));
                    right = std::max(right, int(vertex.x));
                    top = std::min(top, int(vertex.y));
                    bottom = std::max(bottom, int(vertex.y));
                }
                const auto level =
                    rr::texture_mip_level(uint32_t(right - left + 1), uint32_t(bottom - top + 1), 33, 33, 3);
                if (level != size_t(frame.at(prefix + "level"))) {
                    ++failures;
                    std::cerr << "mip mismatch " << c << '\n';
                }
                for (auto& vertex : vertices) {
                    vertex.u = uint16_t(vertex.u >> level);
                    vertex.v = uint16_t(vertex.v >> level);
                }
                rr::Framebuffer fb(640, 480);
                fb.clear(125);
                rr::draw_textured_quad(fb, vertices, textures[level], unsigned(5 - level),
                                       frame.at(prefix + "transparent") != 0, {0, 0, 640, 479});
                const auto raw = std::filesystem::path(path).parent_path() /
                                 (golden.scenario + "-texture" + std::to_string(c) + ".raw");
                std::ifstream in(raw, std::ios::binary);
                if (!in) throw std::runtime_error("Missing textured pixels");
                const std::vector<uint8_t> expected{std::istreambuf_iterator<char>(in),
                                                    std::istreambuf_iterator<char>()};
                if (expected.size() != 640 * 480) throw std::runtime_error("Wrong textured image size");
                size_t count = 0;
                for (int y = 0; y < 480; ++y)
                    for (int x = 0; x < 640; ++x)
                        if (fb.row(y)[x] != expected[size_t(y) * 640 + x]) {
                            if (!count && failures < 12)
                                std::cerr << "texture quad " << c << " at " << x << "," << y
                                          << " native=" << int(fb.row(y)[x])
                                          << " original=" << int(expected[size_t(y) * 640 + x]) << '\n';
                            ++count;
                        }
                if (count) {
                    ++failures;
                    std::cerr << "quad " << c << ": " << count << " pixel mismatches\n";
                }
            }
            std::cout << "64 original textured quads compared\n";
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            return 1;
        }
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
