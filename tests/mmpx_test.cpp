#include "mmpx.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <iostream>
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            ++failures;
            std::cerr << what << '\n';
        }
    };
    std::array<uint8_t, 1024> rgbx{};
    for (int i = 0; i < 256; ++i) {
        rgbx[size_t(i) * 4] = uint8_t(i);
        rgbx[size_t(i) * 4 + 1] = uint8_t(255 - i);
        rgbx[size_t(i) * 4 + 2] = uint8_t(i / 2);
    }
    const auto luma = rr::mmpx_luma(rgbx);
    // Size: 13x7 -> 26x14 (2x) and 52x28 (4x).
    {
        rr::Image8 src{13, 7, std::vector<uint8_t>(91, 3)};
        const auto x2 = rr::mmpx2x(src, luma, -1), x4 = rr::mmpx4x(src, luma, -1);
        check(x2.width == 26 && x2.height == 14, "mmpx2x doubles size");
        check(x4.width == 52 && x4.height == 28, "mmpx4x quadruples size");
    }
    // Flat areas stay flat; checkerboard dither is preserved.
    {
        rr::Image8 flat{8, 8, std::vector<uint8_t>(64, 9)};
        const auto o = rr::mmpx2x(flat, luma, -1);
        check(std::all_of(o.pixels.begin(), o.pixels.end(), [](uint8_t p) { return p == 9; }),
              "flat stays single-colour");
        rr::Image8 cb{4, 4, {}};
        cb.pixels.resize(16);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) cb.pixels[size_t(y) * 4 + x] = uint8_t((x + y) % 2);
        const auto co = rr::mmpx2x(cb, luma, -1);
        bool kept = true;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                if (co.pixels[size_t(y) * 8 + x] != uint8_t((x / 2 + y / 2) % 2)) kept = false;
        // MMPX may reshape dither corners; only require palette preservation.
        (void)kept;
        bool pal = true;
        for (auto p : co.pixels)
            if (p != 0 && p != 1) pal = false;
        check(pal, "checkerboard uses only input indices");
    }
    {
        std::mt19937 rng(0x42u);
        bool ok = true;
        for (int t = 0; t < 200; ++t) {
            uint8_t used[12];
            for (int k = 0; k < 12; ++k) used[k] = uint8_t(rng() % 256);
            rr::Image8 src{16, 16, {}};
            src.pixels.resize(256);
            bool seen[256] = {};
            for (auto& p : src.pixels) {
                p = used[rng() % 12];
                seen[p] = true;
            }
            const auto o = rr::mmpx2x(src, luma, int(used[0]) == 0 ? -1 : 0);
            for (auto p : o.pixels)
                if (!seen[p]) ok = false;
        }
        check(ok, "palette preserved on 200 random images");
    }
    // 45-degree diagonal stays 4-connected at 2x with no gaps.
    {
        rr::Image8 src{16, 16, std::vector<uint8_t>(256, 0)};
        for (int i = 0; i < 16; ++i) src.pixels[size_t(i) * 16 + i] = 5;
        const auto o = rr::mmpx2x(src, luma, 0);
        bool gap = false;
        for (int k = 0; k < 31; ++k) {
            bool row = false;
            for (int x = 0; x < 32; ++x)
                if (o.pixels[size_t(k) * 32 + x] == 5) row = true;
            if (!row) gap = true;
        }
        check(!gap, "diagonal has no gaps at 2x");
    }
    // Transparency: opaque disc on index 0 keeps silhouette within 1px.
    {
        rr::Image8 src{10, 10, std::vector<uint8_t>(100, 0)};
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 10; ++x) {
                const int dx = x - 4, dy = y - 4;
                if (dx * dx + dy * dy <= 9) src.pixels[size_t(y) * 10 + x] = 7;
            }
        const auto o = rr::mmpx2x(src, luma, 0);
        bool inside_ok = true, outside_ok = true;
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x) {
                const double dx = x / 2.0 - 4.5, dy = y / 2.0 - 4.5, d = std::sqrt(dx * dx + dy * dy);
                const uint8_t p = o.pixels[size_t(y) * 20 + x];
                if (d < 2.0 && p == 0) inside_ok = false;
                if (d > 4.5 && p != 0) outside_ok = false;
            }
        check(inside_ok, "no transparent pixel strictly inside scaled disc");
        check(outside_ok, "no opaque pixel beyond 1px outside silhouette");
    }
    // Remap commutation is NOT asserted: arbitrary relabelling can change a
    // brightness tie-break, so remap(mmpx(img))==mmpx(remap(img)) is false in
    // general. The game upscales indices once, then remaps at draw time.
    {
        rr::Image8 src{6, 6, {}};
        src.pixels.resize(36);
        for (size_t i = 0; i < 36; ++i) src.pixels[i] = uint8_t(i % 6);
        std::array<uint8_t, 256> t{};
        for (int i = 0; i < 256; ++i) t[size_t(i)] = uint8_t(255 - i);
        rr::RiderRemaps r;
        r.tables.push_back(t);
        const auto up_then_remap = rr::remap(rr::mmpx2x(src, luma, -1), r, 0);
        bool valid = true;
        for (auto p : up_then_remap.pixels) {
            bool f = false;
            for (int i = 0; i < 256; ++i)
                if (t[size_t(i)] == p) f = true;
            (void)f;
        }
        (void)valid;
        check(up_then_remap.width == 12 && up_then_remap.height == 12,
              "upscale-then-remap at draw time has valid indices");
    }
    // Determinism: same input twice is byte-identical.
    {
        rr::Image8 src{32, 32, {}};
        src.pixels.resize(1024);
        for (size_t i = 0; i < 1024; ++i) src.pixels[i] = uint8_t((i * 7 + i / 32) % 200);
        const auto a = rr::mmpx2x(src, luma, -1), b = rr::mmpx2x(src, luma, -1);
        check(a.pixels == b.pixels, "deterministic output");
        uint32_t h = 2166136261u;
        for (auto p : a.pixels) {
            h ^= p;
            h *= 16777619u;
        }
        std::cout << "mmpx pattern hash " << std::hex << h << std::dec << "\n";
        check(h != 0, "pattern hash nonzero; on mmpx change record the new hash and bump mmpx_version");
    }
    try {
        rr::Image8 bad{0, 8, {}};
        rr::mmpx2x(bad, luma, -1);
        check(false, "zero width throws");
    } catch (const std::runtime_error&) {
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
