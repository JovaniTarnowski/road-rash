// Benchmark: Canvas::draw ms/frame on Napa after 900 sim ticks.
// Local desktop bench, built only with -DRR_BENCH_DASH=ON (not a CTest). Usage:
//   RR_PREF_OVERRIDE=/tmp/ws-orig ./build/dash_bench-test  (needs SDL video; uses hidden window)
// Exits 1 if SDL setup fails or the race has no scene to draw.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <SDL3/SDL.h>
#include "assets.hpp"
#include "game_types.hpp"
#include "race_caches.hpp"
#include "race_setup.hpp"
#include "road_draw.hpp"

static int sdl_fail(const char* what) {
    std::fprintf(stderr, "dash_bench: %s failed: %s\n", what, SDL_GetError());
    SDL_Quit();
    return 1;
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return sdl_fail("SDL_Init");
    SDL_Window* window = SDL_CreateWindow("bench", 640, 480, SDL_WINDOW_HIDDEN);
    if (!window) return sdl_fail("SDL_CreateWindow");
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(window);
        return sdl_fail("SDL_CreateRenderer");
    }
    const std::string assets_root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    rr::Assets assets(assets_root);
    const std::vector<uint8_t> exe = assets.read("ROADRASH.EXE");
    rr::Settings settings = rr::load_settings(
        (std::filesystem::path(rr::pref_directory()) / "settings.txt").string());
    rr::Game g;
    g.depth = rr::read_depth_table(exe);
    g.config.course = "NAPA";
    g.config.level = 0;
    g.config.career = false;
    g.config.opponents = 7;
    g.load_course(assets, exe);
    g.restart();
    for (int t = 0; t < 900; ++t) g.tick(rr::control_accelerate);
    g.widescreen = settings.widescreen;
    g.enhancements = settings.enhancements;
    g.texture_detail = settings.texture_detail;
    if (!g.scene()) {
        std::fprintf(stderr, "dash_bench: no course scene after load\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    rr::CourseCanvas canvas;
    // Same projection as RaceScreen: slices centre on the Hor+ logical width.
    const auto slices = rr::project_road(g.race().route(), g.blend_camera_position(),
                                         g.blend_camera(), g.depth, g.logical_width());
    const int s = 2, threads = 8;
    canvas.draw(renderer, g, slices, s, threads);   // warm-up (workers, caches, texture)
    const int frames = 10;
    const auto start = std::chrono::steady_clock::now();
    for (int f = 0; f < frames; ++f) canvas.draw(renderer, g, slices, s, threads);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - start).count() / frames;
    std::printf("draw_ms=%.2f scale=%d logical_width=%d wide=%d filter=%d fog=%d grade=%d bloom=%d\n", ms, s,
                int(g.logical_width()), int(settings.widescreen), int(settings.enhancements.filter), settings.enhancements.fog,
                int(settings.enhancements.grade), int(settings.enhancements.bloom));
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
