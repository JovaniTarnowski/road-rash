// Shared native/Android frontend and playable race.
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
#include "career.hpp"
#include "course_scene.hpp"
#include "hud.hpp"
#include "exe_tables.hpp"
#include "frontend.hpp"
#include "pe_strings.hpp"
#include "settings.hpp"
#include "sound.hpp"
#include "text.hpp"
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

namespace {
const char* const course_names[] = {"NAPA", "CITY", "HIWAY", "CANYN", "MEDLY"};
constexpr int countdown_ticks = 3 * rr::ticks_per_second;

SDL_Gamepad* open_first_gamepad() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    SDL_Gamepad* pad = nullptr;
    for (int i = 0; ids && i < count && !pad; ++i) pad = SDL_OpenGamepad(ids[i]);
    SDL_free(ids);
    return pad;
}

struct Game {
    rr::DepthTable depth;
    rr::PhysicsTables physics;
    std::vector<rr::BikeSpec> specs;
    rr::Course course;
    std::unique_ptr<rr::CourseScene> scene;
    std::unique_ptr<rr::Hud> hud;
    bool metric=false;
    std::string player_name="Player";
    std::vector<std::string> rider_names;
    std::string course_name;
    int bike = 0;
    std::unique_ptr<rr::Race> race;
    int countdown = countdown_ticks;
    std::vector<rr::Image8> bobs;
    rr::RiderRemaps remaps;
    rr::Palette palette;
    std::vector<rr::Opponent> opponents;
    int level = 0;
    int32_t rider_value = 90;   // rider table +0xc / 2 for the player's rider
    bool career = false;        // race ends after the finish delay instead of restarting
    std::string quit_prompt;    // string 316, shown after Escape in a career race
    bool quitting = false;
    rr::SoundSystem* sound = nullptr;   // race effects and engines (0x00434980)
    rr::Mixer* mixer = nullptr;

    void load_course(const rr::Assets& assets,const std::vector<uint8_t>& exe) {
        scene.reset();
        hud.reset();
        course=rr::read_course(assets.read("DATA/COURSES/"+course_name+".CRS"));
        if(course_name=="NAPA") {
            scene=std::make_unique<rr::CourseScene>(assets,exe,course,course_name,1,level);
            hud=std::make_unique<rr::Hud>(assets,exe,bike,metric);
        }
    }

    void restart() {
        race = std::make_unique<rr::Race>(rr::make_route(course, false), rr::make_route(course, true),
                                          physics, specs.at(size_t(bike)), rider_value, opponents, level);
        if(scene)race->set_terrain_source([this](const rr::RouteSample& sample)->const rr::TerrainSlice&{return scene->terrain(sample.section,sample.local);});
        countdown = countdown_ticks;
    }
    void tick(uint32_t controls) {
        if (countdown > 0) --countdown;   // race clock starts after the countdown
        else race->step(controls);
        if (sound) {
            for (const auto& e : race->take_sound_events()) sound->post(e);
            sound->update(*race);
        }
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

struct CourseCanvas {
    rr::Framebuffer frame{640,480};
    std::vector<uint8_t> rgba=std::vector<uint8_t>(640*480*4);
    SDL_Texture* texture=nullptr;
    SDL_Texture* dashboard=nullptr;
    ~CourseCanvas(){clear();}
    void clear(){SDL_DestroyTexture(texture);texture=nullptr;SDL_DestroyTexture(dashboard);dashboard=nullptr;}
    void draw_dashboard(SDL_Renderer* renderer,const Game& game) {
        const int target=game.race->last_target();
        const auto image=game.hud->compose(game.hud->state(*game.race,game.player_name,
            target>0 && size_t(target)<game.rider_names.size()?game.rider_names[size_t(target)]:std::string()),game.physics);
        for(size_t i=0;i<image.pixels.size();++i){const uint32_t c=image.pixels[i];rgba[4*i]=uint8_t(c>>16);rgba[4*i+1]=uint8_t(c>>8);rgba[4*i+2]=uint8_t(c);rgba[4*i+3]=c==0x01000000?0:255;}
        if(!dashboard) {
            dashboard=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,640,480);
            if(!dashboard)throw std::runtime_error(SDL_GetError());
            SDL_SetTextureScaleMode(dashboard,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(dashboard,SDL_BLENDMODE_BLEND);
        }
        if(!SDL_UpdateTexture(dashboard,nullptr,rgba.data(),640*4))throw std::runtime_error(SDL_GetError());
        const SDL_FRect dst{0,0,640,480};SDL_RenderTexture(renderer,dashboard,nullptr,&dst);
    }
    void draw(SDL_Renderer* renderer,const Game& game,const std::vector<rr::RoadSlice>& slices) {
        game.scene->draw_horizon(frame,game.race->horizon_scroll());
        rr::render_nodes(frame,game.scene->draw_nodes(game.race->route(),slices,game.race->camera(),game.depth),rr::view_mode(true),16);
        frame.to_rgba(game.palette,rgba.data(),640*4);
        if(!texture) {
            texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,640,480);
            if(!texture)throw std::runtime_error(SDL_GetError());
            SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
        }
        if(!SDL_UpdateTexture(texture,nullptr,rgba.data(),640*4))throw std::runtime_error(SDL_GetError());
        const SDL_FRect dst{0,0,640,480};
        if(!SDL_RenderTexture(renderer,texture,nullptr,&dst))throw std::runtime_error(SDL_GetError());
    }
};

// Player bike: original sprite, horizontal position projected at the bike's depth, bottom of the view.
void draw_bike(SDL_Renderer* r, const Game& g, SpriteCache& cache,const std::vector<rr::RoadSlice>& slices) {
    const rr::Race& race = *g.race;
    const auto& me = race.riders()[0];
    const int frame = race.crashed() ? 21 : me.attack.timer ? attack_frame(me) : riding_frame(race.bike());
    SDL_Texture* t = cache.get(r, g, frame, 0);
    if (!t) return;
    float w, h;
    SDL_GetTextureSize(t, &w, &h);
    const uint32_t sample=race.position()>>8;
    if(slices.size()<2 || sample<slices.front().sample || sample>=slices.back().sample)return;
    const size_t at=sample-slices.front().sample;const auto& a=slices[at];const auto& b=slices[at+1];
    const float fraction=float(race.position()&255)/256;
    const auto& pa=a.road[a.own];const auto& pb=b.road[b.own];
    const float base=(float(pa.screen_y)+(float(pb.screen_y)-float(pa.screen_y))*fraction)/32768;
    const int lateral=race.lateral()-race.route()[sample].offset;
    const float x=(float(a.screen_x(a.own,lateral))+(float(b.screen_x(b.own,lateral))-float(a.screen_x(a.own,lateral)))*fraction)/65536;
    const float size=(float(a.scale)+(float(b.scale)-float(a.scale))*fraction)/32768;
    const SDL_FRect dst{x * 2 - w*size / 2, base-h*size, w*size, h*size};
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
    if(!g.hud)SDL_RenderDebugText(r, 8, 8, line);
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
        if (!g.career) SDL_RenderDebugText(r, 200, 260, "Enter / A / tap: race again");
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
    if(!g.hud) {
        bar(24, "YOU", race.riders()[0].stamina);
        if (race.last_target() > 0) bar(36, "THEM", race.riders()[size_t(race.last_target())].stamina);
    }
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
    std::unique_ptr<SDL_Gamepad,decltype(&SDL_CloseGamepad)> pad(open_first_gamepad(),SDL_CloseGamepad);
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

// Original course files by course number 1..5 (0x0046df58): City, Sierra Nevada, Peninsula, Napa, Pacific Highway.
const char* const original_course_files[5] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};
constexpr int race_end_delay = 0xb4;   // 0x00416010: ticks the race keeps running after it is decided

struct RaceEnd {
    bool quit_app = false;
    rr::RaceOutcome outcome = rr::RaceOutcome::lost;
    int place = 0;   // 0-based
};

struct RaceOptions {
    bool auto_drive = false, start_paused = false;
    std::string screenshot;
};

// One race: the playable loop. In career mode the race is decided by the player finishing
// (0x00434660: code 0x7f), wrecking or quitting (code 5), and runs on for 0xb4 ticks.
RaceEnd run_race(Game& g, SDL_Renderer* renderer, const RaceOptions& opt) {
    RaceEnd result;
    SpriteCache sprites;
    CourseCanvas course_canvas;
    LifecycleWatch lifecycle(g.mixer);
    if (opt.start_paused) lifecycle.state.pause();
    rr::TouchInput touch;
    rr::TickClock clock;
    SDL_Gamepad* pad = open_first_gamepad();
    bool running = true, decided = false;
    int end_ticks = 0;
    uint64_t last = SDL_GetTicksNS();
    SDL_SetRenderLogicalPresentation(renderer, 640, 510, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    while (running) {
        SDL_Event e;
        bool restart = false;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; result.quit_app = true; }
            if (!lifecycle_event(e.type)) lifecycle.state.handle(e);
            if (g.mixer && !g.mixer->handle(e)) SDL_Log("Audio reopen failed: %s", SDL_GetError());
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                if (g.quitting) {
                    // 0x004227d0: a second Escape quits the race (outcome 5), any other key continues.
                    g.quitting = false;
                    if (e.key.key == SDLK_ESCAPE) {
                        if (!decided) { result.outcome = rr::RaceOutcome::wrecked; result.place = std::max(0, g.race->place(0) - 1); }
                        running = false;
                    }
                    continue;
                }
                if (e.key.key == SDLK_Q && !g.career) { running = false; result.quit_app = true; }
                if (e.key.key == SDLK_AC_BACK) lifecycle.state.pause();
                if (e.key.key == SDLK_ESCAPE && g.career && !lifecycle.state.paused()) g.quitting = true;
                else if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_P) {
                    if (lifecycle.state.paused()) lifecycle.state.resume(); else lifecycle.state.pause();
                }
                if (e.key.key == SDLK_RETURN) {
                    if (lifecycle.state.paused()) lifecycle.state.resume();
                    else if (g.race->finished() && !g.career) restart = true;
                }
            }
            if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) pad = SDL_OpenGamepad(e.gdevice.which);
            if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad) == e.gdevice.which) {
                SDL_CloseGamepad(pad); pad = nullptr;
                lifecycle.state.pause();
            }
            if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && pad && SDL_GetGamepadID(pad) == e.gbutton.which) {
                if (g.quitting) {
                    g.quitting = false;
                    if (e.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                        if (!decided) { result.outcome = rr::RaceOutcome::wrecked; result.place = std::max(0, g.race->place(0) - 1); }
                        running = false;
                    }
                } else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_START) {
                    if (lifecycle.state.paused()) lifecycle.state.resume(); else lifecycle.state.pause();
                } else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_BACK && g.career && !lifecycle.state.paused()) g.quitting = true;
                else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH && !lifecycle.state.paused() && g.race->finished() && !g.career) restart = true;
            }
            if (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION ||
                e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED) {
                if (lifecycle.state.paused()) {
                    if (e.type == SDL_EVENT_FINGER_DOWN) lifecycle.state.resume();
                } else if (e.type == SDL_EVENT_FINGER_DOWN && g.race->finished() && !g.career) restart = true;
                else touch.handle(e);
            }
            if (e.type == SDL_EVENT_RENDER_DEVICE_RESET || e.type == SDL_EVENT_RENDER_TARGETS_RESET) {sprites.clear();course_canvas.clear();}
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
        const uint32_t controls = opt.auto_drive ? autopilot(*g.race) : rr::controls_from(in);

        const uint64_t now = SDL_GetTicksNS();
        if (lifecycle.state.take_clock_reset()) { touch.clear(); clock.reset(); last = now; }
        const bool frozen = lifecycle.state.paused() || g.quitting;
        if (g.mixer) g.mixer->pause(frozen || lifecycle.state.background());
        const int steps = clock.advance(now - last, frozen);
        last = now;
        for (int step = 0; step < steps && !frozen; ++step) {
            g.tick(controls);
            if (!g.career) continue;
            if (!decided && (g.race->finished() || g.race->wrecked())) {
                decided = true;
                end_ticks = race_end_delay;
                result.place = std::max(0, g.race->place(0) - 1);
                // 0x00416010: finished becomes won for places 1-3 (0-based < 3), lost otherwise.
                result.outcome = g.race->finished() ? (result.place < 3 ? rr::RaceOutcome::won : rr::RaceOutcome::lost)
                                                    : rr::RaceOutcome::wrecked;
            } else if (decided && --end_ticks <= 0) running = false;
        }
        if (lifecycle.state.background()) { SDL_Delay(20); continue; }

        SDL_SetRenderDrawColor(renderer, 15, 18, 22, 255);
        SDL_RenderClear(renderer);
        const auto slices = rr::project_road(g.race->route(), g.race->camera_position(), g.race->camera(), g.depth);
        if(g.scene)course_canvas.draw(renderer,g,slices);
        else rr::draw_road(renderer, slices);
        draw_opponents(renderer, g, sprites, slices);
        draw_bike(renderer, g, sprites,slices);
        if(g.hud)course_canvas.draw_dashboard(renderer,g);
        draw_hud(renderer, g);
        if (g.quitting) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
            SDL_RenderFillRect(renderer, nullptr);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            float y = 200;
            size_t from = 0;
            while (from <= g.quit_prompt.size()) {
                size_t to = g.quit_prompt.find('\n', from);
                if (to == std::string::npos) to = g.quit_prompt.size();
                const std::string line = rr::cp1252_to_utf8(g.quit_prompt.substr(from, to - from));
                SDL_RenderDebugText(renderer, 320 - 4.f * float(line.size()), y, line.c_str());
                y += 14;
                from = to + 1;
            }
        } else if (lifecycle.state.paused()) draw_pause(renderer);
        if (!opt.screenshot.empty()) {
            SDL_Surface* capture = SDL_RenderReadPixels(renderer, nullptr);
            if (!capture || !SDL_SaveBMP(capture, opt.screenshot.c_str())) throw std::runtime_error(SDL_GetError());
            SDL_DestroySurface(capture);
            running = false;
            result.quit_app = true;
        }
        SDL_RenderPresent(renderer);
    }
    if (pad) SDL_CloseGamepad(pad);
    if (g.mixer) g.mixer->pause(false);
    return result;
}

std::string pref_directory() {
    if (const char* o = SDL_getenv("RR_PREF_OVERRIDE")) return o;
    char* p = SDL_GetPrefPath("RoadRashNative", "RoadRash");
    if (!p) throw std::runtime_error(SDL_GetError());
    std::string s = p;
    SDL_free(p);
    return s;
}

// Menus -> videos -> races -> results: the original main loop (0x00448200) around the front end.
int run_game(const rr::Assets& assets, SDL_Window* window, SDL_Renderer* renderer, Game& g, const std::vector<uint8_t>& exe,
             const std::string& screenshot, long menu_frames) {
    const std::string pref = pref_directory();
    const std::string settings_path = (std::filesystem::path(pref) / "settings.txt").string();
    rr::Settings settings = rr::load_settings(settings_path);
    const rr::TextRenderer text(assets);
    const auto tables = rr::read_career_tables(exe);
    rr::Career career;
    const auto resources = rr::read_string_resources(assets.read(rr::language_dll(settings.language)));
    const auto strings = rr::frontend_strings(resources);
    rr::Frontend fe(assets, text, strings, rr::credit_lines(resources), rr::read_frontend_data(exe), tables, career, settings,
                    (std::filesystem::path(pref) / "saves").string());
    // Opponent names: personality +0x165 -> 0x00464188 -> string slot (0x00437860).
    const auto name_table = rr::read_exe_data(exe, 0x00464188, 9 * 4);
    auto personality_name = [&](int p) {
        const uint32_t slot = p >= 0 && p < 9 ? uint32_t(name_table[size_t(p) * 4]) | uint32_t(name_table[size_t(p) * 4 + 1]) << 8 : 0;
        return slot ? fe.string_slot(int(slot)) : std::string();
    };
    // Audio: one mixer for the soundtrack (0x00403040) and race sounds (0x00435da0).
    rr::Mixer mixer(48);
    if (menu_frames < 0 && (!SDL_InitSubSystem(SDL_INIT_AUDIO) || !mixer.open_device()))
        SDL_Log("Audio unavailable: %s", SDL_GetError());
    std::unique_ptr<rr::SoundSystem> sound;
    try {
        sound = std::make_unique<rr::SoundSystem>(assets, mixer, rr::read_volume_curve(exe), int(rr::read_exe_data(exe, 0x0046dce5, 1)[0]));
    } catch (const std::exception& e) { SDL_Log("Race sounds unavailable: %s", e.what()); }
    rr::MusicPlayer music(assets, mixer);
    auto apply_levels = [&] {
        rr::SoundLevels l;
        l.master = settings.master_volume; l.effects = settings.effects_volume; l.engine = settings.engine_volume;
        l.others = settings.others_volume; l.siren = settings.siren_volume; l.music = settings.music_volume;
        l.interface = settings.interface_volume; l.mute = settings.mute; l.swap_speakers = settings.swap_speakers;
        if (sound) sound->set_levels(l);
        music.set_volume(settings.mute ? 0 : 127 * settings.music_volume / 100 * settings.master_volume / 100);
    };
    apply_levels();
    if (menu_frames < 0) music.play(settings.music_track);
    g.mixer = &mixer;
    LifecycleWatch audio_lifecycle(&mixer);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
    if (!texture) throw std::runtime_error(SDL_GetError());
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_Gamepad* pad = open_first_gamepad();
    rr::MenuStickInput menu_stick;
    LifecycleWatch lifecycle;
    rr::RgbImage frame;
    fe.start();
    bool running = true, text_input = false;
    long frames = 0;
    while (running) {
        rr::FrontendEvent ev;
        while (running && fe.poll(ev)) {
            if (ev.kind == rr::FrontendEvent::Kind::quit) running = false;
            else if (ev.kind == rr::FrontendEvent::Kind::play_video) {
                if (menu_frames < 0) {
                    // 0x004023e0: showroom clips (flag 1) only quieten the soundtrack; other videos stop it.
                    const bool showroom = ev.video.find("/RAT/") != std::string::npos || ev.video.find("/SPORT/") != std::string::npos ||
                                          ev.video.find("/SUPER/") != std::string::npos;
                    if (showroom) music.set_volume(0); else music.stop();
                    play_video(assets, ev.video, renderer, {}, -1);
                    menu_stick.suspend();
                    apply_levels();
                    if (!showroom && !music.active()) music.play(music.track());
                }
            } else if (ev.kind == rr::FrontendEvent::Kind::start_race) {
                // Race set-up from the career: course, level, bike, rider weight, opponents (plan 05).
                g.course_name = original_course_files[ev.course - 1];
                g.level = career.state.level;
                g.bike = career.state.bike;
                g.metric=settings.metric;g.player_name=fe.player_name();
                g.rider_names.clear();g.rider_names.push_back(g.player_name);
                for(int i=1;i<8;++i)g.rider_names.push_back(personality_name(i));
                g.load_course(assets,exe);
                const auto body = rr::read_exe_data(exe, 0x00463c78 + uint32_t(rr::character_rider(career.state.character)) * 0x40 + 0xc, 4);
                g.rider_value = int32_t(uint32_t(body[0]) | uint32_t(body[1]) << 8) / 2;
                g.opponents = rr::original_opponents(exe, g.specs, g.level, 7);
                g.career = true;
                g.quit_prompt = fe.string_slot(316);
                g.restart();
                if (menu_frames >= 0) { running = false; break; }   // smoke run: stop at the first race
                music.stop();                                         // 0x004160c0 -> 0x00402f50
                g.sound = sound.get();
                if (sound) sound->start_race(career.state.bike, career.state.character);
                const RaceEnd end = run_race(g, renderer, {});
                menu_stick.suspend();
                if (sound) sound->stop_race();
                g.sound = nullptr;
                settings.music_track = music.track();
                music.play(music.track());
                SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);
                if (end.quit_app) { running = false; break; }
                // Finishing order: riders who finished, by finish tick (0x00420e30 rows).
                std::vector<std::pair<int32_t, size_t>> order;
                for (size_t i = 0; i < g.race->riders().size(); ++i)
                    if (g.race->riders()[i].finished) order.emplace_back(g.race->riders()[i].finish_tick, i);
                std::sort(order.begin(), order.end());
                std::vector<rr::Standing> standings;
                for (const auto& [tick, i] : order)
                    standings.push_back({i == 0 ? fe.player_name() : personality_name(int(i)), tick * 100 / rr::ticks_per_second / 10, i == 0});
                fe.race_finished(end.outcome, end.place, standings);
                rr::save_settings(settings_path, settings);
            }
        }
        if (!running) break;
        if (fe.editing_text() != text_input) {
            text_input = fe.editing_text();
            if (text_input) SDL_StartTextInput(window); else SDL_StopTextInput(window);
        }
        rr::MenuInput in;
        // Nested race/video loops may have consumed the removal notification.
        if (pad && !SDL_GamepadConnected(pad)) {
            SDL_CloseGamepad(pad); pad = open_first_gamepad();
            menu_stick.suspend();
            lifecycle.state.pause();
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (!lifecycle_event(e.type)) lifecycle.state.handle(e);
            if (!mixer.handle(e)) SDL_Log("Audio reopen failed: %s", SDL_GetError());
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                case SDLK_LEFT: in.left = true; break;
                case SDLK_RIGHT: in.right = true; break;
                case SDLK_UP: in.up = true; break;
                case SDLK_DOWN: in.down = true; break;
                case SDLK_TAB: (e.key.mod & SDL_KMOD_SHIFT ? in.shift_tab : in.tab) = true; break;
                case SDLK_HOME: in.home = true; break;
                case SDLK_END: in.end = true; break;
                case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: if (!e.key.repeat && !(text_input && e.key.key == SDLK_SPACE)) in.select = true; break;
                case SDLK_ESCAPE: case SDLK_AC_BACK: if (!e.key.repeat) in.back = true; break;
                case SDLK_BACKSPACE: in.backspace = true; break;
                case SDLK_A: if (!text_input && !e.key.repeat) in.alternate_artwork = true; break;
                case SDLK_KP_PLUS: case SDLK_EQUALS: if (!text_input) in.text += '+'; break;
                case SDLK_KP_MINUS: case SDLK_MINUS: if (!text_input) in.text += '-'; break;
                default: break;
                }
            }
            if (e.type == SDL_EVENT_TEXT_INPUT && text_input) {
                // UTF-8 -> Windows-1252 for the alias.
                std::u16string u;
                const unsigned char* t = reinterpret_cast<const unsigned char*>(e.text.text);
                for (size_t i = 0; t[i];) {
                    uint32_t c = t[i];
                    int n = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
                    if (n > 1) { c &= 0x3f >> (n - 1); for (int k = 1; k < n && t[i + size_t(k)]; ++k) c = c << 6 | (t[i + size_t(k)] & 0x3f); }
                    u.push_back(c < 0x10000 ? char16_t(c) : u'?');
                    i += size_t(n);
                }
                in.text += rr::to_cp1252(u);
            }
            if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) pad = SDL_OpenGamepad(e.gdevice.which);
            if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad) == e.gdevice.which) {
                SDL_CloseGamepad(pad); pad = open_first_gamepad();
                menu_stick.suspend();
                lifecycle.state.pause();
            }
            if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && pad && SDL_GetGamepadID(pad) == e.gbutton.which) {
                switch (e.gbutton.button) {
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT: in.left = true; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: in.right = true; break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP: in.up = true; break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN: in.down = true; break;
                case SDL_GAMEPAD_BUTTON_SOUTH: case SDL_GAMEPAD_BUTTON_START: in.select = true; break;
                case SDL_GAMEPAD_BUTTON_EAST: case SDL_GAMEPAD_BUTTON_BACK: in.back = true; break;
                default: break;
                }
            }
            if (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_FINGER_DOWN) {
                float x = 0, y = 0;
                if (e.type == SDL_EVENT_FINGER_DOWN) {
                    int w = 0, h = 0;
                    SDL_GetWindowSize(window, &w, &h);
                    SDL_RenderCoordinatesFromWindow(renderer, e.tfinger.x * float(w), e.tfinger.y * float(h), &x, &y);
                } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                    if (e.motion.which == SDL_TOUCH_MOUSEID) continue;
                    SDL_RenderCoordinatesFromWindow(renderer, e.motion.x, e.motion.y, &x, &y);
                } else {
                    if (e.button.which == SDL_TOUCH_MOUSEID) continue;
                    SDL_RenderCoordinatesFromWindow(renderer, e.button.x, e.button.y, &x, &y);
                }
                in.pointer_x = int(x); in.pointer_y = int(y);
                in.pointer_moved = true;
                if (e.type != SDL_EVENT_MOUSE_MOTION) in.pointer_click = true;
            }
            if (e.type == SDL_EVENT_RENDER_DEVICE_RESET || e.type == SDL_EVENT_RENDER_TARGETS_RESET) {
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
                if (!texture) throw std::runtime_error(SDL_GetError());
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            }
            if (e.type == SDL_EVENT_RENDER_DEVICE_LOST) throw std::runtime_error("Render device lost");
        }
        if (!running) break;
        if (lifecycle.state.take_clock_reset() || lifecycle.state.paused()) menu_stick.suspend();
        if (lifecycle.state.paused() && (in.select || in.pointer_click)) { lifecycle.state.resume(); in = {}; }
        if (!lifecycle.state.paused() && pad) {
            const auto stick = menu_stick.update(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX),
                                                 SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY), SDL_GetTicks());
            in.left |= stick.left; in.right |= stick.right; in.up |= stick.up; in.down |= stick.down;
        }
        if (!lifecycle.state.paused()) fe.update(in, SDL_GetTicks());
        music.update();
        mixer.pause(lifecycle.state.paused() || audio_lifecycle.state.paused());
        if (lifecycle.state.background()) { SDL_Delay(20); continue; }
        fe.draw(frame);
        SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_UpdateTexture(texture, nullptr, frame.pixels.data(), 640 * 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        if (lifecycle.state.paused()) draw_pause(renderer);
        if (menu_frames >= 0 && ++frames >= menu_frames) {
            if (!screenshot.empty()) {
                SDL_Surface* capture = SDL_RenderReadPixels(renderer, nullptr);
                if (!capture || !SDL_SaveBMP(capture, screenshot.c_str())) throw std::runtime_error(SDL_GetError());
                SDL_DestroySurface(capture);
            }
            running = false;
        }
        SDL_RenderPresent(renderer);
    }
    settings.music_track = music.track();
    rr::save_settings(settings_path, settings);
    music.stop();
    if (text_input) SDL_StopTextInput(window);
    if (pad) SDL_CloseGamepad(pad);
    SDL_DestroyTexture(texture);
    return 0;
}

int main(int argc, char** argv) {
    std::string root, screenshot, video;
    int course = 0, bike = 0, level = 0, opponents = 7;
    long run_ticks = -1, menu_frames = -1;
    bool auto_drive = false, start_paused = false, first_run_smoke = false, quick_race = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--assets" && i + 1 < argc) root = argv[++i];
        else if (a == "--video" && i + 1 < argc) video = argv[++i];
        else if (a == "--course" && i + 1 < argc) { course = std::atoi(argv[++i]); quick_race = true; }
        else if (a == "--bike" && i + 1 < argc) { bike = std::atoi(argv[++i]); quick_race = true; }
        else if (a == "--ticks" && i + 1 < argc) { run_ticks = std::atol(argv[++i]); quick_race = true; }
        else if (a == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
        else if (a == "--autopilot") { auto_drive = true; quick_race = true; }
        else if (a == "--paused") start_paused = true;
        else if (a == "--race") quick_race = true;
        else if (a == "--menu-frames" && i + 1 < argc) menu_frames = std::atol(argv[++i]);
        else if (a == "--first-run-smoke") first_run_smoke = true;
        else if (a == "--level" && i + 1 < argc) { level = std::atoi(argv[++i]) - 1; quick_race = true; }
        else if (a == "--opponents" && i + 1 < argc) { opponents = std::atoi(argv[++i]); quick_race = true; }
        else { std::fprintf(stderr, "Usage: rr-game [--assets DIR] [--menu-frames N --screenshot FILE.bmp] | [--race] [--course 0-4] [--bike 0-14] [--level 1-5] [--opponents 0-7] [--autopilot] [--paused] [--ticks N --screenshot FILE.bmp]\n"); return 2; }
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
        g.bobs = rr::read_bobs(assets.read("DATA/BIKERS/HIBOB.DAT"), rr::bob_frames);
        g.remaps = rr::read_rider_remaps(exe, 9);
        g.palette = rr::Palette::from_raw(assets.read("DATA/PALETTE.RAW"));
        if (!quick_race) {
            SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);
            result = run_game(assets, window, renderer, g, exe, screenshot, menu_frames);
        } else {
            g.course_name = course_names[course];
            if (bike < 0 || size_t(bike) >= g.specs.size()) throw std::runtime_error("Bike index out of range");
            g.bike = bike;
            g.level = level;
            g.load_course(assets,exe);
            g.opponents = rr::original_opponents(exe, g.specs, level, opponents);
            g.restart();
            if (run_ticks >= 0) {   // headless-style verification run
                for (long t = 0; t < run_ticks && !g.race->finished(); ++t) g.tick(auto_drive ? autopilot(*g.race) : 0);
            }
            run_race(g, renderer, {auto_drive, start_paused, screenshot});
        }
    } catch (const std::exception& e) { SDL_Log("%s", e.what()); result = 1; }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
