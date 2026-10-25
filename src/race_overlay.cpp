// Plan 28 Task 3: verbatim move of the run_race FPS + quit-prompt blocks.
#include "race_overlay.hpp"
#include "pe_strings.hpp"

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
