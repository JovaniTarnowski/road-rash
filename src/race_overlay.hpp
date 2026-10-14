// Plan 28 Task 3: FPS overlay + quit prompt as units (verbatim move).
#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "assets.hpp"
#include "sdl_handles.hpp"
#include "text.hpp"

namespace rr {
class FpsOverlay {     // Futura Bold 24 #DEFFF0, 2-px black shadow, refreshed every 500 ms
public:
    explicit FpsOverlay(const Assets& assets);
    void add_ticks(int steps) { tick_acc_ += steps; }
    void frame(SDL_Renderer*, uint64_t now_ns, int ticks_this_frame);   // counts + redraws when due, then renders
private:
    const Assets& assets_;
    std::unique_ptr<TextRenderer> text_;
    TexturePtr tex_;
    RgbImage img_;
    uint64_t next_ = 0, last_ = 0;
    int frames_ = 0, tick_acc_ = 0, v_ = 0, tps_ = 0;
    std::vector<uint32_t> argb_;
};
void draw_quit_prompt(SDL_Renderer*, const std::string& prompt_cp1252);   // the g.quitting block
}
