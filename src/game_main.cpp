// Playable single-course race: original bike physics and road projection,
// placeholder shading (the original raster, sprites and HUD are plans 02/03/06).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "assets.hpp"
#include "input.hpp"
#include "race.hpp"
#include "road_draw.hpp"
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

    void restart() {
        race = std::make_unique<rr::Race>(rr::make_route(course, false), rr::make_route(course, true),
                                          physics, specs.at(size_t(bike)), 90);
        countdown = countdown_ticks;
    }
    void tick(uint32_t controls) {
        if (countdown > 0) { --countdown; race->step(controls & ~(rr::control_accelerate | rr::control_left | rr::control_right)); return; }
        race->step(controls);
    }
};

// Keep-to-centre driver used by --autopilot for demos and smoke runs.
uint32_t autopilot(const rr::Race& race) {
    const auto& s = race.route()[race.position() >> 8];
    const int x = race.lateral() - s.offset;
    uint32_t c = rr::control_accelerate;
    if (x < -60) c |= rr::control_right; else if (x > 60) c |= rr::control_left;
    return c;
}

void quad(SDL_Renderer* r, float x0, float x1, float y0, float x2, float x3, float y1, SDL_FColor c) {
    const SDL_Vertex v[4] = {{{x0,y0},c,{}}, {{x1,y0},c,{}}, {{x3,y1},c,{}}, {{x2,y1},c,{}}};
    const int index[6] = {0,1,2,0,2,3};
    SDL_RenderGeometry(r, nullptr, v, 4, index, 6);
}

// Placeholder rider marker: projected lateral position at the bike's depth, pinned near the bottom.
void draw_bike(SDL_Renderer* r, const Game& g) {
    const rr::Race& race = *g.race;
    const int32_t depth = int32_t(race.position() - race.camera_position());
    const size_t index = size_t(depth & ~1) / 2;
    const int32_t scale = g.depth.scale[std::min(index, g.depth.scale.size() - 1)];
    const auto& road = race.route()[race.camera_position() >> 8];
    const int64_t rel = int64_t(race.lateral() - road.offset - race.camera().x);
    float x = float((rel * scale) >> 16) + 160.f;
    if (x < 20) x = 20; if (x > 300) x = 300;
    const float s = 2.f, bx = x * s, by = 478.f;
    const float lean = race.crashed() ? 40.f : float(race.bike().i32(0x288)) / 400.f;
    quad(r, bx - 18 + lean, bx + 18 + lean, by - 90, bx - 26, bx + 26, by - 20, {.10f, .10f, .12f, 1});   // rider
    quad(r, bx - 8 + lean, bx + 8 + lean, by - 110, bx - 8 + lean, bx + 8 + lean, by - 90, {.75f, .15f, .12f, 1});  // helmet
    quad(r, bx - 10, bx + 10, by - 26, bx - 12, bx + 12, by, {.05f, .05f, .05f, 1});                     // wheel
}

void draw_hud(SDL_Renderer* r, const Game& g) {
    const rr::Race& race = *g.race;
    char line[160];
    const int seconds = race.ticks() / rr::ticks_per_second;
    const int progress = int(uint64_t(race.position() >> 8) * 100 / (race.route().size() - 1));
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_snprintf(line, sizeof line, "%s  SPEED %3d  GEAR %d  TIME %d:%02d  %3d%%", g.course_name.c_str(),
                 race.speed() / 100, race.gear() + 1, seconds / 60, seconds % 60, progress);
    SDL_RenderDebugText(r, 8, 8, line);
    if (g.countdown > 0) {
        SDL_snprintf(line, sizeof line, "GET READY  %d", g.countdown / rr::ticks_per_second + 1);
        SDL_SetRenderScale(r, 3, 3);
        SDL_RenderDebugText(r, 60, 60, line);
        SDL_SetRenderScale(r, 1, 1);
    } else if (race.finished()) {
        SDL_snprintf(line, sizeof line, "FINISHED  %d:%02d", seconds / 60, seconds % 60);
        SDL_SetRenderScale(r, 3, 3);
        SDL_RenderDebugText(r, 50, 60, line);
        SDL_SetRenderScale(r, 1, 1);
        SDL_RenderDebugText(r, 200, 260, "Enter / A / tap: race again");
    } else if (race.crashed()) {
        SDL_SetRenderScale(r, 3, 3);
        SDL_RenderDebugText(r, 80, 60, "WIPEOUT!");
        SDL_SetRenderScale(r, 1, 1);
    }
    SDL_RenderDebugText(r, 8, 483, "Arrows/WASD or gamepad: Up accelerate, Down brake, Left/Right steer");
    SDL_RenderDebugText(r, 8, 497, "Original physics + road geometry; placeholder graphics. Esc quits");
}
}

int main(int argc, char** argv) {
    std::string root = "game", screenshot;
    int course = 0, bike = 0;
    long run_ticks = -1;
    bool auto_drive = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--assets" && i + 1 < argc) root = argv[++i];
        else if (a == "--course" && i + 1 < argc) course = std::atoi(argv[++i]);
        else if (a == "--bike" && i + 1 < argc) bike = std::atoi(argv[++i]);
        else if (a == "--ticks" && i + 1 < argc) run_ticks = std::atol(argv[++i]);
        else if (a == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
        else if (a == "--autopilot") auto_drive = true;
        else { std::fprintf(stderr, "Usage: rr-game [--assets DIR] [--course 0-4] [--bike 0-14] [--autopilot] [--ticks N --screenshot FILE.bmp]\n"); return 2; }
    }
    if (course < 0 || course > 4) return 2;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int result = 0;
    try {
        const rr::Assets assets(root);
        Game g;
        const auto exe = assets.read("ROADRASH.EXE");
        g.depth = rr::read_depth_table(exe);
        g.physics = rr::read_physics_tables(exe);
        g.specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        g.course_name = course_names[course];
        g.course = rr::read_course(assets.read("DATA/COURSES/" + g.course_name + ".CRS"));
        if (bike < 0 || size_t(bike) >= g.specs.size()) throw std::runtime_error("Bike index out of range");
        g.bike = bike;
        g.restart();

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) throw std::runtime_error(SDL_GetError());
        if (!SDL_CreateWindowAndRenderer("Road Rash Native", 960, 760, SDL_WINDOW_RESIZABLE, &window, &renderer))
            throw std::runtime_error(SDL_GetError());
        SDL_SetRenderLogicalPresentation(renderer, 640, 510, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderVSync(renderer, 1);

        if (run_ticks >= 0) {   // headless-style verification run
            for (long t = 0; t < run_ticks && !g.race->finished(); ++t) g.tick(auto_drive ? autopilot(*g.race) : 0);
        }
        SDL_Gamepad* pad = nullptr;
        bool running = true, touching = false;
        rr::PlayerInput touch;
        uint64_t last = SDL_GetTicksNS(), accumulator = 0;
        const uint64_t tick_ns = 1000000000ull / rr::ticks_per_second;
        while (running) {
            SDL_Event e;
            bool restart = false;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_AC_BACK) running = false;
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_RETURN && g.race->finished()) restart = true;
                if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) pad = SDL_OpenGamepad(e.gdevice.which);
                if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad) == e.gdevice.which) { SDL_CloseGamepad(pad); pad = nullptr; }
                if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH && g.race->finished()) restart = true;
                if (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION) {
                    touching = true;
                    touch = rr::touch_input(e.tfinger.x, e.tfinger.y, 1.f, 1.f);
                    if (e.type == SDL_EVENT_FINGER_DOWN && g.race->finished()) restart = true;
                }
                if (e.type == SDL_EVENT_FINGER_UP) touching = false;
                if (e.type == SDL_EVENT_WILL_ENTER_BACKGROUND) accumulator = 0;
            }
            if (restart) g.restart();
            rr::PlayerInput in = touching ? touch : rr::PlayerInput{};
            const bool* keys = SDL_GetKeyboardState(nullptr);
            in.accelerate |= keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
            in.brake |= keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
            in.left |= keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
            in.right |= keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
            if (pad) {
                in.accelerate |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) || SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000;
                in.brake |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000;
                in.left |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
                in.right |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
                in.steer_axis = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            }
            const uint32_t controls = auto_drive ? autopilot(*g.race) : rr::controls_from(in);

            const uint64_t now = SDL_GetTicksNS();
            accumulator += now - last;
            last = now;
            int steps = 0;
            while (accumulator >= tick_ns && steps < rr::max_steps_per_frame) { g.tick(controls); accumulator -= tick_ns; ++steps; }
            if (steps == rr::max_steps_per_frame) accumulator = 0;

            SDL_SetRenderDrawColor(renderer, 15, 18, 22, 255);
            SDL_RenderClear(renderer);
            rr::draw_road(renderer, rr::project_road(g.race->route(), g.race->camera_position(), g.race->camera(), g.depth));
            draw_bike(renderer, g);
            draw_hud(renderer, g);
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
