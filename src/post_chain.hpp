// Plan 21 Task 7: CPU post chain (bloom, vignette, motion blur).
// Deviation: the plan's GPU-shader text assumes render-state infra from plan 20
// Task 6, which has not landed; the port rasterizes on the CPU, so the effects
// run on the RGB frame with no new dependencies. Order: motion blur (temporal)
// -> bloom (additive) -> vignette (multiplicative). Grade stays in game_main
// (Task 5 post-pass, runs before this chain).
#pragma once
#include <cstdint>
#include <vector>
#include "settings.hpp"

namespace rr {
class RenderThreads;
// Motion-blur blend factor for a camera speed (0 at rest, <= 0.5 at max).
float motion_blur_factor(float speed, float max_speed);
// Factor overrides for state transitions (always 0).
float motion_blur_factor_after_pause();
float motion_blur_factor_after_teleport();
class PostChain {
public:
    PostChain() = default;
    void configure(const Enhancements& e);
    bool any() const;
    // Frame is ARGB8888 ints (alpha channel carried, colour graded already).
    // speed/max_speed drive motion blur; teleported (or paused) resets history.
    // A pool splits every pass into row bands; results are identical to the
    // inline (null pool) run because each pixel is computed independently.
    void apply(uint32_t* frame, int width, int height, float speed, bool teleported,
               float max_speed = 20000.0f, RenderThreads* pool = nullptr);
    void reset_history();
private:
    bool bloom_ = false, vignette_ = false, motion_blur_ = false;
    std::vector<uint32_t> previous_;
    int prev_w_ = 0, prev_h_ = 0;
    std::vector<float> bright_, tmp_;      // bloom working buffers, reused
    std::vector<double> vignette_gain_;    // per-pixel factor for vig_w_ x vig_h_
    int vig_w_ = 0, vig_h_ = 0;
};
}  // namespace rr
