// Plan 28 Task 3: verbatim move of the run_race FPS + quit-prompt blocks.
#include "race_overlay.hpp"
#include "pe_strings.hpp"
#include <stdexcept>

namespace rr {
FpsOverlay::FpsOverlay(const Assets& assets) : assets_(assets) {}

void FpsOverlay::frame(SDL_Renderer* renderer, uint64_t now, int ticks_this_frame) {
    ++frames_;
    tick_acc_ += ticks_this_frame;
    const uint64_t now_ms = now / 1000000;
    if (now_ms >= next_) {
        const uint64_t span = now_ms >= last_ ? now_ms - last_ : 1;
        v_ = int(uint64_t(frames_) * 1000 / span);
        tps_ = int(uint64_t(tick_acc_) * 1000 / span);
        frames_ = 0;
        tick_acc_ = 0;
        last_ = now_ms;
        next_ = now_ms + 500;
        if (!text_) text_ = std::make_unique<TextRenderer>(assets_);
        if (text_->has(Face::futura_bold)) {
            const std::string s = std::to_string(v_) + " FPS " + std::to_string(tps_) + " TPS";
            const int w = text_->width(Face::futura_bold, 24, s) + 4;
            img_ = RgbImage(w, 30, 0);
            text_->draw(img_, Face::futura_bold, 24, 2, 2, 0x404040, s);
            text_->draw(img_, Face::futura_bold, 24, 0, 0, 0xdefff0, s);
            // Opaque ink over a transparent background (straight-alpha
            // ARGB8888) so the sky shows through.
            argb_.assign(size_t(w) * 30, 0);
            for (size_t i = 0; i < argb_.size(); ++i)
                if (img_.pixels[i]) argb_[i] = 0xff000000u | img_.pixels[i];
            if (tex_) {
                float tw = 0, th = 0;
                SDL_GetTextureSize(tex_.get(), &tw, &th);
                if (int(tw) != w || int(th) != 30) tex_.reset();
            }
            if (!tex_) {
                tex_.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                             w, 30));
                if (tex_) SDL_SetTextureBlendMode(tex_.get(), SDL_BLENDMODE_BLEND);
            }
            if (tex_) SDL_UpdateTexture(tex_.get(), nullptr, argb_.data(), w * 4);
        }
    }
    if (tex_) {
        // Logical space is 640 wide in the race; pin to its top-right.
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex_.get(), &tw, &th);
        const SDL_FRect dst{640.f - tw - 8.f, 8.f, tw, th};
        SDL_RenderTexture(renderer, tex_.get(), nullptr, &dst);
    }
}

void NetworkOverlay::frame(SDL_Renderer* renderer, const net::Client::Stats* stats, int width) {
    // Original race log: six persistent lines, clipped to the newest line in
    // the normal 0x15-pixel top strip (0x00446c30). No expiry timer.
    std::vector<std::string> visible;
    if (!lines_.empty()) visible.push_back(lines_.back());
    if (stats)
        visible.push_back("RTT " + std::to_string(int(stats->rtt_ms)) + " ms  rollback " +
                          std::to_string(stats->rollback_depth) + "  late " + std::to_string(stats->late));
    if (visible.empty()) return;
    if (!text_ || !text_->has(Face::futura_bold)) {
        SDL_SetRenderDrawColor(renderer, 222, 255, 240, 255);
        for (size_t i = 0; i < visible.size(); ++i)
            SDL_RenderDebugText(renderer, 0, float(i * 21), visible[i].c_str());
        return;
    }
    if (texture_ && painted_width_ == width && painted_ == visible) {
        const SDL_FRect destination{0, 0, float(width), float(visible.size() * 21)};
        SDL_RenderTexture(renderer, texture_.get(), nullptr, &destination);
        return;
    }
    painted_ = visible;
    painted_width_ = width;
    RgbImage image(width, int(visible.size() * 21), 0);
    for (size_t i = 0; i < visible.size(); ++i) {
        text_->draw(image, Face::futura_bold, 14, 2, int(i * 21 + 2), 0x404040, visible[i]);
        text_->draw(image, Face::futura_bold, 14, 0, int(i * 21), 0xdefff0, visible[i]);
    }
    std::vector<uint32_t> argb(image.pixels.size());
    for (size_t i = 0; i < argb.size(); ++i)
        if (image.pixels[i]) argb[i] = 0xff000000u | image.pixels[i];
    texture_.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width,
                                     image.height));
    if (!texture_) throw std::runtime_error(SDL_GetError());
    SDL_SetTextureBlendMode(texture_.get(), SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture_.get(), nullptr, argb.data(), width * 4);
    const SDL_FRect destination{0, 0, float(width), float(image.height)};
    SDL_RenderTexture(renderer, texture_.get(), nullptr, &destination);
}

void draw_quit_prompt(SDL_Renderer* renderer, const std::string& prompt_cp1252) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
    SDL_RenderFillRect(renderer, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    float y = 200;
    size_t from = 0;
    while (from <= prompt_cp1252.size()) {
        size_t to = prompt_cp1252.find('\n', from);
        if (to == std::string::npos) to = prompt_cp1252.size();
        const std::string line = cp1252_to_utf8(prompt_cp1252.substr(from, to - from));
        SDL_RenderDebugText(renderer, 320 - 4.f * float(line.size()), y, line.c_str());
        y += 14;
        from = to + 1;
    }
}
} // namespace rr
