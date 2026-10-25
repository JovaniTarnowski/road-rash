// Plan 21 Task 7: CPU post chain (bloom, vignette, motion blur).
// Deviation from the plan's GPU-shader text: the port's rasterizer is CPU and
// no GPU render-state infra exists (plan 20 Task 6 not landed); the effects
// run on the RGB frame with no new dependencies. Semantics pinned here:
// - default-constructed chain applies nothing (all off);
// - configure() stores bloom/vignette/motion_blur;
// - motion-blur factor is 0 at speed 0, <= 0.5 at max speed, and 0 for the
//   first frame after a pause or a teleport;
// - with all effects off, apply() leaves the frame untouched.
#include "post_chain.hpp"
#include "render_threads.hpp"
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    check(rr::motion_blur_factor(0, 20000) == 0.0f, "no blur at speed 0");
    check(rr::motion_blur_factor(20000, 20000) <= 0.5f, "blur capped at max speed");
    check(rr::motion_blur_factor(20000, 20000) > 0.0f, "blur positive at speed");
    check(rr::motion_blur_factor_after_pause() == 0.0f, "no blur after pause");
    check(rr::motion_blur_factor_after_teleport() == 0.0f, "no blur after teleport");
    {
        rr::PostChain chain;
        rr::Enhancements e;
        chain.configure(e);
        check(!chain.any(), "default chain applies nothing");
        e.bloom = true;
        chain.configure(e);
        check(chain.any(), "bloom enables the chain");
    }
    {
        // Off-chain apply leaves the frame untouched.
        rr::PostChain chain;
        rr::Enhancements e;
        chain.configure(e);
        std::vector<uint32_t> frame(64 * 64, 0xff112233u);
        std::vector<uint32_t> copy = frame;
        chain.apply(frame.data(), 64, 64, 0.0f, false);
        check(frame == copy, "off chain leaves the frame untouched");
    }
    {
        // Vignette darkens corners relative to centre on a flat field.
        rr::PostChain chain;
        rr::Enhancements e;
        e.vignette = true;
        chain.configure(e);
        std::vector<uint32_t> frame(64 * 64, 0xffffffffu);
        chain.apply(frame.data(), 64, 64, 0.0f, false);
        const auto lum = [](uint32_t p) { return ((p >> 16) & 0xff) + ((p >> 8) & 0xff) + (p & 0xff); };
        check(lum(frame[0]) < lum(frame[32 * 64 + 32]), "vignette darkens corners");
        check(frame[32 * 64 + 32] == 0xffffffffu, "vignette keeps the centre");
    }
    {
        // Bloom lifts a dark field around a bright bar (no dimming anywhere).
        rr::PostChain chain;
        rr::Enhancements e;
        e.bloom = true;
        chain.configure(e);
        std::vector<uint32_t> frame(64 * 64, 0xff101010u);
        for (int x = 28; x < 36; ++x) frame[size_t(32) * 64 + x] = 0xffffffffu;
        chain.apply(frame.data(), 64, 64, 0.0f, false);
        check(frame[32 * 64 + 24] != 0xff101010u, "bloom spills beside the bar");
        bool dimmed = false;
        for (uint32_t p : frame)
            if (int(p & 0xff) < 0x10 || int((p >> 8) & 0xff) < 0x10 || int((p >> 16) & 0xff) < 0x10)
                dimmed = true;
        check(!dimmed, "bloom never dims");
    }
    {
        // Motion blur blends toward the previous frame.
        rr::PostChain chain;
        rr::Enhancements e;
        e.motion_blur = true;
        chain.configure(e);
        std::vector<uint32_t> frame(16 * 16, 0xffff0000u);
        chain.apply(frame.data(), 16, 16, 20000.0f, false); // seeds previous, blends 0?
        std::vector<uint32_t> blue(16 * 16, 0xff0000ffu);
        chain.apply(blue.data(), 16, 16, 20000.0f, false);
        const uint32_t p = blue[0];
        const int r = (p >> 16) & 0xff, b = p & 0xff;
        check(r > 0 && b > 0 && r < 255 && b < 255, "motion blur mixes frames");
    }
    {
        // Row-parallel passes match the inline chain exactly (every effect,
        // several frames so motion-blur history and the vignette cache run).
        rr::Enhancements e;
        e.bloom = e.vignette = e.motion_blur = true;
        rr::PostChain inline_chain, pooled_chain;
        inline_chain.configure(e);
        pooled_chain.configure(e);
        rr::RenderThreads pool(5);
        const int w = 213, h = 97; // odd sizes: uneven bands and 4x bloom edges
        uint32_t seed = 12345;
        for (int f = 0; f < 4; ++f) {
            std::vector<uint32_t> frame(size_t(w) * h);
            for (auto& p : frame) {
                seed = seed * 1103515245u + 12345u;
                p = 0xff000000u | (seed >> 8);
            }
            std::vector<uint32_t> pooled = frame;
            inline_chain.apply(frame.data(), w, h, 12000.0f, f == 2);
            pooled_chain.apply(pooled.data(), w, h, 12000.0f, f == 2, 20000.0f, &pool);
            check(frame == pooled, "pooled post chain equals inline");
        }
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
