// Plan 21 Task 7: CPU post-chain implementation.
#include "post_chain.hpp"
#include "render_threads.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace rr {
namespace {
// Runs fn(top, bottom_inclusive) over row bands of [0, rows).
template <class F> void for_rows(RenderThreads* pool, int rows, F&& fn) {
    if (rows <= 0) return;
    if (!pool || pool->count() <= 1) {
        fn(0, rows - 1);
        return;
    }
    pool->run(std::min(pool->count(), rows), rows, [&](int, int top, int bottom) { fn(top, bottom); });
}
} // namespace

float motion_blur_factor(float speed, float max_speed) {
    if (speed <= 0 || max_speed <= 0) return 0.0f;
    return std::min(0.5f, 0.5f * speed / max_speed);
}
float motion_blur_factor_after_pause() {
    return 0.0f;
}
float motion_blur_factor_after_teleport() {
    return 0.0f;
}

void PostChain::configure(const Enhancements& e) {
    bloom_ = e.bloom;
    vignette_ = e.vignette;
    motion_blur_ = e.motion_blur;
    if (!motion_blur_) reset_history();
}
bool PostChain::any() const {
    return bloom_ || vignette_ || motion_blur_;
}
void PostChain::reset_history() {
    previous_.clear();
    prev_w_ = prev_h_ = 0;
}

void PostChain::apply(uint32_t* frame, int width, int height, float speed, bool teleported, float max_speed,
                      RenderThreads* pool) {
    if (!frame || width <= 0 || height <= 0) return;
    const size_t n = size_t(width) * height;
    if (motion_blur_) {
        if (teleported || prev_w_ != width || prev_h_ != height) {
            // First frame after a pause/teleport/resize: seed history, no blend.
            previous_.assign(frame, frame + n);
            prev_w_ = width;
            prev_h_ = height;
        } else {
            const float f =
                teleported ? motion_blur_factor_after_teleport() : motion_blur_factor(speed, max_speed);
            uint32_t* prev = previous_.data();
            for_rows(pool, height, [&](int top, int bottom) {
                const size_t from = size_t(top) * width, to = size_t(bottom + 1) * width;
                if (f > 0) {
                    for (size_t i = from; i < to; ++i) {
                        const uint32_t c = frame[i], p = prev[i];
                        const auto mix = [&](int sh) {
                            const int v = int((c >> sh) & 0xff), u = int((p >> sh) & 0xff);
                            return uint32_t(std::max(0, std::min(255, int(v + (u - v) * f))));
                        };
                        frame[i] = (c & 0xff000000u) | (mix(16) << 16) | (mix(8) << 8) | mix(0);
                    }
                }
                std::memcpy(prev + from, frame + from, (to - from) * 4);
            });
        }
    }
    if (bloom_) {
        // Bright-pass (luminance gate), small box blur, additive composite.
        // Downsampled 4x working buffer keeps it cheap at 4K surfaces.
        const int dw = std::max(1, width / 4), dh = std::max(1, height / 4);
        bright_.assign(size_t(dw) * dh, 0.0f);
        tmp_.resize(bright_.size());
        float* bright = bright_.data();
        float* tmp = tmp_.data();
        for_rows(pool, dh, [&](int top, int bottom) {
            for (int y = top; y <= bottom; ++y)
                for (int x = 0; x < dw; ++x) {
                    float acc = 0.0f;
                    for (int dy = 0; dy < 4; ++dy)
                        for (int dx = 0; dx < 4; ++dx) {
                            const uint32_t p = frame[size_t(std::min(height - 1, y * 4 + dy)) * width +
                                                     std::min(width - 1, x * 4 + dx)];
                            const float lum = (0.2126f * ((p >> 16) & 0xff) + 0.7152f * ((p >> 8) & 0xff) +
                                               0.0722f * (p & 0xff)) /
                                              255.0f;
                            if (lum > 0.6f) acc += lum - 0.6f;
                        }
                    bright[size_t(y) * dw + x] = acc / 16.0f;
                }
        });
        // Two box-blur passes (separable, radius 2): bright -> tmp (x), tmp -> bright (y).
        for_rows(pool, dh, [&](int top, int bottom) {
            for (int y = top; y <= bottom; ++y)
                for (int x = 0; x < dw; ++x) {
                    float acc = 0;
                    int cnt = 0;
                    for (int dx = -2; dx <= 2; ++dx) {
                        const int xx = std::max(0, std::min(dw - 1, x + dx));
                        acc += bright[size_t(y) * dw + xx];
                        ++cnt;
                    }
                    tmp[size_t(y) * dw + x] = acc / cnt;
                }
        });
        for_rows(pool, dh, [&](int top, int bottom) {
            for (int y = top; y <= bottom; ++y)
                for (int x = 0; x < dw; ++x) {
                    float acc = 0;
                    int cnt = 0;
                    for (int dy = -2; dy <= 2; ++dy) {
                        const int yy = std::max(0, std::min(dh - 1, y + dy));
                        acc += tmp[size_t(yy) * dw + x];
                        ++cnt;
                    }
                    bright[size_t(y) * dw + x] = acc / cnt;
                }
        });
        // Additive composite, sampled bilinear-ish (nearest is fine at 4x).
        for_rows(pool, height, [&](int top, int bottom) {
            for (int y = top; y <= bottom; ++y)
                for (int x = 0; x < width; ++x) {
                    const float b = bright[size_t(std::min(dh - 1, y / 4)) * dw + std::min(dw - 1, x / 4)];
                    if (b <= 0.001f) continue;
                    uint32_t& p = frame[size_t(y) * width + x];
                    const auto add = [&](int sh) {
                        return uint32_t(std::min(255.0f, float((p >> sh) & 0xff) + b * 400.0f));
                    };
                    p = (p & 0xff000000u) | (add(16) << 16) | (add(8) << 8) | add(0);
                }
        });
    }
    if (vignette_) {
        // Multiplicative edge darkening, centre untouched (radius-normalized).
        // The per-pixel factor depends only on the size: computed once.
        if (vig_w_ != width || vig_h_ != height) {
            vignette_gain_.resize(n);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    const double nx = (double(x) / (width - 1)) * 2 - 1;
                    const double ny = (double(y) / (height - 1)) * 2 - 1;
                    const double d = std::sqrt(nx * nx + ny * ny) / std::sqrt(2.0);
                    vignette_gain_[size_t(y) * width + x] = std::max(0.0, 1.0 - 0.45 * d * d);
                }
            vig_w_ = width;
            vig_h_ = height;
        }
        const double* gain = vignette_gain_.data();
        for_rows(pool, height, [&](int top, int bottom) {
            for (size_t i = size_t(top) * width, end = size_t(bottom + 1) * width; i < end; ++i) {
                const double v = gain[i];
                if (v >= 0.9995) continue; // centre untouched
                uint32_t& p = frame[i];
                const auto mul = [&](int sh) { return uint32_t(double((p >> sh) & 0xff) * v); };
                p = (p & 0xff000000u) | (mul(16) << 16) | (mul(8) << 8) | mul(0);
            }
        });
    }
}
} // namespace rr
