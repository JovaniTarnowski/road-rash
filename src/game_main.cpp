// Playable single-course race: original bike physics and road projection,
// placeholder shading (the original raster, sprites and HUD are plans 02/03/06).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "assets.hpp"
#include "bob.hpp"
#include "framebuffer.hpp"
#include "input.hpp"
#include "importer.hpp"
#include "import_ui.hpp"
#include "race.hpp"
#include "road_draw.hpp"
#include "session.hpp"
#include "video_player.hpp"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

namespace {
const char* const course_names[] = {"NAPA", "CITY", "HIWAY", "CANYN", "MEDLY"};
constexpr int countdown_ticks = 3 * rr::ticks_per_second;

struct Game {
    rr::DepthTable depth;
    rr::PhysicsTables physics;
    std::vector<rr::BikeSpec> specs;
    rr::Course course;
    std::string course_name;
    int bike = 0;
    std::unique_ptr<rr::Race> race;
    int countdown = countdown_ticks;
    std::vector<rr::Image8> bobs;
    rr::RiderRemaps remaps;
    rr::Palette palette;
    std::vector<rr::Opponent> opponents;
    int level = 0;

    void restart() {
        race = std::make_unique<rr::Race>(rr::make_route(course, false), rr::make_route(course, true),
                                          physics, specs.at(size_t(bike)), 90, opponents, level);
        countdown = countdown_ticks;
    }
    void tick(uint32_t controls) {
        if (countdown > 0) { --countdown; return; }   // race clock starts after the countdown
        race->step(controls);
    }
};

// Keep-to-centre driver used by --autopilot for demos and smoke runs.
uint32_t autopilot(const rr::Race& race) {
    const auto& s = race.route()[race.position() >> 8];
    const int x = race.lateral() - s.offset;
    uint32_t c = rr::control_accelerate;
    if (x < -60) c |= rr::control_right; else if (x > 60) c |= rr::control_left;
    if (race.ticks() % 40 == 0) c |= rr::control_punch;   // demo: swing at anyone alongside
    return c;
}

void quad(SDL_Renderer* r, float x0, float x1, float y0, float x2, float x3, float y1, SDL_FColor c) {
    const SDL_Vertex v[4] = {{{x0,y0},c,{}}, {{x1,y0},c,{}}, {{x3,y1},c,{}}, {{x2,y1},c,{}}};
    const int index[6] = {0,1,2,0,2,3};
    SDL_RenderGeometry(r, nullptr, v, 4, index, 6);
}

// Riding frame from the steering state. Frames 4..11 of HIBOB.DAT are the lean sequence
// (4 strongest right, 11 strongest left) and 3 is straight; the original picks frames through
// its RRAN/RRFD animation system (0x00445a60, 0x0043ae90), which is not ported yet.
// Attack frames picked by inspection of HIBOB.DAT (wind-up, strike) per weapon and side.
int attack_frame(const rr::Rider& r) {
    const bool strike = r.attack.timer >= rr::attack_hit_tick;
    switch (r.attack.weapon) {
    case 1: return r.attack.right ? (strike ? 143 : 141) : (strike ? 142 : 140);   // club
    case 2: return r.attack.right ? (strike ? 102 : 101) : (strike ? 106 : 105);   // chain
    case 3: return r.attack.right ? 60 : 66;                                        // kick
    default: return r.attack.right ? (strike ? 45 : 44) : (strike ? 52 : 51);      // fists
    }
}

int riding_frame(const rr::Bike& bike) {
    const int32_t steer = bike.i32(0x288), limit = 0x2e00;
    const int level = std::abs(steer) * 8 / limit;   // 0..8
    if (level < 1) return 3;
    if (steer > 0) return level < 3 ? 6 : (level < 6 ? 5 : 4);
    return level < 3 ? 8 : (level < 5 ? 9 : (level < 7 ? 10 : 11));
}

struct SpriteCache {
    std::vector<SDL_Texture*> textures;
    void clear() {
        for (auto* t : textures) SDL_DestroyTexture(t);
        textures.clear();
    }
    ~SpriteCache() { clear(); }
    SDL_Texture* get(SDL_Renderer* r, const Game& g, int frame, int rider) {
        if (textures.empty()) textures.assign(g.bobs.size() * g.remaps.tables.size(), nullptr);
        SDL_Texture*& t = textures.at(size_t(rider) * g.bobs.size() + size_t(frame));
        if (t) return t;
        const rr::Image8 img = rr::remap(g.bobs.at(size_t(frame)), g.remaps, rider);
        if (!img.width || !img.height) return nullptr;
        std::vector<uint8_t> rgba(size_t(img.width) * img.height * 4);
        for (size_t i = 0; i < img.pixels.size(); ++i) {
            const uint8_t* c = &g.palette.rgbx[4 * img.pixels[i]];
            rgba[4*i] = c[0]; rgba[4*i+1] = c[1]; rgba[4*i+2] = c[2]; rgba[4*i+3] = img.pixels[i] ? 255 : 0;  // index 0 transparent
        }
        SDL_Surface* s = SDL_CreateSurfaceFrom(img.width, img.height, SDL_PIXELFORMAT_RGBA32, rgba.data(), img.width * 4);
        t = s ? SDL_CreateTextureFromSurface(r, s) : nullptr;
        SDL_DestroySurface(s);
        if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        return t;
    }
};

// Player bike: original sprite, horizontal position projected at the bike's depth, bottom of the view.
void draw_bike(SDL_Renderer* r, const Game& g, SpriteCache& cache) {
    const rr::Race& race = *g.race;
    const int32_t depth = int32_t(race.position() - race.camera_position());
    const size_t index = size_t(depth & ~1) / 2;
    const int32_t scale = g.depth.scale[std::min(index, g.depth.scale.size() - 1)];
    const auto& road = race.route()[race.camera_position() >> 8];
    const int64_t rel = int64_t(race.lateral() - road.offset - race.camera().x);
    float x = float((rel * scale) >> 16) + 160.f;
    if (x < 20) x = 20;
    if (x > 300) x = 300;
    const auto& me = race.riders()[0];
    const int frame = race.crashed() ? 21 : me.attack.timer ? attack_frame(me) : riding_frame(race.bike());
    SDL_Texture* t = cache.get(r, g, frame, 0);
    if (!t) return;
    float w, h;
    SDL_GetTextureSize(t, &w, &h);
    const SDL_FRect dst{x * 2 - w / 2, 476 - h, w, h};
    SDL_RenderTexture(r, t, nullptr, &dst);
}

// Opponents: positioned on the projected road at their sample (interpolated within it) and scaled
// by depth relative to depth 512, capped at the player sprite's size and baseline so riders level
// with the player match it. The original sprite scale law, raster and sort order are plan 03 work.
void draw_opponents(SDL_Renderer* r, const Game& g, SpriteCache& cache, const std::vector<rr::RoadSlice>& slices) {
    const rr::Race& race = *g.race;
    struct Item { int32_t depth; float x, y, scale; int frame, colour; };
    std::vector<Item> items;
    for (size_t i = 1; i < race.riders().size(); ++i) {
        const rr::Rider& rd = race.riders()[i];
        const uint32_t pos = uint32_t(rd.bike.i32(0x1c));
        if (pos >> 8 < slices.front().sample || pos >> 8 >= slices.back().sample) continue;
        const size_t n = (pos >> 8) - slices.front().sample;
        const auto& a = slices[n];
        const auto& b = slices[n + 1];
        const auto& sample = race.route_of(rd)[pos >> 8];
        const int half = (a.count == 2 && b.count == 2) ? (rd.route == 1 ? 1 : 0) : 0;
        const int32_t lateral = (rd.bike.i32(0x28) >> 8) - sample.offset;
        const float f = float(pos & 0xff) / 256.f;
        const float xa = float(a.screen_x(half, lateral)) / 65536.f, xb = float(b.screen_x(half, lateral)) / 65536.f;
        const float ya = float(a.road[half].screen_y) / 65536.f, yb = float(b.road[half].screen_y) / 65536.f;
        const float scale = (float(a.scale) + (float(b.scale) - float(a.scale)) * f) / 32768.f;
        const int frame = rd.bike.i32(0x2f4) ? 21 : rd.attack.timer ? attack_frame(rd) : riding_frame(rd.bike);
        items.push_back({a.depth, xa + (xb - xa) * f, ya + (yb - ya) * f, scale, frame, rd.colour});
    }
    std::sort(items.begin(), items.end(), [](const Item& p, const Item& q) { return p.depth > q.depth; });
    const SDL_Rect clip{0, 0, 640, 480};
    SDL_SetRenderClipRect(r, &clip);
    for (const auto& it : items) {
        SDL_Texture* t = cache.get(r, g, it.frame, it.colour);
        if (!t) continue;
        float w, h;
        SDL_GetTextureSize(t, &w, &h);
        // Capped at the player sprite's size and baseline (see the comment above draw_opponents).
        const float s = std::min(it.scale, 1.f);
        const float base = std::min(it.y * 2, 476.f);
        const SDL_FRect dst{it.x * 2 - w * s / 2, base - h * s, w * s, h * s};
        SDL_RenderTexture(r, t, nullptr, &dst);
    }
    SDL_SetRenderClipRect(r, nullptr);
}

void draw_hud(SDL_Renderer* r, const Game& g) {
    const rr::Race& race = *g.race;
    char line[160];
    const int seconds = race.ticks() / rr::ticks_per_second;
    const int progress = int(uint64_t(race.position() >> 8) * 100 / (race.route().size() - 1));
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_snprintf(line, sizeof line, "%s  PLACE %d/%zu  SPEED %3d  GEAR %d  TIME %d:%02d  %3d%%", g.course_name.c_str(),
                 race.place(0), race.riders().size(), race.speed() / 100, race.gear() + 1, seconds / 60, seconds % 60, progress);
    SDL_RenderDebugText(r, 8, 8, line);
    if (g.countdown > 0) {
        SDL_snprintf(line, sizeof line, "GET READY  %d", g.countdown / rr::ticks_per_second + 1);
        SDL_SetRenderScale(r, 3, 3);
        SDL_RenderDebugText(r, 60, 60, line);
        SDL_SetRenderScale(r, 1, 1);
    } else if (race.finished()) {
        const int finish = race.riders()[0].finish_tick / rr::ticks_per_second;
        SDL_snprintf(line, sizeof line, "FINISHED %d%s  %d:%02d", race.place(0), race.place(0) == 1 ? "ST" : race.place(0) == 2 ? "ND" : race.place(0) == 3 ? "RD" : "TH", finish / 60, finish % 60);
        SDL_SetRenderScale(r, 3, 3);
        SDL_RenderDebugText(r, 50, 60, line);
        SDL_SetRenderScale(r, 1, 1);
        SDL_RenderDebugText(r, 200, 260, "Enter / A / tap: race again");
    } else if (race.crashed()) {
        SDL_SetRenderScale(r, 3, 3);
        SDL_RenderDebugText(r, 80, 60, "WIPEOUT!");
        SDL_SetRenderScale(r, 1, 1);
    }
    // Stamina bars: player and the rider it last hit.
    auto bar = [&](float y, const char* label, const rr::RiderStats& st) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderDebugText(r, 8, y, label);
        const SDL_FRect back{72, y - 1, 120, 9}, fill{72, y - 1, 120.f * float(st.current) / float(std::max(1, st.base)), 9};
        SDL_SetRenderDrawColor(r, 40, 40, 40, 255); SDL_RenderFillRect(r, &back);
        SDL_SetRenderDrawColor(r, 220, 60, 40, 255); SDL_RenderFillRect(r, &fill);
    };
    bar(24, "YOU", race.riders()[0].stamina);
    if (race.last_target() > 0) bar(36, "THEM", race.riders()[size_t(race.last_target())].stamina);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDebugText(r, 8, 483, "Up/Down throttle/brake, Left/Right steer, Z punch, X kick, C backhand");
    SDL_RenderDebugText(r, 8, 497, "P/Esc/Back/Start pauses. Enter/Start/tap resumes. Q quits");
}

bool lifecycle_event(Uint32 type) {
    return type == SDL_EVENT_WILL_ENTER_BACKGROUND || type == SDL_EVENT_DID_ENTER_BACKGROUND ||
           type == SDL_EVENT_WILL_ENTER_FOREGROUND || type == SDL_EVENT_DID_ENTER_FOREGROUND;
}

struct LifecycleWatch {
    rr::SessionState state;
    rr::Mixer* mixer = nullptr;
    static bool SDLCALL watch(void* data, SDL_Event* event) {
        if (lifecycle_event(event->type)) {
            auto& self=*static_cast<LifecycleWatch*>(data);
            self.state.handle(*event);
            if (self.mixer) self.mixer->pause(self.state.paused());
        }
        return true;
    }
    explicit LifecycleWatch(rr::Mixer* audio=nullptr):mixer(audio) {
        if (!SDL_AddEventWatch(watch, this)) throw std::runtime_error(SDL_GetError());
    }
    ~LifecycleWatch() { SDL_RemoveEventWatch(watch, this); }
};

void draw_pause(SDL_Renderer* r) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 190);
    SDL_RenderFillRect(r, nullptr);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_SetRenderScale(r, 3, 3);
    SDL_RenderDebugText(r, 80, 65, "PAUSED");
    SDL_SetRenderScale(r, 1, 1);
    SDL_RenderDebugText(r, 184, 260, "Enter / Start / tap to resume");
    SDL_RenderDebugText(r, 232, 284, "Q to quit on desktop");
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void play_video(const rr::Assets& assets,const std::string& path,SDL_Renderer* renderer,
                const std::string& screenshot,long ticks) {
    rr::Mixer mixer(32);
    if (ticks<0 && SDL_InitSubSystem(SDL_INIT_AUDIO) && !mixer.open_device()) SDL_Log("Audio unavailable: %s",SDL_GetError());
    rr::VideoPlayer player(assets,path,mixer);
    LifecycleWatch lifecycle(&mixer);
    std::unique_ptr<SDL_Gamepad,decltype(&SDL_CloseGamepad)> pad(nullptr,SDL_CloseGamepad);
    bool running=true;
    player.update(ticks>=0 ? 0 : SDL_GetTicksNS());
    if (ticks>=0) player.update(uint64_t(ticks)*1000000000ull/rr::ticks_per_second);
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (!lifecycle_event(e.type)) lifecycle.state.handle(e);
            if (!mixer.handle(e)) SDL_Log("Audio reopen failed: %s",SDL_GetError());
            if (e.type==SDL_EVENT_QUIT) running=false;
            if (e.type==SDL_EVENT_GAMEPAD_ADDED && !pad) pad.reset(SDL_OpenGamepad(e.gdevice.which));
            if (e.type==SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad.get())==e.gdevice.which) {
                pad.reset();lifecycle.state.pause();
            }
            if (e.type==SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                if (e.key.key==SDLK_Q) running=false;
                else if (e.key.key==SDLK_P || e.key.key==SDLK_ESCAPE) {
                    if (lifecycle.state.paused()) lifecycle.state.resume();else lifecycle.state.pause();
                } else if (e.key.key==SDLK_AC_BACK) lifecycle.state.pause();
                else if (lifecycle.state.paused()) lifecycle.state.resume();else player.skip();
            }
            if (e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN && pad && SDL_GetGamepadID(pad.get())==e.gbutton.which) {
                if (e.gbutton.button==SDL_GAMEPAD_BUTTON_START) {
                    if (lifecycle.state.paused()) lifecycle.state.resume();else lifecycle.state.pause();
                } else if (lifecycle.state.paused()) lifecycle.state.resume();else player.skip();
            }
            if (e.type==SDL_EVENT_FINGER_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (lifecycle.state.paused()) lifecycle.state.resume();else player.skip();
            }
            if (e.type==SDL_EVENT_RENDER_DEVICE_RESET || e.type==SDL_EVENT_RENDER_TARGETS_RESET) {
                // Recreate the video texture through a separate explicit reset.
                player.reset_texture();
            }
            if (e.type==SDL_EVENT_RENDER_DEVICE_LOST) throw std::runtime_error("Render device lost");
        }
        player.pause(lifecycle.state.paused());
        if (ticks<0 && !player.update(SDL_GetTicksNS())) running=false;
        if (lifecycle.state.background()) { SDL_Delay(20);continue; }
        SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);player.draw(renderer);
        if (lifecycle.state.paused()) draw_pause(renderer);
        if (!screenshot.empty()) {
            SDL_Surface* capture=SDL_RenderReadPixels(renderer,nullptr);
            if (!capture || !SDL_SaveBMP(capture,screenshot.c_str())) throw std::runtime_error(SDL_GetError());
            SDL_DestroySurface(capture);running=false;
        }
        SDL_RenderPresent(renderer);
        if (ticks>=0) running=false;
    }
}
}

int main(int argc, char** argv) {
    std::string root, screenshot, video;
    int course = 0, bike = 0, level = 0, opponents = 7;
    long run_ticks = -1;
    bool auto_drive = false, start_paused = false, first_run_smoke = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--assets" && i + 1 < argc) root = argv[++i];
        else if (a == "--video" && i + 1 < argc) video = argv[++i];
        else if (a == "--course" && i + 1 < argc) course = std::atoi(argv[++i]);
        else if (a == "--bike" && i + 1 < argc) bike = std::atoi(argv[++i]);
        else if (a == "--ticks" && i + 1 < argc) run_ticks = std::atol(argv[++i]);
        else if (a == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
        else if (a == "--autopilot") auto_drive = true;
        else if (a == "--paused") start_paused = true;
        else if (a == "--first-run-smoke") first_run_smoke = true;
        else if (a == "--level" && i + 1 < argc) level = std::atoi(argv[++i]) - 1;
        else if (a == "--opponents" && i + 1 < argc) opponents = std::atoi(argv[++i]);
        else { std::fprintf(stderr, "Usage: rr-game [--assets DIR] [--course 0-4] [--bike 0-14] [--level 1-5] [--opponents 0-7] [--autopilot] [--paused] [--ticks N --screenshot FILE.bmp]\n"); return 2; }
    }
    if (course < 0 || course > 4 || level < 0 || level > 4 || opponents < 0 || opponents > 7) return 2;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int result = 0;
    try {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) throw std::runtime_error(SDL_GetError());
        if (!SDL_CreateWindowAndRenderer("Road Rash Native", 960, 760, SDL_WINDOW_RESIZABLE, &window, &renderer))
            throw std::runtime_error(SDL_GetError());
        SDL_SetRenderLogicalPresentation(renderer, 640, 510, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderVSync(renderer, 1);
        if (root.empty()) {
            root = rr::data_root();
#ifdef RR_DEV_ASSETS
            if (!rr::Assets(root).exists("ROADRASH.EXE")) root = "game";
#endif
        }
        if (!rr::Assets(root).exists("ROADRASH.EXE")) {
            if (!rr::run_import_screen(window, renderer, root, screenshot, first_run_smoke)) {
                SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
                return 0;
            }
        }
        const rr::Assets assets(root);
        if (!video.empty()) {
            play_video(assets,video,renderer,screenshot,run_ticks);
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
        }
        Game g;
        const auto exe = assets.read("ROADRASH.EXE");
        g.depth = rr::read_depth_table(exe);
        g.physics = rr::read_physics_tables(exe);
        g.specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        g.course_name = course_names[course];
        g.course = rr::read_course(assets.read("DATA/COURSES/" + g.course_name + ".CRS"));
        if (bike < 0 || size_t(bike) >= g.specs.size()) throw std::runtime_error("Bike index out of range");
        g.bike = bike;
        g.bobs = rr::read_bobs(assets.read("DATA/BIKERS/HIBOB.DAT"), rr::bob_frames);
        g.remaps = rr::read_rider_remaps(exe, 9);
        g.palette = rr::Palette::from_raw(assets.read("DATA/PALETTE.RAW"));
        g.level = level;
        g.opponents = rr::original_opponents(exe, g.specs, level, opponents);
        g.restart();

        if (run_ticks >= 0) {   // headless-style verification run
            for (long t = 0; t < run_ticks && !g.race->finished(); ++t) g.tick(auto_drive ? autopilot(*g.race) : 0);
        }
        SpriteCache sprites;
        LifecycleWatch lifecycle;
        if (start_paused) lifecycle.state.pause();
        rr::TouchInput touch;
        rr::TickClock clock;
        SDL_Gamepad* pad = nullptr;
        bool running = true;
        uint64_t last = SDL_GetTicksNS();
        while (running) {
            SDL_Event e;
            bool restart = false;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                // Android lifecycle state is set immediately by the event watch;
                // do not replay older queued lifecycle events here.
                if (!lifecycle_event(e.type)) lifecycle.state.handle(e);
                if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                    if (e.key.key == SDLK_Q) running = false;
                    if (e.key.key == SDLK_AC_BACK) lifecycle.state.pause();
                    if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_P) {
                        if (lifecycle.state.paused()) lifecycle.state.resume(); else lifecycle.state.pause();
                    }
                    if (e.key.key == SDLK_RETURN) {
                        if (lifecycle.state.paused()) lifecycle.state.resume();
                        else if (g.race->finished()) restart = true;
                    }
                }
                if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) pad = SDL_OpenGamepad(e.gdevice.which);
                if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad) == e.gdevice.which) {
                    SDL_CloseGamepad(pad); pad = nullptr;
                    lifecycle.state.pause();
                }
                if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && pad && SDL_GetGamepadID(pad) == e.gbutton.which) {
                    if (e.gbutton.button == SDL_GAMEPAD_BUTTON_START) {
                        if (lifecycle.state.paused()) lifecycle.state.resume(); else lifecycle.state.pause();
                    } else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH && !lifecycle.state.paused() && g.race->finished()) restart = true;
                }
                if (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION ||
                    e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED) {
                    if (lifecycle.state.paused()) {
                        if (e.type == SDL_EVENT_FINGER_DOWN) lifecycle.state.resume();
                    } else if (e.type == SDL_EVENT_FINGER_DOWN && g.race->finished()) restart = true;
                    else touch.handle(e);
                }
                if (e.type == SDL_EVENT_RENDER_DEVICE_RESET || e.type == SDL_EVENT_RENDER_TARGETS_RESET) sprites.clear();
                if (e.type == SDL_EVENT_RENDER_DEVICE_LOST) throw std::runtime_error("Render device lost");
                if (lifecycle.state.take_clock_reset()) {
                    touch.clear(); clock.reset(); last = SDL_GetTicksNS();
                }
            }
            if (!running) break;
            if (restart) g.restart();
            rr::PlayerInput in = touch.current();
            const bool* keys = SDL_GetKeyboardState(nullptr);
            in.accelerate |= keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
            in.brake |= keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
            in.left |= keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
            in.right |= keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
            in.punch |= keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_LCTRL];
            in.kick |= keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_LALT];
            in.backhand |= keys[SDL_SCANCODE_C];
            if (pad) {
                in.accelerate |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) || SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000;
                in.brake |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000;
                in.left |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
                in.right |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
                in.steer_axis = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
                in.punch |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST);
                in.kick |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH);
                in.backhand |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
            }
            const uint32_t controls = auto_drive ? autopilot(*g.race) : rr::controls_from(in);

            const uint64_t now = SDL_GetTicksNS();
            if (lifecycle.state.take_clock_reset()) { touch.clear(); clock.reset(); last = now; }
            const int steps = clock.advance(now - last, lifecycle.state.paused());
            last = now;
            for (int step = 0; step < steps && !lifecycle.state.paused(); ++step) g.tick(controls);
            if (lifecycle.state.background()) { SDL_Delay(20); continue; }

            SDL_SetRenderDrawColor(renderer, 15, 18, 22, 255);
            SDL_RenderClear(renderer);
            const auto slices = rr::project_road(g.race->route(), g.race->camera_position(), g.race->camera(), g.depth);
            rr::draw_road(renderer, slices);
            draw_opponents(renderer, g, sprites, slices);
            draw_bike(renderer, g, sprites);
            draw_hud(renderer, g);
            if (lifecycle.state.paused()) draw_pause(renderer);
            if (!screenshot.empty()) {
                SDL_Surface* capture = SDL_RenderReadPixels(renderer, nullptr);
                if (!capture || !SDL_SaveBMP(capture, screenshot.c_str())) throw std::runtime_error(SDL_GetError());
                SDL_DestroySurface(capture);
                running = false;
            }
            SDL_RenderPresent(renderer);
        }
        if (pad) SDL_CloseGamepad(pad);
    } catch (const std::exception& e) { SDL_Log("%s", e.what()); result = 1; }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
