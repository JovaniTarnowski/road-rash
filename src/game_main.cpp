#include "rr_build_info.hpp"
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
#include "traffic.hpp"
#include "pedestrians.hpp"
#include "road_draw.hpp"
#include "session.hpp"
#include "video_player.hpp"
#include "career.hpp"
#include "course_scene.hpp"
#include "actor_nodes.hpp"
#include "hud.hpp"
#include "exe_tables.hpp"
#include "frontend.hpp"
#include "native_screens.hpp"
#include "pe_strings.hpp"
#include "settings.hpp"
#include "display.hpp"
#include "view_state.hpp"
#include "sound.hpp"
#include "race_music.hpp"
#include "render_threads.hpp"
#include "text.hpp"
#include <filesystem>
#include <future>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <functional>
#include <limits>
#include <string>

namespace {
const char* const course_names[] = {"NAPA", "CITY", "HIWAY", "CANYN", "MEDLY"};
// The 640x480 original frames are scaled to arbitrary window sizes. Nearest
// sampling at non-integer factors doubles alternate texel columns (uneven,
// "soft" menu text); pixel-art sampling keeps texels sharp and even.
#if SDL_VERSION_ATLEAST(3, 4, 0)
constexpr SDL_ScaleMode crisp_scale = SDL_SCALEMODE_PIXELART;
#else
constexpr SDL_ScaleMode crisp_scale = SDL_SCALEMODE_NEAREST;
#endif
constexpr int countdown_ticks = 3 * rr::ticks_per_second;
#ifdef __ANDROID__
constexpr bool kMobileDisplay = true;
#else
constexpr bool kMobileDisplay = false;
#endif
// Display settings driver (plan 18 Task 3): owns the window/renderer state derived
// from Settings. Each mode applies its logical height (480 menus/videos, 480 race:
// the race composes the dashboard inside the 480 frame, so no 510 path exists yet).
struct DisplayDriver {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    rr::Settings* settings = nullptr;   // borrowed: single source of truth lives with the caller
    std::string settings_path;
    rr::DisplayConfig config;
    long toggle_at = -1;   // debug: toggle fullscreen at this frame/tick count (--toggle-fullscreen-at)
    long frame = 0;
    int render_scale = 1;   // plan 19 Task 8: effective scale, re-queried on apply
    int render_threads = 1;   // plan 19 Task 8: worker count for the band renderer
    void apply(int logical_h) {
        config = rr::display_config(*settings, logical_h, kMobileDisplay);
        rr::apply_display(window, renderer, config);
        int dw = 0, dh = 0;
        SDL_GetWindowSize(window, &dw, &dh);
        render_scale = rr::effective_render_scale(*settings, dw, dh, kMobileDisplay);
        config.supersampling = 640 * render_scale > dw || 480 * render_scale > dh;
        int cores = SDL_GetNumLogicalCPUCores();
        render_threads = std::max(1, std::min(8, cores - 1));
    }
    void toggle_fullscreen() {
        settings->display_mode = settings->display_mode == rr::DisplayMode::fullscreen
            ? rr::DisplayMode::windowed : rr::DisplayMode::fullscreen;
        rr::save_settings(settings_path, *settings);
        apply(config.logical_h);
    }
    void maybe_toggle() { if (toggle_at >= 0 && ++frame >= toggle_at) { toggle_at = -1; toggle_fullscreen(); } }
};
struct CliDisplayOverride { int fullscreen = -1; int window_scale = 0; long toggle_at = -1; };
void apply_cli_overrides(rr::Settings& s, const CliDisplayOverride& o) {
    if (o.fullscreen >= 0) s.display_mode = o.fullscreen ? rr::DisplayMode::fullscreen : rr::DisplayMode::windowed;
    if (o.window_scale >= 1 && o.window_scale <= 4) s.window_scale = o.window_scale;
}

// Native startup feedback, available before any disc fonts/art are decoded.
void draw_loading(SDL_Renderer* renderer, const char* stage) {
    SDL_SetRenderDrawColor(renderer, 15, 18, 22, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 225, 225, 128, 255);
    SDL_SetRenderScale(renderer, 2, 2);
    SDL_RenderDebugText(renderer, 92, 102, "LOADING ROAD RASH");
    SDL_SetRenderScale(renderer, 1, 1);
    SDL_RenderDebugText(renderer, 180, 250, stage);
    SDL_FRect track{180, 282, 280, 4};
    SDL_SetRenderDrawColor(renderer, 50, 55, 60, 255);
    SDL_RenderFillRect(renderer, &track);
    SDL_FRect pulse{180 + float((SDL_GetTicks() / 4) % 232), 282, 48, 4};
    SDL_SetRenderDrawColor(renderer, 225, 225, 128, 255);
    SDL_RenderFillRect(renderer, &pulse);
    SDL_RenderPresent(renderer);
}

template<class Work>
auto load_with_indicator(SDL_Renderer* renderer, const char* stage, Work work) {
    draw_loading(renderer, stage);
#ifdef __EMSCRIPTEN__
    // The web build does not enable pthreads. Native platforms animate while
    // loading; the single-thread web build still presents the loading label.
    return work();
#else
    auto pending = std::async(std::launch::async, std::move(work));
    bool quit = false;
    while (pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) if (event.type == SDL_EVENT_QUIT) quit = true;
        draw_loading(renderer, stage);
        SDL_Delay(16);
    }
    auto result = pending.get();
    if (quit) { SDL_Event event{}; event.type = SDL_EVENT_QUIT; SDL_PushEvent(&event); }
    return result;
#endif
}

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
    std::unique_ptr<rr::ActorNodes> actors;
    std::vector<rr::ActorAnimationState> actor_states;
    std::vector<int> previous_attacks;
    std::vector<bool> previous_crashes;
    rr::CrashConfig crash_config;
    std::array<std::array<std::array<int8_t,4>,2>,2> attack_codes{};
    std::unique_ptr<rr::VehicleLibrary> vehicles;
    std::unique_ptr<rr::TrafficSystem> traffic;
    std::unique_ptr<rr::PedestrianSystem> pedestrians;   // DZHR pedestrians (plan 24 Task 4); needs the course scene
    bool metric=false;
    std::string player_name="Player";
    std::vector<std::string> rider_names;
    std::string course_name;
    int bike = 0;
    rr::Widescreen widescreen = rr::Widescreen::off;  // plan 21 Task 8: live setting
    rr::Enhancements enhancements;  // plan 21 Task 1: synced per frame; all-off = original
    rr::ViewState view_prev, view_cur, view_drawn;  // plan 21 Task 2: interpolated frame state
    bool view_ready = false;
    // Blended position overrides for the current frame (id -> pose).
    const rr::ActorPose* blend_pose(uint32_t id) const {
        for (const auto& p : view_drawn.actors) if (p.id == id) return &p;
        return nullptr;
    }
    uint32_t blend_camera_position() const { return view_ready ? view_drawn.camera_position_q8 : race->camera_position(); }
    rr::Camera blend_camera() const { return view_ready ? view_drawn.camera : race->camera(); }
    int32_t logical_width() const {
        const rr::Widescreen w = enhancements.any() ? enhancements.widescreen : widescreen;
        return rr::logical_width_for(w);
    }
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
    rr::RaceMusicPlayer* race_music = nullptr;
    rr::SoundSystem* sound = nullptr;   // race effects and engines (0x00434980)
    rr::Mixer* mixer = nullptr;

    void load_course(const rr::Assets& assets,const std::vector<uint8_t>& exe) {
        scene.reset();
        hud.reset();
        actors.reset();
        course=rr::read_course(assets.read("DATA/COURSES/"+course_name+".CRS"));
        // Plan 17 Task 2: the original scene, dashboard and actors on every
        // course ("playable, not yet verified" off Napa). A course whose scene
        // data fails to resolve keeps the dashboard and actors and falls back
        // to the flat debug road, logging the missing resource.
        {
            try { scene=std::make_unique<rr::CourseScene>(assets,exe,course,course_name,1,level); }
            catch(const std::exception& e) { SDL_Log("%s scene unavailable: %s",course_name.c_str(),e.what()); }
            hud=std::make_unique<rr::Hud>(assets,exe,bike,metric);
            actors=std::make_unique<rr::ActorNodes>(assets,exe,true);
            crash_config=rr::read_crash_config(exe);
            for(int side=0;side<2;++side) {
                const auto bytes=rr::read_exe_data(exe,side?0x464518:0x4644f8,32);
                for(int kind=0;kind<2;++kind)for(int weapon=0;weapon<4;++weapon)attack_codes[side][kind][weapon]=int8_t(bytes[(kind*4+weapon)*4]);
            }
        }
        // Free traffic from the course's own vehicle container (0x0044db80 /
        // 0x004465f0). A malformed container disables traffic rather than the
        // race; the importer has already hash-verified the files.
        pedestrians.reset();
        if(scene)pedestrians=std::make_unique<rr::PedestrianSystem>(
            [this](const rr::RouteSample& s)->const std::vector<rr::DzhrActor>&{return scene->dzhr_actors(s.section,s.local);},
            [this](const rr::RouteSample& s,uint8_t selector){return scene->pedestrian_look(s.section,s.local,selector);},exe);
        traffic.reset();
        if(assets.exists("DATA/CARS/"+course_name+".CAR")) {
            try {
                vehicles=std::make_unique<rr::VehicleLibrary>(assets.read("DATA/CARS/"+course_name+".CAR"),exe);
                traffic=std::make_unique<rr::TrafficSystem>(*vehicles,exe);
                traffic->configure(6);
            } catch(const std::exception&) {traffic.reset();}
        }
    }

    void restart() {
        race = std::make_unique<rr::Race>(rr::make_route(course, false), rr::make_route(course, true),
                                          physics, specs.at(size_t(bike)), rider_value, opponents, level);
        if(scene)race->set_terrain_source([this](const rr::RouteSample& sample)->const rr::TerrainSlice&{return scene->terrain(sample.section,sample.local);});
        if(traffic)race->set_traffic(traffic.get());
        if(pedestrians){pedestrians->reset();race->set_pedestrians(pedestrians.get());}
        if(actors)race->set_crash_library(actors->library(),crash_config);
        actor_states.assign(race->riders().size(),{});
        previous_attacks.assign(race->riders().size(),0);
        previous_crashes.assign(race->riders().size(),false);
        if(actors)for(auto& state:actor_states) {
            rr::set_animation_with_fallback(state,actors->library().playback[0],2,0,actors->library().fallback);
            rr::queue_animation(state,actors->library().playback[0],2); // 0x00446060 initializes current and queued riding
            rr::advance_animation(state,actors->library().playback[0],0);
        }
        countdown = countdown_ticks;
    }
    std::vector<rr::DrawNode> rider_nodes(const rr::RoadSlice& near,const rr::RoadSlice& far,int road,int32_t ceiling,bool back)const {
        struct Visible {int32_t position;size_t index;bool car,body=false,pedestrian=false;};
        std::vector<Visible> visible;
        // Plan 21 Task 2: sample membership uses live positions (which slice
        // an actor is on); the drawn quad uses the blended pose. With
        // interpolation off the blend is alpha=65536 = live, exactly.
        auto rider_pose = [&](size_t i,bool body) -> const rr::ActorPose* {
            return view_ready ? blend_pose(uint32_t(body ? 1000 + i : i + 1)) : nullptr;
        };
        for(size_t i=0;i<race->riders().size();++i) {
            const auto& rd=race->riders()[i];
            auto include=[&](const rr::Bike& actor,int route,const rr::ActorAnimationState& state,bool body) {
                const auto pos=uint32_t(actor.i32(0x1c));
                if((pos>>8)!=near.sample || ((pos&255)>=128)!=back || !state.selected)return;
                if(near.count==2 && route!=road)return;
                if(const auto* pose=rider_pose(i,body))visible.push_back({int32_t(pose->position_q8),i,false,body});
                else visible.push_back({actor.i32(0x1c),i,false,body});
            };
            if(rd.crash_active) {
                include(rd.bike,rd.route,rd.crash_bike_animation,false);
                include(rd.crash_actor.motion,rd.crash_actor.route,rd.crash_actor.animation,true);
            }else include(rd.bike,rd.route,actor_states[i],false);
        }
        if(traffic && road==near.own)for(size_t i=0;i<traffic->cars().size();++i) {
            const auto& car=traffic->cars()[i];const auto pos=uint32_t(car.position);
            if(car.active && (pos>>8)==near.sample && ((pos&255)>=128)==back) {
                if(view_ready) {
                    if(const auto* pose=blend_pose(uint32_t(2000 + i)))visible.push_back({int32_t(pose->position_q8),i,true});
                    else visible.push_back({car.position,i,true});
                } else visible.push_back({car.position,i,true});
            }
        }
        if(pedestrians && road==near.own)for(size_t i=0;i<pedestrians->pool().size();++i) {
            const auto& p=pedestrians->pool()[i];const auto pos=uint32_t(p.body.bike.i32(0x1c));
            if(p.active && (pos>>8)==near.sample && ((pos&255)>=128)==back)visible.push_back({int32_t(pos),i,false,false,true});
        }
        std::stable_sort(visible.begin(),visible.end(),[](const Visible& a,const Visible& b){return a.position>b.position;});
        std::vector<rr::DrawNode> output;
        for(const auto& item:visible) {
            const size_t i=item.index;
            if(item.pedestrian) {
                if(auto node=rr::pedestrian_node(pedestrians->pool()[i],near,far,road,race->route().at(near.sample),race->camera(),depth,ceiling,logical_width()))output.push_back(*node);
                continue;
            }
            if(item.car) {
                const auto& car=traffic->cars()[i];
                int32_t bpos = 0, blat = 0; bool use_blend = false;
                if(view_ready) {
                    if(const auto* pose=blend_pose(uint32_t(2000 + i))) { bpos=int32_t(pose->position_q8); blat=pose->lateral_q8; use_blend=true; }
                }
                if(auto node=rr::traffic_car_node(traffic->library().vehicles().at(car.vehicle),car,near,far,road,race->route().at(near.sample),blend_camera(),depth,int32_t(blend_camera_position()),ceiling,logical_width(),bpos,blat,use_blend))output.push_back(*node);
                continue;
            }
            const auto& rd=race->riders()[i];const auto& bike=item.body?rd.crash_actor.motion:rd.bike;
            const auto& sample=race->route_for(item.body?rd.crash_actor.route:rd.route).at(near.sample);
            const auto& a=near.road[road];const auto& b=far.road[road];const auto camera=blend_camera();
            // Plan 21 Task 2: drawn position/lateral/ground/heading/roll come
            // from the blended pose; sample membership above stays live.
            int32_t pos_q8=bike.i32(0x1c),lat_q8=bike.i32(0x28),gnd_q8=bike.i32(0x24),head=bike.i32(0x5c),roll=bike.i32(0x60);
            if(view_ready) {
                if(const auto* pose=rider_pose(i,item.body)) {
                    pos_q8=int32_t(pose->position_q8);lat_q8=pose->lateral_q8;gnd_q8=pose->height_q8;head=pose->heading;roll=pose->roll;
                }
            }
            rr::ActorOrientation orientation;orientation.depth=near.depth;orientation.position=pos_q8;orientation.road_x=a.road_x+camera.x;orientation.lateral_q8=lat_q8-sample.offset*256;orientation.road_heading=a.heading;orientation.heading=head;orientation.roll=roll;orientation.rider=!rd.crash_active || item.body;
            rr::ActorProjection projection;projection.road_x=orientation.road_x;projection.far_x=b.road_x+camera.x;projection.road_y=a.world_y;projection.far_y=b.world_y;projection.lateral=orientation.lateral_q8/256;projection.ground=gnd_q8;projection.depth=near.depth;projection.fraction=uint8_t(orientation.position);projection.occlusion=ceiling;
            const auto& state=rd.crash_active?(item.body?rd.crash_actor.animation:rd.crash_bike_animation):actor_states[i];
            const auto selected=*state.selected;const auto& descriptor=actors->library().playback[0].at(size_t(selected[0]))->at(size_t(selected[1]));
            const auto nodes=actors->rider(descriptor,0,rd.colour,orientation,projection,camera,depth,bike.i32(0x20),nullptr,logical_width());output.insert(output.end(),nodes.begin(),nodes.end());
        }
        return output;
    }
    void tick(uint32_t controls) {
        if (countdown > 0) --countdown;   // race clock starts after the countdown
        else {
            race->step(controls);
            if(actors)for(size_t i=0;i<actor_states.size();++i) {
                const auto& rider=race->riders()[i];const auto& b=rider.bike;auto& state=actor_states[i];
                const auto& library=actors->library();
                if(b.i32(0x2f4)){previous_crashes[i]=true;continue;}
                if(previous_crashes[i]){state=rider.crash_bike_animation;previous_crashes[i]=false;previous_attacks[i]=0;}
                // Race still uses its documented combat timer. Bridge its
                // completion to the original queued-animation mechanism until
                // original hit/event scheduling replaces that timer.
                if(!rider.attack.timer && previous_attacks[i])rr::queue_animation_with_fallback(state,library.playback[0],2,library.fallback);
                previous_attacks[i]=rider.attack.timer;
                if(rider.attack.timer==1)rr::set_animation_with_fallback(state,library.playback[0],attack_codes[rider.attack.right][rider.attack.backhand][rider.attack.weapon],uint32_t(race->ticks()),library.fallback);
                rr::RidingAnimationInput input;input.speed=b.i32(0xec);input.throttle=b.i32(0x114);input.lateral_speed=b.i32(0x100);input.boost=b.i32(0xf4);input.flags=b.u8(0x278);input.player=i==0;input.wrecked=(b.u8(0xc0)&0x10)!=0;input.camera_distance=std::abs(b.i32(0x1c)-int32_t(race->camera_position()));
                rr::advance_riding_animation(state,library.playback[0],library.fallback,input,uint32_t(race->ticks()));
            }
        }
        if (sound) {
            for (const auto& e : race->take_sound_events()) sound->post(e);
            if (traffic) for (const auto& e : traffic->take_sound_events()) sound->post(e);
            sound->update(*race);
        } else {
            rr::update_engine_rpm(race->riders()[0].bike);   // 0x00434980 without sound
        }
    }
};

// Demo/smoke driver for --autopilot (native, not the original AI). Picks a
// line clear of traffic and riders (overtaking only when oncoming cars leave
// time), follows a blocking car's speed when no line is clear, eases off for
// bends ahead and lets go of every control while crashed: the crash walk back
// to the bike (0x0040a600 state 38) only converges with no input, see
// steer_into_traffic.
uint32_t autopilot(const rr::Race& race, const rr::TrafficSystem* traffic) {
    if (race.crashed()) return 0;
    const auto& route = race.route();
    const auto& bike = race.bike();
    const int32_t speed = race.speed();
    const size_t here = race.position() >> 8, last = route.size() - 1;
    const size_t aim = std::min(last, here + 3 + size_t(std::max(0, speed) / 1500));
    const auto& s = route[aim];
    // Lateral positions relative to the road centre of their own sample.
    const int32_t me = race.lateral() - route[here].offset;
    // Candidate lines every 62 units across the narrowest road between here
    // and the aim point, plus lane centres and lane lines (250-unit lanes);
    // the first right-hand lane first, then the closest line. A car is touched
    // within 94 lateral units (car contact box dl < 24064 Q8), so a line 120
    // from every car is clear: the gap between two lanes' traffic counts.
    int32_t road_left = s.path.left, road_right = s.path.right;
    for (size_t i = here; i <= aim; ++i) { road_left = std::min(road_left, route[i].path.left); road_right = std::min(road_right, route[i].path.right); }
    std::vector<int32_t> lanes;
    for (int32_t x = -road_left + 40; x <= road_right - 40; x += 62) lanes.push_back(x);
    for (int32_t x = -road_left + 125; x <= road_right - 125; x += 125) lanes.push_back(x);   // centres and lines
    if (lanes.empty()) lanes.push_back(0);
    const int32_t home = road_right >= 250 ? 125 : road_right / 2;   // first right-hand lane
    std::stable_sort(lanes.begin(), lanes.end(), [&](int32_t a, int32_t b) {
        if ((a == home) != (b == home)) return a == home;
        return std::abs(a - me) < std::abs(b - me);
    });
    // Traffic within reach; speeds become Q8 samples per tick (speed/260,
    // measured: 10000 covers 0.15 samples a tick). Oncoming cars (lane < 0)
    // carry a negative velocity.
    struct Obstacle { int32_t x, gap; double velocity; };
    std::vector<Obstacle> obstacles;
    if (traffic) for (const auto& car : traffic->cars()) {
        if (!car.active) continue;
        const int64_t gap = int64_t(car.position) - int64_t(race.position());
        if (gap < -128 || gap > 64 * 256) continue;
        const size_t at = std::min(last, size_t(std::max<int64_t>(0, car.position >> 8)));
        obstacles.push_back({(car.lateral >> 8) - route[at].offset, int32_t(gap), (car.lane < 0 ? -car.speed : car.speed) / 260.0});
    }
    // Other riders on this roadway: contact shoves the bike sideways.
    for (size_t i = 1; i < race.riders().size(); ++i) {
        const auto& rider = race.riders()[i];
        if (rider.route != race.riders()[0].route) continue;   // crashed bikes still block
        const int64_t gap = int64_t(rider.bike.i32(0x1c)) - int64_t(race.position());
        if (gap < -128 || gap > 64 * 256) continue;
        const size_t at = std::min(last, size_t(std::max<int64_t>(0, rider.bike.i32(0x1c) >> 8)));
        obstacles.push_back({(rider.bike.i32(0x28) >> 8) - route[at].offset, int32_t(gap), rider.bike.i32(0xec) / 260.0});
    }
    const double pass_velocity = std::max(speed, 7000) / 260.0;
    // Nearest same-direction obstacle in a line within 20 samples.
    auto ahead_in = [&](int32_t lane) -> const Obstacle* {
        const Obstacle* best = nullptr;
        for (const auto& o : obstacles)
            if (o.velocity >= 0 && o.gap > -128 && o.gap < 20 * 256 && std::abs(o.x - lane) < 120 && (!best || o.gap < best->gap)) best = &o;
        return best;
    };
    const Obstacle* blocker = ahead_in(lanes.front());
    double need = 90;   // ticks to change line, plus to get past the blocker
    if (blocker) need += (blocker->gap + 3 * 256) / std::max(1.0, pass_velocity - blocker->velocity);
    auto clear = [&](int32_t lane) {
        if (ahead_in(lane)) return false;
        for (const auto& o : obstacles) {
            if (o.velocity >= 0 || std::abs(o.x - lane) >= 120) continue;
            if (o.gap < 128) return false;                                         // alongside
            if (o.gap / (pass_velocity - o.velocity) < need * 1.2 + 40) return false;   // meets before the pass ends
        }
        return true;
    };
    int32_t target = lanes.front(), limit = 1 << 30;
    bool found = false;
    for (const int32_t lane : lanes) if (clear(lane)) { target = lane; found = true; break; }
    if (!found && blocker && blocker->gap > 64 && blocker->gap < 14 * 256)   // follow it until a lane opens
        limit = int32_t(std::max(0.0, blocker->velocity) * 260) + 300;
    // Threading close past a car: keep the closing speed modest.
    for (const auto& o : obstacles)
        if (o.gap > -64 && o.gap < 8 * 256 && std::abs(o.x - target) < 160)
            limit = std::min(limit, std::max(6000, int32_t(std::max(0.0, o.velocity) * 260) + 3000));
    // Zero-width road data (CITY's route tail past sample 5456 until plan 17's
    // race bounds land): the wall stand-in crashes above 4000, so crawl the centre.
    if (road_left + road_right < 100) { target = 0; limit = std::min(limit, 3500); }
    // Bend severity ahead (path bend per sample) sets a speed ceiling.
    int32_t bend = 0;
    for (size_t i = here; i <= std::min(last, here + 12); ++i) bend = std::max(bend, std::abs(route[i].path.bend));
    // move() pushes the bike outward by about bend x distance a tick against
    // a fixed steering authority, so the sustainable speed goes as 1/bend.
    if (bend > 0) limit = std::min(limit, std::clamp(200000 / bend, 4500, 13500));
    // Steer on the lateral position 16 ticks on (+0x28 Q8 plus the +0x10c
    // lateral velocity, as move() integrates it) so the bike does not overshoot.
    // The target is relative to the centre of the sample aimed at: through a
    // split the road centre (offset) moves quickly under the bike.
    const int32_t predicted = int32_t((int64_t(bike.i32(0x28)) + int64_t(bike.i32(0x10c)) * 16) >> 8);
    const int32_t error = s.offset + target - predicted;
    uint32_t c = 0;
    if (error > 20) c |= rr::control_right; else if (error < -20) c |= rr::control_left;
    if (speed > limit + 800) c |= rr::control_brake;
    else if (speed < std::min(3000, limit) || (speed < limit && std::abs(target - me) < 500)) c |= rr::control_accelerate;
    if (race.ticks() % 40 == 0) c |= rr::control_punch;   // demo: swing at anyone alongside
    return c;
}
// Plan 11a Task 1 Step 5: debug driver that hunts the oncoming lane, forcing
// rider–car contact so a headless run logs at least one reason-2 crash.
// Holds a fixed bias (~150 world units left of the road centre), inside the
// oncoming-traffic lanes but clear of the wall stand-in: the constant-left
// predecessor rode the shoulder clamp (x ≈ -723, wall hits every tick),
// which crashed the rider at speed > 4000 (reason 0) before traffic
// arrived. Hands off entirely while crashed (controls 0): the crash-walk
// back to the bike only converges with no steering input (0x0040a600
// state-38 walk; any held direction resets state_elapsed and the walk
// never reaches the bike), then remounts and keeps hunting. Probe
// (Game-faithful: NAPA, bike 0, level 0, 7 opponents, density 6, crash
// library): bias -150 meets a car box (dp<128 AND dl<24064) at t=783 and
// logs reason 2 the same tick (x=-114, no wall contact before the meet);
// 6 reason-2 crashes over 20000 ticks with remounts between each.
uint32_t steer_into_traffic(const rr::Race& race) {
    if (race.bike().i32(0x2f4) != 0) return 0;   // crash-walk needs no input (see above)
    const auto& s = race.route()[race.position() >> 8];
    const int x = race.lateral() - s.offset;
    constexpr int bias = -150;
    uint32_t c = rr::control_accelerate;
    if (x < bias - 40) c |= rr::control_right; else if (x > bias + 40) c |= rr::control_left;
    return c;
}

void quad(SDL_Renderer* r, float x0, float x1, float y0, float x2, float x3, float y1, SDL_FColor c) {
    const SDL_Vertex v[4] = {{{x0,y0},c,{}}, {{x1,y0},c,{}}, {{x3,y1},c,{}}, {{x2,y1},c,{}}};
    const int index[6] = {0,1,2,0,2,3};
    SDL_RenderGeometry(r, nullptr, v, 4, index, 6);
}

// Riding frame from the steering state. Frames 4..11 of HIBOB.DAT are the lean sequence
// (4 strongest right, 11 strongest left) and 3 is straight; the original picks frames through
// its RRAN/RRFD animation system (0x00445a60, 0x0043ae90). Napa uses ActorNodes;
// these inspected frame helpers remain for the other courses.
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
        if (t) SDL_SetTextureScaleMode(t, crisp_scale);
        return t;
    }
};

struct CourseCanvas {
    rr::Framebuffer frame{640,480};
    std::vector<uint8_t> rgba=std::vector<uint8_t>(640*480*4);
    int scale_w=640,scale_h=480;   // plan 19 Task 4: 640s x 480s surface
    int wide_w=320;                // plan 21 Task 8: logical width of the surface
    SDL_Texture* texture=nullptr;
    SDL_ScaleMode scale = crisp_scale;   // plan 18 Task 3: from DisplayDriver (filter setting)
    ~CourseCanvas(){clear();}
    void clear(){SDL_DestroyTexture(texture);texture=nullptr;}
    void resize(int s,int logical_width=320) {
        if (s < 1 || s > 4) throw std::runtime_error("Render scale out of range 1..4");
        if (logical_width * 2 * s == scale_w && 480 * s == scale_h) return;
        wide_w = logical_width;
        scale_w = logical_width * 2 * s; scale_h = 480 * s;
        frame = rr::Framebuffer(scale_w, scale_h);
        rgba.assign(size_t(scale_w) * size_t(scale_h) * 4, 0);
        clear();
    }

    std::unique_ptr<rr::RenderThreads> workers;
    int worker_count = 0;
    void draw(SDL_Renderer* renderer,const Game& game,const std::vector<rr::RoadSlice>& slices,int s,int threads) {
        const int32_t wide = game.logical_width();
        resize(s,wide);
        // Hor+ (plan 21 Task 8): builders centre at wide/2; the 4:3 picture
        // is the centre crop. Shift node coords by the side margin so the
        // 4:3-tuned projection lands centred on the wide surface.
        const int32_t margin = (wide - 320) / 2;
        // Plan 19 Task 8: the dashboard composes at the race scale (Task 5).
        // The quick-race boot path has no HUD (dashboard is a career-mode
        // surface); without one the scene stands alone as in Task 4 shots.
        if (game.hud) {
            // Plan 19 Task 8: the dashboard composes at the race scale. Its
            // RgbImage pixels are palette-mapped into the indexed frame.
            const auto dash = game.hud->compose(game.hud->state(*game.race, game.player_name, game.rider_names),
                                                game.physics, rr::DashMode::full, s);
            // Centred 4:3 dashboard (plan 21 Task 8): offset by the side
            // margin so it sits in the middle of the wide frame.
            const int xoff = margin * 2 * s;
            for (int y = 0; y < dash.height && y < frame.height(); ++y)
                for (int x = 0; x < dash.width && x + xoff < frame.width(); ++x) {
                    const uint32_t c = dash.at(x, y);
                    if (c == 0x01000000) continue;
                    uint8_t best = 0;
                    int best_d = INT_MAX;
                    for (int i = 1; i < 256; ++i) {
                        const uint8_t* rgb = &game.palette.rgbx[size_t(4 * i)];
                        const int dr = int(rgb[0]) - int((c >> 16) & 0xff);
                        const int dg = int(rgb[1]) - int((c >> 8) & 0xff);
                        const int db = int(rgb[2]) - int(c & 0xff);
                        const int d = dr * dr + dg * dg + db * db;
                        if (d < best_d) { best_d = d; best = uint8_t(i); }
                    }
                    frame.row(y)[x + xoff] = best;
                }
            // Side panels: stretched + darkened edge columns (generated at
            // runtime from the user's own dash art, no shipped assets).
            if (margin > 0) {
                const int side_w = margin * 2 * s;
                for (int y = 0; y < dash.height && y < frame.height(); ++y) {
                    const uint32_t lc = dash.at(0, y), rc = dash.at(dash.width - 1, y);
                    if (lc == 0x01000000 && rc == 0x01000000) continue;
                    for (int x = 0; x < side_w; ++x) {
                        const uint32_t c = x < side_w / 2 ? lc : rc;
                        if (c == 0x01000000) continue;
                        // Darken by halving each channel via the palette.
                        const int r = int((c >> 16) & 0xff) / 2, gg = int((c >> 8) & 0xff) / 2, bb = int(c & 0xff) / 2;
                        uint8_t best = 0;
                        int best_d = INT_MAX;
                        for (int i = 1; i < 256; ++i) {
                            const uint8_t* rgb = &game.palette.rgbx[size_t(4 * i)];
                            const int dr = int(rgb[0]) - r, dg = int(rgb[1]) - gg, db = int(rgb[2]) - bb;
                            const int d = dr * dr + dg * dg + db * db;
                            if (d < best_d) { best_d = d; best = uint8_t(i); }
                        }
                        if (x < side_w / 2) frame.row(y)[x] = best;
                        else frame.row(y)[xoff + dash.width + (x - side_w / 2)] = best;
                    }
                }
            }
        }
        if (!workers || worker_count != threads) { workers = std::make_unique<rr::RenderThreads>(threads); worker_count = threads; }
        game.scene->draw_horizon(frame,game.race->horizon_scroll(),wide);
        auto nodes=game.scene->draw_nodes(game.race->route(),slices,game.race->camera(),game.depth,[&](const rr::RoadSlice& near,const rr::RoadSlice& far,int road,int32_t ceiling,bool back){return game.rider_nodes(near,far,road,ceiling,back);},wide);
        if (margin) for (auto& n:nodes) for (auto& v:n.view_points) v.x+=margin*65536;
        // The dashboard stays a centred 4:3 image (plan 21 Task 8); side
        // panels are drawn from its edge columns below.
        rr::render_nodes_threaded(frame,nodes,rr::view_mode_wide(s,wide),16,*workers,game.enhancements.perspective);
        frame.to_rgba(game.palette,rgba.data(),scale_w*4);
        if(!texture) {
            texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,scale_w,scale_h);
            if(!texture)throw std::runtime_error(SDL_GetError());
            SDL_SetTextureScaleMode(texture,scale);
        }
        if(!SDL_UpdateTexture(texture,nullptr,rgba.data(),scale_w*4))throw std::runtime_error(SDL_GetError());
        // Hor+ (plan 21 Task 8): logical presentation is wide*2 x 480,
        // set once per resize below; the texture fills it exactly.
        const SDL_FRect dst{0,0,float(scale_w),float(scale_h)};
        SDL_SetRenderLogicalPresentation(renderer,float(scale_w),float(scale_h),SDL_LOGICAL_PRESENTATION_LETTERBOX);
        if(!SDL_RenderTexture(renderer,texture,nullptr,&dst))throw std::runtime_error(SDL_GetError());
    }
};

// Player bike: original sprite, horizontal position projected at the bike's depth, bottom of the view.
void draw_bike(SDL_Renderer* r, const Game& g, SpriteCache& cache,const std::vector<rr::RoadSlice>& slices) {
    const rr::Race& race = *g.race;
    const auto& me = race.riders()[0];
    if(g.actors)return;
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
        if(g.actors)continue;
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

void play_video(const rr::Assets& assets,const std::string& path,SDL_Renderer* renderer,
                const std::string& screenshot,long ticks,DisplayDriver& display) {
    rr::Mixer mixer(32);
    if (ticks<0 && SDL_InitSubSystem(SDL_INIT_AUDIO) && !mixer.open_device()) SDL_Log("Audio unavailable: %s",SDL_GetError());
    auto loaded = load_with_indicator(renderer, "Preparing video...", [&] {
        return std::make_unique<rr::VideoPlayer>(assets, path, mixer);
    });
    auto& player = *loaded;
    LifecycleWatch lifecycle(&mixer);
    std::unique_ptr<SDL_Gamepad,decltype(&SDL_CloseGamepad)> pad(open_first_gamepad(),SDL_CloseGamepad);
    bool running=true;
    display.frame = 0;
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
        if (!screenshot.empty()) {
            SDL_Surface* capture=SDL_RenderReadPixels(renderer,nullptr);
            if (!capture || !SDL_SaveBMP(capture,screenshot.c_str())) throw std::runtime_error(SDL_GetError());
            SDL_DestroySurface(capture);running=false;
        }
        SDL_RenderPresent(renderer);
        display.maybe_toggle();
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
    bool auto_drive = false, start_paused = false, demo = false;   // attract demo: any input exits quietly
    std::string screenshot;
    std::array<uint16_t, 11> key_codes{};   // plan 18 Task 7: honored when custom_keys
    bool custom_keys = false;
    bool swap_punch_kick = false;            // plan 18 Task 7: native gamepad page
    int stick_deadzone = 35;                 // plan 18 Task 7: percent, 10..40
    bool left_handed = false;                // plan 18 Task 7: native touch page
    rr::Frontend* menu = nullptr;            // plan 18 Task 8: pause menu; null keeps the old freeze
    std::function<void()> apply_settings;    // host applies display+mixer levels on menu events
    bool show_fps = false;                   // plan 18 Task 9: FPS + ticks/sec overlay
    const rr::Assets* assets = nullptr;      // fonts for the overlay; null disables it
    int render_scale = 1;                    // plan 19 Task 4: race surface is 640s x 480s
    const rr::Settings* settings = nullptr;  // plan 19 Task 8: live scale via effective_render_scale
};

// One race: the playable loop. In career mode the race is decided by the player finishing
// (0x00434660: code 0x7f), wrecking or quitting (code 5), and runs on for 0xb4 ticks.
// Plan 18 Task 8: capture the presented race frame into a 640x480 menu
// background (box-sample down from the window surface).
static rr::RgbImage capture_race_frame(SDL_Renderer* renderer) {
    rr::RgbImage frame(640, 480, 0);
    SDL_Surface* raw = SDL_RenderReadPixels(renderer, nullptr);
    if (!raw) return frame;
    SDL_Surface* s = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_XRGB8888);
    SDL_DestroySurface(raw);
    if (!s) return frame;
    const auto* px = static_cast<const uint32_t*>(s->pixels);
    const int sw = s->w, sh = s->h, pitch = s->pitch / 4;
    for (int y = 0; y < 480; ++y) {
        const int y0 = y * sh / 480, y1 = (y + 1) * sh / 480;
        for (int x = 0; x < 640; ++x) {
            const int x0 = x * sw / 640, x1 = (x + 1) * sw / 640;
            uint64_t r = 0, gg = 0, b = 0, n = 0;
            for (int sy = y0; sy < std::max(y0 + 1, y1); ++sy)
                for (int sx = x0; sx < std::max(x0 + 1, x1); ++sx) {
                    const uint32_t p = px[size_t(sy) * size_t(pitch) + size_t(sx)];
                    r += (p >> 16) & 0xff; gg += (p >> 8) & 0xff; b += p & 0xff; ++n;
                }
            frame.at(x, y) = uint32_t(((r / n) << 16) | ((gg / n) << 8) | (b / n));
        }
    }
    SDL_DestroySurface(s);
    return frame;
}

// Plan 18 Task 8: while the pause menu is open it owns all input, with the
// menu loop's mapping (arrows + Enter/Escape, d-pad + A/B, pointer/touch).
static void pause_event(rr::Frontend* menu, const SDL_Event& e, SDL_Gamepad*& pad, SDL_Renderer* renderer) {
    rr::MenuInput in;
    if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) { pad = SDL_OpenGamepad(e.gdevice.which); return; }
    if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad) == e.gdevice.which) {
        SDL_CloseGamepad(pad); pad = nullptr; return;
    }
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (!e.key.repeat && e.key.scancode > 0 && e.key.scancode < SDL_SCANCODE_COUNT)
            in.capture_scancode = int(e.key.scancode);
        switch (e.key.key) {
        case SDLK_LEFT: in.left = true; break;
        case SDLK_RIGHT: in.right = true; break;
        case SDLK_UP: in.up = true; break;
        case SDLK_DOWN: in.down = true; break;
        case SDLK_HOME: in.home = true; break;
        case SDLK_END: in.end = true; break;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: if (!e.key.repeat) in.select = true; break;
        case SDLK_ESCAPE: case SDLK_P: case SDLK_AC_BACK: if (!e.key.repeat) in.back = true; break;
        default: break;
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && pad && SDL_GetGamepadID(pad) == e.gbutton.which) {
        switch (e.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: in.left = true; break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: in.right = true; break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP: in.up = true; break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: in.down = true; break;
        case SDL_GAMEPAD_BUTTON_SOUTH: case SDL_GAMEPAD_BUTTON_START: in.select = true; break;
        case SDL_GAMEPAD_BUTTON_EAST: case SDL_GAMEPAD_BUTTON_BACK: in.back = true; break;
        default: break;
        }
    } else if (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
               e.type == SDL_EVENT_FINGER_DOWN) {
        float x = 0, y = 0;
        if (e.type == SDL_EVENT_FINGER_DOWN) {
            int w = 0, h = 0;
            SDL_Window* window = SDL_GetRenderWindow(renderer);
            if (window) SDL_GetWindowSize(window, &w, &h);
            SDL_RenderCoordinatesFromWindow(renderer, e.tfinger.x * float(w), e.tfinger.y * float(h), &x, &y);
        } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
            if (e.motion.which == SDL_TOUCH_MOUSEID) return;
            SDL_RenderCoordinatesFromWindow(renderer, e.motion.x, e.motion.y, &x, &y);
        } else {
            if (e.button.which == SDL_TOUCH_MOUSEID) return;
            SDL_RenderCoordinatesFromWindow(renderer, e.button.x, e.button.y, &x, &y);
        }
        in.pointer_x = int(x); in.pointer_y = int(y);
        in.pointer_moved = true;
        if (e.type != SDL_EVENT_MOUSE_MOTION) in.pointer_click = true;
    } else return;
    menu->update(in, SDL_GetTicks());
}

RaceEnd run_race(Game& g, SDL_Renderer* renderer, const RaceOptions& opt, DisplayDriver& display) {
    RaceEnd result;
    SpriteCache sprites;
    CourseCanvas course_canvas;
    course_canvas.scale = display.config.texture_scale;
    LifecycleWatch lifecycle(g.mixer);
    if (opt.start_paused) lifecycle.state.pause();
    rr::TouchInput touch;
    touch.set_left_handed(opt.left_handed);
    rr::TickClock clock;
    SDL_Gamepad* pad = open_first_gamepad();
    SDL_Texture* pause_tex = nullptr;   // plan 18 Task 8: presents the 640x480 pause menu
    int pause_scale_w = 0;   // plan 19 Task 8: reallocates when the effective scale changes
    rr::RgbImage pause_img;
    // Plan 18 Task 9: FPS + ticks/sec overlay, refreshed twice a second.
    const bool fps_on = opt.show_fps && opt.assets;
    std::unique_ptr<rr::TextRenderer> fps_text;
    SDL_Texture* fps_tex = nullptr;
    rr::RgbImage fps_img;
    uint64_t fps_next = 0, fps_last = 0;
    int fps_frames = 0, fps_tick_acc = 0, fps_v = 0, tps_v = 0;
    std::vector<uint32_t> fps_argb;   // upload buffer: transparent bg, opaque ink
    bool running = true, decided = false;
    int end_ticks = 0;
    uint64_t last = SDL_GetTicksNS();
    display.frame = 0;
    display.apply(480);
    while (running) {
        SDL_Event e;
        bool restart = false;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; result.quit_app = true; }
            if (!lifecycle_event(e.type)) lifecycle.state.handle(e);
            if (g.mixer && !g.mixer->handle(e)) SDL_Log("Audio reopen failed: %s", SDL_GetError());
            const bool demo_exit = opt.demo && ((e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) ||
                e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_ADDED ||
                e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            if (demo_exit) { running = false; continue; }   // attract demo: any input exits quietly
            const bool menu_open = opt.menu && opt.menu->pause_open();
            if (menu_open) { pause_event(opt.menu, e, pad, renderer); continue; }   // menu owns all input
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
                // Plan 18 Task 8: Escape/P/Back opens the pause menu; without one
                // the old lifecycle freeze stays (headless/smoke paths).
                if (e.key.key == SDLK_AC_BACK) {
                    if (opt.menu) { opt.menu->open_pause(capture_race_frame(renderer)); lifecycle.state.resume(); }
                    else lifecycle.state.pause();
                }
                if (e.key.key == SDLK_ESCAPE && g.career && !lifecycle.state.paused()) g.quitting = true;
                else if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_P) {
                    if (opt.menu) { opt.menu->open_pause(capture_race_frame(renderer)); lifecycle.state.resume(); }
                    else if (lifecycle.state.paused()) lifecycle.state.resume(); else lifecycle.state.pause();
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
                    if (opt.menu) { opt.menu->open_pause(capture_race_frame(renderer)); lifecycle.state.resume(); }
                    else if (lifecycle.state.paused()) lifecycle.state.resume(); else lifecycle.state.pause();
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
        // Plan 18 Task 8: a lifecycle freeze (background return, focus loss,
        // pad removal) opens the pause menu instead of the old PAUSED text.
        if (opt.menu && !opt.menu->pause_open() && lifecycle.state.paused() && !lifecycle.state.background()) {
            opt.menu->open_pause(capture_race_frame(renderer));
            lifecycle.state.resume();
        }
        if (opt.menu && opt.menu->pause_open()) {
            rr::FrontendEvent ev;
            while (opt.menu->poll(ev)) {
                if (ev.kind == rr::FrontendEvent::Kind::resume_race) lifecycle.state.resume();
                else if (ev.kind == rr::FrontendEvent::Kind::quit_race) {
                    lifecycle.state.resume();
                    if (!decided) { result.outcome = rr::RaceOutcome::wrecked; result.place = std::max(0, g.race->place(0) - 1); }
                    running = false;
                } else if (ev.kind == rr::FrontendEvent::Kind::apply_display) display.apply(480);
                else if (ev.kind == rr::FrontendEvent::Kind::apply_mixer && opt.apply_settings) opt.apply_settings();
            }
        }
        if (!running) break;
        rr::PlayerInput in = touch.current();
        const bool* keys = SDL_GetKeyboardState(nullptr);
        // Plan 18 Task 7: custom tables are honored via bound_control_bits below;
        // defaults use the hardcoded keys (touch/gamepad layer through `in` either way).
        if (!opt.custom_keys) {
            in.accelerate |= keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
            in.brake |= keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
            in.left |= keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
            in.right |= keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
            in.punch |= keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_LCTRL];
            in.kick |= keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_LALT];
            in.backhand |= keys[SDL_SCANCODE_C];
        }
        const int zone = opt.stick_deadzone * 32768 / 100;
        if (pad) {
            const auto south = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
            const auto west = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST);
            const auto north = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH);
            in.accelerate |= south || SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000;
            in.brake |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000;
            in.left |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
            in.right |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            in.steer_axis = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            in.punch |= opt.swap_punch_kick ? north : west;
            in.kick |= opt.swap_punch_kick ? west : north;
            in.backhand |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        }
        uint32_t controls = opt.auto_drive ? autopilot(*g.race, g.traffic.get()) : rr::controls_from(in, zone);
        if (!opt.auto_drive && opt.custom_keys) controls |= rr::bound_control_bits(opt.key_codes, true, keys);

        const uint64_t now = SDL_GetTicksNS();
        if (lifecycle.state.take_clock_reset()) { touch.clear(); clock.reset(); last = now; }
        // Plan 18 Task 8: the race loop stops stepping the simulation while
        // pause_open(); the mixer pauses race sounds but the interface levels
        // stay live so the sound sliders apply (no preview click, as in Task 6).
        const bool menu_pause = opt.menu && opt.menu->pause_open();
        const bool frozen = lifecycle.state.paused() || g.quitting || menu_pause;
        if (g.mixer) g.mixer->pause(frozen || lifecycle.state.background());
        if (g.race_music && !frozen && !lifecycle.state.background()) g.race_music->update();
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
        if (fps_on) fps_tick_acc += steps;
        // Plan 21 Task 2: capture the new tick; blend for this frame. With
        // interpolation off, alpha=65536 reproduces the live frame exactly.
        if (steps > 0 && !frozen) {
            g.view_prev = std::move(g.view_cur);
            rr::capture_view(g.view_cur, *g.race, g.traffic.get());
            g.view_ready = true;
        }
        if (g.view_ready) {
            const uint32_t alpha = g.enhancements.interpolate ? clock.alpha_q16() : 65536;
            rr::blend_view(g.view_drawn, g.view_prev, g.view_cur, alpha);
        }
        if (lifecycle.state.background()) { SDL_Delay(20); continue; }
        if (menu_pause && opt.menu) {
            // Plan 18 Task 8: the pause menu draws over the captured frame.
            // Plan 19 Task 8: pause menu draws at the effective scale too.
            const int pause_scale = display.render_scale;
            opt.menu->draw(pause_img, pause_scale);
            if (!pause_tex || pause_scale_w != pause_scale) {
                pause_scale_w = pause_scale;
                SDL_DestroyTexture(pause_tex);
                pause_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                              640 * pause_scale, 480 * pause_scale);
                if (!pause_tex) throw std::runtime_error(SDL_GetError());
                rr::apply_texture_scale(pause_tex, display.config);
            }
            SDL_UpdateTexture(pause_tex, nullptr, pause_img.pixels.data(), 640 * pause_scale * 4);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, pause_tex, nullptr, nullptr);
        } else {
            SDL_SetRenderDrawColor(renderer, 15, 18, 22, 255);
            SDL_RenderClear(renderer);
            const auto slices = rr::project_road(g.race->route(), g.blend_camera_position(), g.blend_camera(), g.depth);
            int dw = 0, dh = 0;
            if (display.window) SDL_GetWindowSize(display.window, &dw, &dh);
            const int race_scale = opt.menu && opt.settings ? rr::effective_render_scale(*opt.settings, dw, dh, kMobileDisplay)
                                                            : opt.render_scale;
            if (opt.settings) { g.widescreen = opt.settings->widescreen; g.enhancements = opt.settings->enhancements; }
            if (g.scene) course_canvas.draw(renderer, g, slices, race_scale, display.render_threads);
            else rr::draw_road(renderer, slices);
            draw_opponents(renderer, g, sprites, slices);
            draw_bike(renderer, g, sprites,slices);
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
            }
        }
        // Plan 18 Task 9: FPS + simulation ticks/sec, top-right, Futura Bold 24
        // #DEFFF0 with a 2-pixel black shadow like all original text, twice a second.
        if (fps_on && opt.assets) {
            ++fps_frames;
            const uint64_t now_ms = now / 1000000;
            if (now_ms >= fps_next) {
                const uint64_t span = now_ms >= fps_last ? now_ms - fps_last : 1;
                fps_v = int(uint64_t(fps_frames) * 1000 / span);
                tps_v = int(uint64_t(fps_tick_acc) * 1000 / span);
                fps_frames = 0; fps_tick_acc = 0; fps_last = now_ms; fps_next = now_ms + 500;
                if (!fps_text) fps_text = std::make_unique<rr::TextRenderer>(*opt.assets);
                if (fps_text->has(rr::Face::futura_bold)) {
                    const std::string s = std::to_string(fps_v) + " FPS " + std::to_string(tps_v) + " TPS";
                    const int w = fps_text->width(rr::Face::futura_bold, 24, s) + 4;
                    fps_img = rr::RgbImage(w, 30, 0);
                    fps_text->draw(fps_img, rr::Face::futura_bold, 24, 2, 2, 0x404040, s);
                    fps_text->draw(fps_img, rr::Face::futura_bold, 24, 0, 0, 0xdefff0, s);
                    // Opaque ink over a transparent background (straight-alpha
                    // ARGB8888) so the sky shows through.
                    fps_argb.assign(size_t(w) * 30, 0);
                    for (size_t i = 0; i < fps_argb.size(); ++i)
                        if (fps_img.pixels[i]) fps_argb[i] = 0xff000000u | fps_img.pixels[i];
                    if (fps_tex) { float tw = 0, th = 0; SDL_GetTextureSize(fps_tex, &tw, &th); if (int(tw) != w || int(th) != 30) { SDL_DestroyTexture(fps_tex); fps_tex = nullptr; } }
                    if (!fps_tex) {
                        fps_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, 30);
                        if (fps_tex) SDL_SetTextureBlendMode(fps_tex, SDL_BLENDMODE_BLEND);
                    }
                    if (fps_tex) SDL_UpdateTexture(fps_tex, nullptr, fps_argb.data(), w * 4);
                }
            }
            if (fps_tex) {
                // Logical space is 640 wide in the race; pin to its top-right.
                float tw = 0, th = 0;
                SDL_GetTextureSize(fps_tex, &tw, &th);
                const SDL_FRect dst{640.f - tw - 8.f, 8.f, tw, th};
                SDL_RenderTexture(renderer, fps_tex, nullptr, &dst);
            }
        }
        if (!opt.screenshot.empty()) {
            SDL_Surface* capture = SDL_RenderReadPixels(renderer, nullptr);
            if (!capture || !SDL_SaveBMP(capture, opt.screenshot.c_str())) throw std::runtime_error(SDL_GetError());
            SDL_DestroySurface(capture);
            running = false;
            result.quit_app = true;
        }
        SDL_RenderPresent(renderer);
        display.maybe_toggle();
    }
    if (pad) SDL_CloseGamepad(pad);
    if (pause_tex) SDL_DestroyTexture(pause_tex);
    if (fps_tex) SDL_DestroyTexture(fps_tex);
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
             const std::string& screenshot, long menu_frames, const CliDisplayOverride& cli, const std::string& menu_script) {
    const std::string pref = pref_directory();
    const std::string settings_path = (std::filesystem::path(pref) / "settings.txt").string();
    rr::Settings settings = rr::load_settings(settings_path);
    apply_cli_overrides(settings, cli);
    DisplayDriver display{window, renderer, &settings, settings_path};
    display.toggle_at = cli.toggle_at;
    display.apply(480);
    draw_loading(renderer, "Preparing menus and sound...");
    const rr::TextRenderer text(assets);
    const auto tables = rr::read_career_tables(exe);
    rr::Career career;
    const auto resources = rr::read_string_resources(assets.read(rr::language_dll(settings.language)));
    const auto strings = rr::frontend_strings(resources);
    auto frontend_data = rr::read_frontend_data(exe);
    rr::add_native_screens(frontend_data, kMobileDisplay);
    rr::Frontend fe(assets, text, strings, rr::credit_lines(resources), std::move(frontend_data), tables, career, settings,
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
    rr::RaceMusicPlayer race_music(assets, mixer);  // 0x00433bb0: MIDI by course, Klown by alias
    auto apply_levels = [&] {
        rr::SoundLevels l;
        l.master = settings.master_volume; l.effects = settings.effects_volume; l.engine = settings.engine_volume;
        l.others = settings.others_volume; l.siren = settings.siren_volume; l.music = settings.music_volume;
        l.interface = settings.interface_volume; l.mute = settings.mute; l.swap_speakers = settings.swap_speakers;
        if (sound) sound->set_levels(l);
        music.set_volume(settings.mute ? 0 : 127 * settings.music_volume / 100 * settings.master_volume / 100);
    };
    apply_levels();
    if (menu_frames < 0) load_with_indicator(renderer, "Loading menu music...", [&] {
        music.play(settings.music_track);
        return 0;
    });
    g.mixer = &mixer;
    LifecycleWatch audio_lifecycle(&mixer);
    int menu_scale = 0;
    SDL_Texture* texture = nullptr;
    SDL_Gamepad* pad = open_first_gamepad();
    rr::MenuStickInput menu_stick;
    LifecycleWatch lifecycle;
    rr::RgbImage frame;
    fe.start();
    if (!menu_script.empty()) {   // debug: comma-separated screen names entered in order
        static const std::pair<const char*, int> kMenuScriptScreens[] = {
            {"main", 9}, {"restroom", 0x10}, {"graphics", 0x13}, {"display", 0x24},
            {"street", 0x0f}, {"characters", 0x0e}, {"jukebox", 0x11},
        };
        size_t from = 0;
        while (from <= menu_script.size()) {
            size_t to = menu_script.find(',', from);
            if (to == std::string::npos) to = menu_script.size();
            const std::string name = menu_script.substr(from, to - from);
            for (const auto& [key, screen] : kMenuScriptScreens)
                if (name == key) { fe.enter(screen); break; }
            from = to + 1;
        }
    }
    bool running = true, text_input = false;
    long frames = 0;
    display.frame = 0;
    while (running) {
        rr::FrontendEvent ev;
        while (running && fe.poll(ev)) {
            if (ev.kind == rr::FrontendEvent::Kind::quit) running = false;
            else if (ev.kind == rr::FrontendEvent::Kind::select_soundtrack) music.play(ev.track);
            else if (ev.kind == rr::FrontendEvent::Kind::apply_display) display.apply(480);
            else if (ev.kind == rr::FrontendEvent::Kind::apply_mixer) apply_levels();
            else if (ev.kind == rr::FrontendEvent::Kind::play_video) {
                if (menu_frames < 0) {
                    // 0x004023e0: showroom clips (flag 1) only quieten the soundtrack; other videos stop it.
                    const bool showroom = ev.video.find("/RAT/") != std::string::npos || ev.video.find("/SPORT/") != std::string::npos ||
                                          ev.video.find("/SUPER/") != std::string::npos;
                    if (showroom) music.set_volume(0); else music.stop();
                    play_video(assets, ev.video, renderer, {}, -1, display);
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
                if (ev.demo) {
                    // Attract demo (plan 13 Task 5): AI drives (DAT_004753c8); any input exits to the menu.
                    g.career = false;
                    if (g.race) g.race->set_demo_hold(true, 95);
                    music.stop();
                    race_music.play_race(ev.course, fe.player_name());
                    g.race_music = &race_music;
                    const RaceEnd demo_end = run_race(g, renderer, {.demo = true, .key_codes = settings.controls, .custom_keys = settings.custom_controls, .swap_punch_kick = settings.swap_punch_kick, .stick_deadzone = settings.stick_deadzone, .left_handed = settings.left_handed, .menu = &fe, .apply_settings = apply_levels, .show_fps = settings.show_fps, .assets = &assets, .settings = &settings}, display);
                    menu_stick.suspend();
                    race_music.stop();
                    g.race_music = nullptr;
                    settings.music_track = music.track();
                    music.play(music.track());
                    display.apply(480);
                    if (demo_end.quit_app) { running = false; break; }
                    fe.enter(rr::screen_id::main_menu);   // re-arm the 60 s timer
                    continue;
                }
                if (menu_frames >= 0) { running = false; break; }   // smoke run: stop at the first race
                music.stop();                                         // 0x004160c0 -> 0x00402f50
                // Plan 11 race-start point: race MIDI replaces the RRA soundtrack.
                // ev.course is the original 1..5 index (0x0046df58 order), matching race_song_name.
                race_music.play_race(ev.course, fe.player_name());
                g.race_music = &race_music;
                race_music.set_music_level(settings.mute ? 0 : settings.music_volume * settings.master_volume / 100);
                g.sound = sound.get();
                if (sound) sound->start_race(career.state.bike, career.state.character);
                const RaceEnd end = run_race(g, renderer, {.key_codes = settings.controls, .custom_keys = settings.custom_controls, .swap_punch_kick = settings.swap_punch_kick, .stick_deadzone = settings.stick_deadzone, .left_handed = settings.left_handed, .menu = &fe, .settings = &settings, .apply_settings = [&] { apply_levels(); race_music.set_music_level(settings.mute ? 0 : settings.music_volume * settings.master_volume / 100); }, .show_fps = settings.show_fps, .assets = &assets}, display);
                menu_stick.suspend();
                if (sound) sound->stop_race();
                race_music.stop();  // race end
                g.sound = nullptr;
                g.race_music = nullptr;
                settings.music_track = music.track();
                music.play(music.track());
                display.apply(480);
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
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                // Plan 18 Task 3: Alt+Enter and Cmd+F toggle fullscreen (saved + re-applied).
                if ((e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT)) ||
                    (e.key.key == SDLK_F && (e.key.mod & SDL_KMOD_GUI))) { display.toggle_fullscreen(); continue; }
            }
            if (e.type == SDL_EVENT_KEY_DOWN) {
                // Plan 18 Task 7: raw scancode for press-a-key capture (0 = none).
                if (!e.key.repeat && e.key.scancode > 0 && e.key.scancode < SDL_SCANCODE_COUNT)
                    in.capture_scancode = int(e.key.scancode);
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
                case SDLK_SLASH: if (!text_input && !e.key.repeat) in.opponent_key = true; break;
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
                texture = nullptr;
                menu_scale = 0;
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
        display.apply(480);
        if (menu_scale != display.render_scale) {
            menu_scale = display.render_scale;
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                        640 * menu_scale, 480 * menu_scale);
            if (!texture) throw std::runtime_error(SDL_GetError());
            rr::apply_texture_scale(texture, display.config);
        }
        fe.draw(frame, menu_scale);
        SDL_UpdateTexture(texture, nullptr, frame.pixels.data(), 640 * menu_scale * 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        if (menu_frames >= 0 && ++frames >= menu_frames) {
            if (!screenshot.empty()) {
                SDL_Surface* capture = SDL_RenderReadPixels(renderer, nullptr);
                if (!capture || !SDL_SaveBMP(capture, screenshot.c_str())) throw std::runtime_error(SDL_GetError());
                SDL_DestroySurface(capture);
            }
            running = false;
        }
        SDL_RenderPresent(renderer);
        display.maybe_toggle();
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
    if (argc == 2 && std::string(argv[1]) == "--build-info") {
        std::puts(rr::build_info_json);
        return 0;
    }
    std::string root, screenshot, video;
    int course = 0, bike = 0, level = 0, opponents = 7;
    long run_ticks = -1, menu_frames = -1;
    bool auto_drive = false, into_traffic = false, start_paused = false, first_run_smoke = false, quick_race = false;
    int render_scale = 1;   // plan 19 Task 4: --render-scale 1..4
    std::string menu_script;
    CliDisplayOverride cli;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--assets" && i + 1 < argc) root = argv[++i];
        else if (a == "--video" && i + 1 < argc) video = argv[++i];
        else if (a == "--course" && i + 1 < argc) { course = std::atoi(argv[++i]); quick_race = true; }
        else if (a == "--bike" && i + 1 < argc) { bike = std::atoi(argv[++i]); quick_race = true; }
        else if (a == "--ticks" && i + 1 < argc) { run_ticks = std::atol(argv[++i]); quick_race = true; }
        else if (a == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
        else if (a == "--autopilot") { auto_drive = true; quick_race = true; }
        else if (a == "--steer-into-traffic") { into_traffic = true; quick_race = true; }
        else if (a == "--paused") start_paused = true;
        else if (a == "--race") quick_race = true;
        else if (a == "--menu-frames" && i + 1 < argc) menu_frames = std::atol(argv[++i]);
        else if (a == "--menu-script" && i + 1 < argc) menu_script = argv[++i];
        else if (a == "--first-run-smoke") first_run_smoke = true;
        else if (a == "--level" && i + 1 < argc) { level = std::atoi(argv[++i]) - 1; quick_race = true; }
        else if (a == "--opponents" && i + 1 < argc) { opponents = std::atoi(argv[++i]); quick_race = true; }
        else if (a == "--fullscreen") cli.fullscreen = 1;
        else if (a == "--windowed") cli.fullscreen = 0;
        else if (a == "--window-scale" && i + 1 < argc) cli.window_scale = std::atoi(argv[++i]);
        else if (a == "--render-scale" && i + 1 < argc) { render_scale = std::atoi(argv[++i]); quick_race = true; }
        else if (a == "--toggle-fullscreen-at" && i + 1 < argc) cli.toggle_at = std::atol(argv[++i]);
    }
    if (course < 0 || course > 4 || level < 0 || level > 4 || opponents < 0 || opponents > 7) return 2;
    if (render_scale < 1 || render_scale > 4) return 2;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int result = 0;
    try {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) throw std::runtime_error(SDL_GetError());
        if (!SDL_CreateWindowAndRenderer("Road Rash Native", 960, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer))
            throw std::runtime_error(SDL_GetError());
        rr::Settings boot_settings = rr::load_settings((std::filesystem::path(pref_directory()) / "settings.txt").string());
        apply_cli_overrides(boot_settings, cli);
        DisplayDriver boot{window, renderer, &boot_settings, (std::filesystem::path(pref_directory()) / "settings.txt").string()};
        boot.toggle_at = cli.toggle_at;
        boot.apply(480);
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
            boot.frame = 0;
            boot.apply(480);
            play_video(assets,video,renderer,screenshot,run_ticks,boot);
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
        }
        Game g;
        const auto exe = load_with_indicator(renderer, "Reading game data...", [&] {
            auto bytes = assets.read("ROADRASH.EXE");
            g.depth = rr::read_depth_table(bytes);
            g.physics = rr::read_physics_tables(bytes);
            g.specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
            g.bobs = rr::read_bobs(assets.read("DATA/BIKERS/HIBOB.DAT"), rr::bob_frames);
            g.remaps = rr::read_rider_remaps(bytes, 9);
            g.palette = rr::Palette::from_raw(assets.read("DATA/PALETTE.RAW"));
            return bytes;
        });
        if (!quick_race) {
            boot.apply(480);
            result = run_game(assets, window, renderer, g, exe, screenshot, menu_frames, cli, menu_script);
        } else {
            g.course_name = course_names[course];
            if (bike < 0 || size_t(bike) >= g.specs.size()) throw std::runtime_error("Bike index out of range");
            g.bike = bike;
            g.level = level;
            g.load_course(assets,exe);
            g.opponents = rr::original_opponents(exe, g.specs, level, opponents);
            // Dashboard target names: personality +0x165 -> 0x00464188 ->
            // string slot (0x00437860), as the career path resolves them.
            {
                const auto strings = rr::frontend_strings(rr::read_string_resources(assets.read(rr::language_dll(rr::default_language()))));
                const auto name_table = rr::read_exe_data(exe, 0x00464188, 9 * 4);
                g.rider_names.assign(1, g.player_name);
                for (const auto& o : g.opponents) {
                    const int p = o.rider;
                    const uint32_t slot = p >= 0 && p < 9 ? uint32_t(name_table[size_t(p) * 4]) | uint32_t(name_table[size_t(p) * 4 + 1]) << 8 : 0;
                    g.rider_names.push_back(slot && slot < strings.size() ? strings[slot] : std::string());
                }
            }
            g.restart();
            if (run_ticks >= 0) {   // headless-style verification run
                const bool hunt = into_traffic;
                if (hunt) auto_drive = true;
                int car_crashes = 0, any_crash = 0;
                long t = 0;
                for (; t < run_ticks && !g.race->finished(); ++t) {
                    g.tick(hunt ? steer_into_traffic(*g.race) : (auto_drive ? autopilot(*g.race, g.traffic.get()) : 0));
                    const auto& b = g.race->bike();
                    if (hunt && t % 1000 == 0) {
                        int active = 0;
                        if (g.traffic) for (const auto& car : g.traffic->cars()) if (car.active) ++active;
                        std::printf("hunt t=%ld pos=%d lat=%d speed=%d crash=%d reason=%d cars=%d\n",
                                    t, b.i32(0x1c), b.i32(0x28), b.i32(0xec),
                                    b.i32(0x2f4), b.i32(0x2f8), active);
                        std::fflush(stdout);
                    }
                    if (b.i32(0x2f4) != 0 && any_crash == 0) {
                        any_crash = 1;
                        std::printf("crash tick %ld reason %d speed %d pos %d lat %d\n",
                                    t, b.i32(0x2f8), b.i32(0xec), b.i32(0x1c), b.i32(0x28));
                        std::fflush(stdout);
                    } else if (b.i32(0x2f4) == 0) {
                        any_crash = 0;   // re-arm after remount
                        car_crashes = 0;
                    }
                    if (b.i32(0x2f4) != 0 && b.i32(0x2f8) == 2 && car_crashes == 0) {
                        std::printf("car-crash tick %ld reason 2 speed %d pos %d\n",
                                    t, b.i32(0xec), b.i32(0x1c));
                        std::fflush(stdout);
                        ++car_crashes;
                    }
                }
                std::printf("end tick %ld pos %u of %zu finished %d place %d\n", t, g.race->position() >> 8,
                            g.race->route().size(), int(g.race->finished()), g.race->place(0));
                if (g.traffic) {
                    int cars = 0, crossing = 0, controllers = 0;
                    for (const auto& car : g.traffic->cars()) if (car.active) { ++cars; crossing += car.kind & 1; }
                    for (const auto& c : g.traffic->controllers()) controllers += c.active;
                    std::printf("traffic cars %d crossing %d intersections %d\n", cars, crossing, controllers);
                }
                if (g.pedestrians) {
                    int active = 0, crossing = 0, down = 0;
                    for (const auto& p : g.pedestrians->pool()) if (p.active) { ++active; crossing += p.state == 1; down += p.state == 2; }
                    std::printf("pedestrians spawned %d active %d crossing %d down %d\n", g.pedestrians->spawned(), active, crossing, down);
                }
                std::fflush(stdout);
            }
            if (!into_traffic) {
                rr::Mixer mixer(48);
                rr::RaceMusicPlayer race_music(assets, mixer);
                std::unique_ptr<rr::SoundSystem> sound;
                if (run_ticks < 0) {
                    const auto settings = rr::load_settings((std::filesystem::path(pref_directory()) / "settings.txt").string());
                    if (!SDL_InitSubSystem(SDL_INIT_AUDIO) || !mixer.open_device()) SDL_Log("Audio unavailable: %s", SDL_GetError());
                    sound = std::make_unique<rr::SoundSystem>(assets, mixer, rr::read_volume_curve(exe), int(rr::read_exe_data(exe, 0x0046dce5, 1)[0]));
                    rr::SoundLevels levels;
                    levels.master = settings.master_volume; levels.engine = settings.engine_volume;
                    levels.effects = settings.effects_volume; levels.others = settings.others_volume;
                    levels.mute = settings.mute; levels.swap_speakers = settings.swap_speakers;
                    sound->set_levels(levels);
                    sound->start_race(bike, 0);
                    // Public quick-race order differs from the original course index.
                    const int original_index[] = {4, 1, 2, 5, 3};
                    race_music.set_music_level(settings.mute ? 0 : settings.music_volume * settings.master_volume / 100);
                    race_music.play_race(original_index[course], "");
                    g.mixer = &mixer; g.sound = sound.get(); g.race_music = &race_music;
                }
                boot.frame = 0;
                boot.apply(480);
                run_race(g, renderer, {.auto_drive = auto_drive, .start_paused = start_paused, .screenshot = screenshot, .key_codes = boot_settings.controls, .custom_keys = boot_settings.custom_controls, .swap_punch_kick = boot_settings.swap_punch_kick, .stick_deadzone = boot_settings.stick_deadzone, .left_handed = boot_settings.left_handed, .show_fps = boot_settings.show_fps, .assets = &assets, .render_scale = render_scale, .settings = &boot_settings}, boot);
                g.mixer = nullptr; g.sound = nullptr; g.race_music = nullptr;
            }
        }
    } catch (const std::exception& e) { SDL_Log("%s", e.what()); result = 1; }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
