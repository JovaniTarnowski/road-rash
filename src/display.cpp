#include "display.hpp"
#include "view.hpp"

namespace rr {
DisplayConfig display_config(const Settings& s, int logical_h, bool mobile) {
    DisplayConfig c;
    c.fullscreen = mobile || s.display_mode == DisplayMode::fullscreen;
    // Menus/videos stay 640 wide; the race widens its presentation
    // separately (plan 21 Task 8: SDL_SetRenderLogicalPresentation per frame
    // is expensive, so run_race sets it once at startup from race_config).
    c.logical_w = 640;
    c.logical_h = logical_h;
    c.window_w = c.logical_w * s.window_scale;
    c.window_h = logical_h * s.window_scale;
    c.presentation = s.scale_mode == ScaleMode::integer   ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                     : s.scale_mode == ScaleMode::stretch ? SDL_LOGICAL_PRESENTATION_STRETCH
                                                          : SDL_LOGICAL_PRESENTATION_LETTERBOX;
    c.texture_scale = s.filter == FilterMode::smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
    c.vsync = s.vsync ? 1 : 0;
    return c;
}

int effective_render_scale(const Settings& s, int drawable_w, int drawable_h, bool mobile) {
    (void)mobile;
    if (!s.high_resolution || s.inset) return 1; // original Low/Inset choice
    if (s.render_scale >= 1 && s.render_scale <= 4) return s.render_scale;
    int best = 1;
    for (int q = 4; q >= 1; --q)
        if (640 * q <= drawable_w && 480 * q <= drawable_h) {
            best = q;
            break;
        }
    return best;
}

void apply_display(SDL_Window* window, SDL_Renderer* renderer, const DisplayConfig& c) {
    const bool is_full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    if (is_full != c.fullscreen) SDL_SetWindowFullscreen(window, c.fullscreen);
    if (!c.fullscreen) {
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        if (w != c.window_w || h != c.window_h) SDL_SetWindowSize(window, c.window_w, c.window_h);
    }
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode{};
    SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, &mode);
    if (lw != c.logical_w || lh != c.logical_h || mode != c.presentation)
        SDL_SetRenderLogicalPresentation(renderer, c.logical_w, c.logical_h, c.presentation);
    SDL_SetRenderVSync(renderer, c.vsync);
}

void apply_texture_scale(SDL_Texture* texture, const DisplayConfig& c) {
    if (texture) SDL_SetTextureScaleMode(texture, c.supersampling ? SDL_SCALEMODE_LINEAR : c.texture_scale);
}
} // namespace rr
