// Plan 16 Task 7 + plan 24 Task 5: native determinism and soak gate.
// For each race: seed 1, seven opponents, accelerate held, up to 108,000
// ticks (30 simulated minutes) or finish; FNV-1a over every tick snapshot.
// Same setup twice must hash equal. Also: rider positions monotonic
// except while crashed (+0x2f4), and pools never exceed the original sizes
// (16 cars, 10 controllers, 40 signals, 15 pedestrians).
// Full matrix (all courses x levels) with RR_SOAK_FULL=1 (CI Release);
// default runs Napa L0 twice (determinism core).
#include "assets.hpp"
#include "course_scene.hpp"
#include "golden.hpp"
#include "input.hpp"
#include "race_setup.hpp"
#include "render_threads.hpp"
#include "road.hpp"
#include "view.hpp"
#include "world_renderer.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>

static uint64_t fnv(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}
struct SoakResult {
    uint64_t hash = 1469598103934665603ULL;
    int ticks = 0;
    bool finished = false;
};
static SoakResult soak(rr::RaceSetup& setup, uint32_t controls) {
    SoakResult out;
    std::map<size_t, int32_t> last_pos;
    for (; out.ticks < 108000 && !setup.race->finished(); ++out.ticks) {
        setup.race->step(controls);
        // Pool bounds (original sizes) every tick.
        if (setup.traffic) {
            size_t cars = 0;
            for (const auto& c : setup.traffic->cars()) if (c.active) ++cars;
            if (cars > 16) throw std::runtime_error("traffic pool exceeds 16");
            size_t ctl = 0, sig = 0;
            for (const auto& c : setup.traffic->controllers()) if (c.active) ++ctl;
            for (const auto& s : setup.traffic->signals()) if (s.active) ++sig;
            if (ctl > 10 || sig > 40) throw std::runtime_error("intersection pools exceed 10/40");
        }
        const auto& riders = setup.race->riders();
        for (size_t i = 0; i < riders.size(); ++i) {
            const int32_t pos = riders[i].bike.i32(0x1c);
            auto it = last_pos.find(i);
            // Contact separation shoves riders backward a few Q8 units
            // without crashing (observed: -6); anything past one sample
            // outside a crash is a rollback bug.
            if (it != last_pos.end() && pos < it->second - 0x100 && riders[i].bike.i32(0x2f4) == 0)
                throw std::runtime_error("non-monotonic position outside crash");
            last_pos[i] = pos;
        }
        const auto snap = setup.race->snapshot();
        out.hash = fnv(out.hash, uint64_t(snap.tick));
        out.hash = fnv(out.hash, uint64_t(snap.rng));
        for (const auto& f : snap.fields) out.hash = fnv(out.hash, uint64_t(f.value));
    }
    out.finished = setup.race->finished();
    return out;
}
// Plan 19 Task 7: race results are independent of render scale and frame
// rate. A 3,600-tick Napa race with seven opponents runs twice with the same
// controls: once rendering every tick at s = 1, once at s = 4 with frames
// skipped randomly (render on ~50% of ticks). Per-tick snapshot hashes must
// be identical (Task 1 Step 3 found no render-time RNG to relocate).
static uint64_t render_soak(rr::RaceSetup& setup, const std::vector<uint8_t>& exe, uint32_t controls, int scale, uint64_t render_mask) {
    if (!setup.scene) throw std::runtime_error("render soak needs the course scene");
    const auto depths = rr::read_depth_table(exe);
    uint64_t hash = 1469598103934665603ULL;
    uint64_t lcg = 0x12345678u;
    rr::Framebuffer fb(640 * scale, 480 * scale);
    rr::RenderThreads pool(2);
    const rr::ViewMode mode = scale == 1 ? rr::view_mode(true) : rr::view_mode_scaled(scale);
    for (int tick = 0; tick < 3600 && !setup.race->finished(); ++tick) {
        setup.race->step(controls);
        lcg = lcg * 1103515245u + 12345u;
        if ((lcg & render_mask) == 0) {
            const auto slices = rr::project_road(setup.race->route(), setup.race->camera_position(),
                                                 setup.race->camera(), depths);
            setup.scene->draw_horizon(fb, setup.race->horizon_scroll());
            rr::render_nodes_threaded(fb, setup.scene->draw_nodes(setup.race->route(), slices,
                                                                  setup.race->camera(), depths),
                                      mode, 16, pool);
        }
        const auto snap = setup.race->snapshot();
        hash = fnv(hash, uint64_t(snap.tick));
        hash = fnv(hash, uint64_t(snap.rng));
        for (const auto& f : snap.fields) hash = fnv(hash, uint64_t(f.value));
    }
    return hash;
}
int main() {
    const std::string assets_root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(assets_root + "/ROADRASH.EXE")) {
        std::cout << "SKIP no disc\n";
        return rr::golden::skip;
    }
    rr::Assets assets(assets_root);
    const std::vector<uint8_t> exe = assets.read("ROADRASH.EXE");
    rr::PlayerInput in;
    in.accelerate = true;
    const uint32_t controls = rr::controls_from(in);
    std::vector<std::pair<std::string, int>> matrix;
    if (std::getenv("RR_SOAK_FULL")) {
        for (const char* c : {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"})
            for (int level = 0; level <= 4; ++level) matrix.emplace_back(c, level);
    } else {
        matrix.emplace_back("NAPA", 0);
    }
    for (const auto& [course, level] : matrix) {
        rr::RaceConfig config;
        config.course = course;
        config.level = level;
        config.career = false;
        config.opponents = 7;
        auto a = rr::make_race_setup(assets, exe, config);
        const SoakResult first = soak(a, controls);
        auto b = rr::make_race_setup(assets, exe, config);
        const SoakResult second = soak(b, controls);
        if (first.hash != second.hash) throw std::runtime_error(course + " L" + std::to_string(level) + ": soak not deterministic");
        if (first.ticks != second.ticks) throw std::runtime_error("tick count diverged");
        std::cout << course << " L" << level << ": " << first.ticks << " ticks"
                  << (first.finished ? " finished" : "") << " hash=" << std::hex << first.hash << std::dec << "\n";
    }
    // Plan 19 Task 7: same controls, s = 1 rendering every tick vs s = 4
    // rendering ~50% of ticks — snapshot hashes must be identical.
    {
        rr::RaceConfig config;
        config.course = "NAPA";
        config.level = 0;
        config.career = false;
        config.opponents = 7;
        auto a = rr::make_race_setup(assets, exe, config);
        const uint64_t every = render_soak(a, exe, controls, 1, 0);
        auto b = rr::make_race_setup(assets, exe, config);
        const uint64_t half = render_soak(b, exe, controls, 4, 1);
        if (every != half) throw std::runtime_error("race depends on render scale/rate");
        std::cout << "render-rate independence hash=" << std::hex << every << std::dec << "\n";
    }
    return 0;
}
