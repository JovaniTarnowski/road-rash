#include "display.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    rr::Settings s = rr::default_settings();
    s.display_mode = rr::DisplayMode::windowed;
    s.window_scale = 2;
    auto c = rr::display_config(s, 480, false);
    check(!c.fullscreen && c.window_w == 1280 && c.window_h == 960 && c.logical_h == 480,
          "2x window for menus");
    c = rr::display_config(s, 510, false);
    check(c.window_h == 1020 && c.logical_h == 510, "race window keeps the dashboard strip");
    s.scale_mode = rr::ScaleMode::integer;
    check(rr::display_config(s, 480, false).presentation == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE,
          "integer scaling");
    s.scale_mode = rr::ScaleMode::stretch;
    check(rr::display_config(s, 480, false).presentation == SDL_LOGICAL_PRESENTATION_STRETCH, "stretch");
    s.filter = rr::FilterMode::smooth;
    check(rr::display_config(s, 480, false).texture_scale == SDL_SCALEMODE_LINEAR, "smooth filter");
    s.vsync = false;
    check(rr::display_config(s, 480, false).vsync == 0, "vsync off");
    s.display_mode = rr::DisplayMode::windowed;
    check(rr::display_config(s, 480, true).fullscreen, "mobile is always fullscreen");
    // Plan 19 Task 8: effective_render_scale. 0 picks the largest s <= 4
    // fitting the drawable; fixed settings pass through; Low/Inset forces 1.
    s.render_scale = 0;
    s.high_resolution = true;
    check(rr::effective_render_scale(s, 2560, 1600, false) == 3, "match display 2560x1600 -> 3");
    check(rr::effective_render_scale(s, 1920, 1080, false) == 2, "match display 1920x1080 -> 2");
    s.render_scale = 3;
    check(rr::effective_render_scale(s, 800, 600, false) == 3, "fixed setting passes through");
    s.high_resolution = false;
    check(rr::effective_render_scale(s, 2560, 1600, false) == 1, "low resolution forces 1");
    s.high_resolution = true;
    s.inset = true;
    check(rr::effective_render_scale(s, 2560, 1600, false) == 1, "inset forces 1");
    return failures ? 1 : 0;
}
