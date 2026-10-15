// Plan 28 Task 4: RaceScreen implementation (moved from run_race, split in two).
#include "race_screen.hpp"
#include "race_caches.hpp"
#include "race_helpers.hpp"
#include "course_scene.hpp"
#include "display.hpp"
#include "road_draw.hpp"
#include "upscale_cache.hpp"
#include "view_state.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <string>

std::string pref_directory();  // game_main.cpp; plan 20 Task 3 cache root

namespace rr {
// DisplayDriver methods (moved unchanged from game_main.cpp).
void DisplayDriver::apply(int logical_h) {
    config = display_config(*settings, logical_h, kMobileDisplay);
    apply_display(window, renderer, config);
    int dw = 0, dh = 0;
    SDL_GetWindowSize(window, &dw, &dh);
    render_scale = effective_render_scale(*settings, dw, dh, kMobileDisplay);
    config.supersampling = 640 * render_scale > dw || 480 * render_scale > dh;
    int cores = SDL_GetNumLogicalCPUCores();
    render_threads = std::max(1, std::min(8, cores - 1));
}
void DisplayDriver::toggle_fullscreen() {
    settings->display_mode = settings->display_mode == DisplayMode::fullscreen
        ? DisplayMode::windowed : DisplayMode::fullscreen;
    save_settings(settings_path, *settings);
    apply(config.logical_h);
}

RaceScreen::RaceScreen(Game& g, SDL_Renderer* r, const RaceOptions& opt, DisplayDriver& d)
    : g_(g), renderer_(r), opt_(opt), display_(d), lifecycle_(g.mixer) {
    canvas_.scale = display_.config.texture_scale;
    if (opt_.start_paused) lifecycle_.state.pause();
    touch_.set_left_handed(opt_.left_handed);
    pad_ = open_first_gamepad();
    if (opt_.show_fps && opt_.assets) fps_ = std::make_unique<FpsOverlay>(*opt_.assets);
    last_ = SDL_GetTicksNS();
    display_.frame = 0;
    display_.apply(480);
}

RaceActionContext RaceScreen::ctx_now() const {
    RaceActionContext c;
    c.demo = opt_.demo; c.career = g_.career; c.finished = g_.race().finished();
    c.paused = lifecycle_.state.paused(); c.quitting = g_.quitting;
    c.has_menu = opt_.menu != nullptr; c.menu_open = opt_.menu && opt_.menu->pause_open();
    c.pad = pad_ ? SDL_GetGamepadID(pad_.get()) : 0;
    return c;
}

void RaceScreen::end_as_quit() {
    if (!decided_) { result_.outcome = RaceOutcome::wrecked; result_.place = std::max(0, g_.race().place(0) - 1); }
}

void RaceScreen::reset_clock(uint64_t now_ns) {
    touch_.clear(); clock_.reset(); last_ = now_ns; g_.reset_view_history();
}

void RaceScreen::event(const SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) { running_ = false; result_.quit_app = true; return; }
    if (!lifecycle_event(e.type)) lifecycle_.state.handle(e);
    if (g_.mixer && !g_.mixer->handle(e)) SDL_Log("Audio reopen failed: %s", SDL_GetError());
    const RaceActionContext act_ctx = ctx_now();
    const RaceAction act = race_action(e, act_ctx);
    if (act == RaceAction::demo_exit) { running_ = false; return; }
    if (act_ctx.menu_open) { pause_event(opt_.menu, e, pad_, renderer_); return; }
    switch (act) {
    case RaceAction::quit_app: running_ = false; result_.quit_app = true; return;
    case RaceAction::open_pause: opt_.menu->open_pause(capture_race_frame(renderer_)); lifecycle_.state.resume(); return;
    case RaceAction::toggle_pause: lifecycle_.state.paused() ? lifecycle_.state.resume() : lifecycle_.state.pause(); return;
    case RaceAction::resume: lifecycle_.state.resume(); return;
    case RaceAction::request_quit: g_.quitting = true; return;
    case RaceAction::cancel_quit: g_.quitting = false; return;
    case RaceAction::confirm_quit: g_.quitting = false; end_as_quit(); running_ = false; return;
    case RaceAction::restart: restart_ = true; return;
    case RaceAction::none: case RaceAction::demo_exit: break;
    }
    if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad_) pad_.reset(SDL_OpenGamepad(e.gdevice.which));
    if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad_ && SDL_GetGamepadID(pad_.get()) == e.gdevice.which) {
        pad_.reset();
        lifecycle_.state.pause();
    }
    if (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION ||
        e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED) {
        if (act == RaceAction::none) touch_.handle(e);
    }
    if (e.type == SDL_EVENT_RENDER_DEVICE_RESET || e.type == SDL_EVENT_RENDER_TARGETS_RESET) {sprites_.clear();canvas_.clear();}
    if (e.type == SDL_EVENT_RENDER_DEVICE_LOST) throw std::runtime_error("Render device lost");
    if (lifecycle_.state.take_clock_reset()) {
        reset_clock(SDL_GetTicksNS());
    }
}

bool RaceScreen::iterate(uint64_t now_ns) {
    (void)now_ns;
    if (!running_) return false;
    if (restart_) g_.restart();   // Enter / pad South / tap after a quick-race finish (README)
    // Plan 18 Task 8: a lifecycle freeze (background return, focus loss,
    // pad removal) opens the pause menu instead of the old PAUSED text.
    if (opt_.menu && !opt_.menu->pause_open() && lifecycle_.state.paused() && !lifecycle_.state.background()) {
        opt_.menu->open_pause(capture_race_frame(renderer_));
        lifecycle_.state.resume();
    }
    if (opt_.menu && opt_.menu->pause_open()) {
        FrontendEvent ev;
        while (opt_.menu->poll(ev)) {
            if (ev.kind == FrontendEvent::Kind::resume_race) lifecycle_.state.resume();
            else if (ev.kind == FrontendEvent::Kind::quit_race) {
                lifecycle_.state.resume();
                if (!decided_) { result_.outcome = RaceOutcome::wrecked; result_.place = std::max(0, g_.race().place(0) - 1); }
                running_ = false;
            } else if (ev.kind == FrontendEvent::Kind::apply_display) display_.apply(480);
            else if (ev.kind == FrontendEvent::Kind::apply_mixer && opt_.apply_settings) opt_.apply_settings();
        }
    }
    if (!running_) return false;
    PlayerInput in = touch_.current();
    const bool* keys = SDL_GetKeyboardState(nullptr);
    // Plan 18 Task 7: custom tables are honored via bound_control_bits below;
    // defaults use the hardcoded keys (touch_/gamepad layer through `in` either way).
    if (!opt_.custom_keys) {
        in.accelerate |= keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
        in.brake |= keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
        in.left |= keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
        in.right |= keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
        in.punch |= keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_LCTRL];
        in.kick |= keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_LALT];
        in.backhand |= keys[SDL_SCANCODE_C];
    }
    const int zone = opt_.stick_deadzone * 32768 / 100;
    if (pad_) {
        const auto south = SDL_GetGamepadButton(pad_.get(), SDL_GAMEPAD_BUTTON_SOUTH);
        const auto west = SDL_GetGamepadButton(pad_.get(), SDL_GAMEPAD_BUTTON_WEST);
        const auto north = SDL_GetGamepadButton(pad_.get(), SDL_GAMEPAD_BUTTON_NORTH);
        in.accelerate |= south || SDL_GetGamepadAxis(pad_.get(), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000;
        in.brake |= SDL_GetGamepadButton(pad_.get(), SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadAxis(pad_.get(), SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000;
        in.left |= SDL_GetGamepadButton(pad_.get(), SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        in.right |= SDL_GetGamepadButton(pad_.get(), SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        in.steer_axis = SDL_GetGamepadAxis(pad_.get(), SDL_GAMEPAD_AXIS_LEFTX);
        in.punch |= opt_.swap_punch_kick ? north : west;
        in.kick |= opt_.swap_punch_kick ? west : north;
        in.backhand |= SDL_GetGamepadButton(pad_.get(), SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    }
    uint32_t controls = opt_.auto_drive ? autopilot(g_.race(), g_.traffic()) : controls_from(in, zone);
    if (!opt_.auto_drive && opt_.custom_keys) controls |= bound_control_bits(opt_.key_codes, true, keys);

    const uint64_t now = SDL_GetTicksNS();
    if (lifecycle_.state.take_clock_reset()) { reset_clock(now); }
    // Plan 18 Task 8: the race loop stops stepping the simulation while
    // pause_open(); the mixer pauses race sounds but the interface levels
    // stay live so the sound sliders apply (no preview click, as in Task 6).
    const bool menu_pause = opt_.menu && opt_.menu->pause_open();
    const bool frozen = lifecycle_.state.paused() || g_.quitting || menu_pause;
    if (g_.mixer) g_.mixer->pause(frozen || lifecycle_.state.background());
    if (g_.race_music && !frozen && !lifecycle_.state.background()) g_.race_music->update();
    const int steps = clock_.advance(now - last_, frozen);
    last_ = now;
    for (int step = 0; step < steps && !frozen; ++step) {
        g_.tick(controls);
        if (!g_.career) continue;
        if (!decided_ && (g_.race().finished() || g_.race().wrecked())) {
            decided_ = true;
            end_ticks_ = race_end_delay;
            result_.place = std::max(0, g_.race().place(0) - 1);
            // 0x00416010: finished becomes won for places 1-3 (0-based < 3), lost otherwise.
            result_.outcome = g_.race().finished() ? (result_.place < 3 ? RaceOutcome::won : RaceOutcome::lost)
                                                : RaceOutcome::wrecked;
        } else if (decided_ && --end_ticks_ <= 0) running_ = false;
    }
    // Plan 21 Task 2: capture the new tick; blend for this frame. With
    // interpolation off, alpha=65536 reproduces the live frame exactly.
    if (steps > 0 && !frozen) {
        g_.view_prev = std::move(g_.view_cur);
        capture_view(g_.view_cur, g_.race(), g_.traffic());
        g_.view_ready = true;
    }
    if (g_.view_ready) {
        const uint32_t alpha = g_.enhancements.interpolate ? clock_.alpha_q16() : 65536;
        blend_view(g_.view_drawn, g_.view_prev, g_.view_cur, alpha);
    }
    if (lifecycle_.state.background()) { SDL_Delay(20); return true; }
    if (menu_pause && opt_.menu) {
        // Plan 18 Task 8: the pause menu draws over the captured frame.
        // Plan 19 Task 8: pause menu draws at the effective scale too.
        int dw = 0, dh = 0;
        if (display_.window) SDL_GetWindowSize(display_.window, &dw, &dh);
        opt_.menu->set_display_size(dw, dh);
        const int pause_lw = opt_.menu->menu_width(dw, dh);
        SDL_SetRenderLogicalPresentation(renderer_, pause_lw, 480, display_.config.presentation);
        const int pause_scale = display_.render_scale;
        opt_.menu->draw(pause_img_, pause_scale);
        if (!pause_tex_ || pause_scale_w_ != pause_img_.width) {
            pause_scale_w_ = pause_img_.width;
            pause_tex_.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                          pause_img_.width, pause_img_.height));
            if (!pause_tex_) throw std::runtime_error(SDL_GetError());
            apply_texture_scale(pause_tex_.get(), display_.config);
        }
        SDL_UpdateTexture(pause_tex_.get(), nullptr, pause_img_.pixels.data(), pause_img_.width * 4);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, pause_tex_.get(), nullptr, nullptr);
    } else {
        SDL_SetRenderDrawColor(renderer_, 15, 18, 22, 255);
        SDL_RenderClear(renderer_);
        const auto slices = project_road(g_.race().route(), g_.blend_camera_position(), g_.blend_camera(), g_.depth);
        int dw = 0, dh = 0;
        if (display_.window) SDL_GetWindowSize(display_.window, &dw, &dh);
        const int race_scale = opt_.menu && opt_.settings ? effective_render_scale(*opt_.settings, dw, dh, kMobileDisplay)
                                                        : opt_.render_scale;
        if (opt_.settings) {
            g_.widescreen = opt_.settings->widescreen;
            g_.enhancements = opt_.settings->enhancements;
            g_.texture_detail = opt_.settings->texture_detail;
        }
        if (g_.scene()) canvas_.draw(renderer_, g_, slices, race_scale, display_.render_threads);
        else draw_road(renderer_, slices);
        draw_opponents(renderer_, g_, sprites_, slices);
        draw_bike(renderer_, g_, sprites_,slices);
        draw_hud(renderer_, g_);
        if (g_.quitting) draw_quit_prompt(renderer_, g_.quit_prompt);
    }
    // Plan 18 Task 9: FPS + simulation ticks/sec, top-right, Futura Bold 24
    // #DEFFF0 with a 2-pixel black shadow like all original text, twice a second.
    if (fps_) fps_->frame(renderer_, now, steps);
    if (!opt_.screenshot.empty()) {
        SurfacePtr capture(SDL_RenderReadPixels(renderer_, nullptr));
        if (!capture || !SDL_SaveBMP(capture.get(), opt_.screenshot.c_str())) throw std::runtime_error(SDL_GetError());
        running_ = false;
        result_.quit_app = true;
    }
    SDL_RenderPresent(renderer_);
    display_.maybe_toggle();
    return running_;
}
}
