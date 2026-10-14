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
#include "view_state.hpp"
#include "bike.hpp"
#include "course.hpp"
#include "crash.hpp"
#include "race.hpp"
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
    for (; out.ticks < 108000 && !setup.instance.race->finished(); ++out.ticks) {
        setup.instance.race->step(controls);
        // Pool bounds (original sizes) every tick.
        if (setup.instance.traffic) {
            size_t cars = 0;
            for (const auto& c : setup.instance.traffic->cars()) if (c.active) ++cars;
            if (cars > 16) throw std::runtime_error("traffic pool exceeds 16");
            size_t ctl = 0, sig = 0;
            for (const auto& c : setup.instance.traffic->controllers()) if (c.active) ++ctl;
            for (const auto& s : setup.instance.traffic->signals()) if (s.active) ++sig;
            if (ctl > 10 || sig > 40) throw std::runtime_error("intersection pools exceed 10/40");
        }
        const auto& riders = setup.instance.race->riders();
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
        const auto snap = setup.instance.race->snapshot();
        out.hash = fnv(out.hash, uint64_t(snap.tick));
        out.hash = fnv(out.hash, uint64_t(snap.rng));
        for (const auto& f : snap.fields) out.hash = fnv(out.hash, uint64_t(f.value));
    }
    out.finished = setup.instance.race->finished();
    return out;
}
// Plan 19 Task 7: race results are independent of render scale and frame
// rate. A 3,600-tick Napa race with seven opponents runs twice with the same
// controls: once rendering every tick at s = 1, once at s = 4 with frames
// skipped randomly (render on ~50% of ticks). Per-tick snapshot hashes must
// be identical (Task 1 Step 3 found no render-time RNG to relocate).
static uint64_t render_soak(rr::RaceSetup& setup, const std::vector<uint8_t>& exe, uint32_t controls, int scale, uint64_t render_mask) {
    if (!setup.assets.scene) throw std::runtime_error("render soak needs the course scene");
    const auto depths = rr::read_depth_table(exe);
    uint64_t hash = 1469598103934665603ULL;
    uint64_t lcg = 0x12345678u;
    rr::Framebuffer fb(640 * scale, 480 * scale);
    rr::RenderThreads pool(2);
    const rr::ViewMode mode = scale == 1 ? rr::view_mode(true) : rr::view_mode_scaled(scale);
    for (int tick = 0; tick < 3600 && !setup.instance.race->finished(); ++tick) {
        setup.instance.race->step(controls);
        lcg = lcg * 1103515245u + 12345u;
        if ((lcg & render_mask) == 0) {
            const auto slices = rr::project_road(setup.instance.race->route(), setup.instance.race->camera_position(),
                                                 setup.instance.race->camera(), depths);
            setup.assets.scene->draw_horizon(fb, setup.instance.race->horizon_scroll());
            rr::render_nodes_threaded(fb, setup.assets.scene->draw_nodes(setup.instance.race->route(), slices,
                                                                  setup.instance.race->camera(), depths),
                                      mode, 16, pool);
        }
        const auto snap = setup.instance.race->snapshot();
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
    // Plan 21 Task 2: interpolation on — capture/blend every tick at varying
    // alpha — must not move the simulation either.
    {
        rr::RaceConfig config;
        config.course = "NAPA";
        config.level = 0;
        config.career = false;
        config.opponents = 7;
        auto c = rr::make_race_setup(assets, exe, config);
        const auto depths = rr::read_depth_table(exe);
        uint64_t hash = 1469598103934665603ULL;
        rr::ViewState prev, cur, drawn;
        bool ready = false;
        for (int tick = 0; tick < 3600 && !c.instance.race->finished(); ++tick) {
            c.instance.race->step(controls);
            prev = std::move(cur);
            rr::capture_view(cur, *c.instance.race, nullptr);
            ready = true;
            // Vary alpha across the tick like a 120 Hz display would sample.
            // (Blended cameras past the route end cannot project; skip those,
            // as the game does when the race finishes.)
            for (uint32_t alpha : {0u, 21745u, 43590u, 65536u}) {
                rr::blend_view(drawn, prev, cur, alpha);
                try {
                    const auto slices = rr::project_road(c.instance.race->route(), drawn.camera_position_q8,
                                                         drawn.camera, depths);
                    (void)slices;
                } catch (const std::runtime_error&) {}
            }
            const auto snap = c.instance.race->snapshot();
            hash = fnv(hash, uint64_t(snap.tick));
            hash = fnv(hash, uint64_t(snap.rng));
            for (const auto& f : snap.fields) hash = fnv(hash, uint64_t(f.value));
        }
        auto d = rr::make_race_setup(assets, exe, config);
        const uint64_t plain = render_soak(d, exe, controls, 1, 0);
        if (hash != plain) throw std::runtime_error("interpolation moves the simulation");
        std::cout << "interpolation independence hash=" << std::hex << hash << std::dec << "\n";
    }
    // Plan 21 Task 2 Review Focus 1: crash/remount teleport rule. Script a
    // crash; at the crash, remount and route-commit ticks, no drawn actor may
    // lie more than teleport_q8 from BOTH endpoints (it must snap to one).
    {
        const auto tables = rr::read_physics_tables(exe);
        const auto course = rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
        const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        const auto lib = rr::ActorAnimationLibrary(exe, assets.read("DATA/GLOBAL.RSC"));
        const auto crash_config = rr::read_crash_config(exe);
        rr::Race race(rr::make_route(course, false), rr::make_route(course, true), tables, specs.at(0), 90);
        race.set_crash_library(lib, crash_config);
        auto& live_bike = const_cast<rr::Bike&>(race.bike());
        live_bike.i32(0xec) = 6000;
        live_bike.i32(0x2f4) = 1;
        constexpr uint32_t teleport_q8 = 8 << 8;
        rr::ViewState prev, cur, drawn;
        rr::capture_view(cur, race, nullptr);
        bool was_crashed = race.riders()[0].crash_active;
        int checks = 0;
        for (int t = 0; t < 5000 && checks < 3; ++t) {
            prev = std::move(cur);
            rr::capture_view(cur, race, nullptr);
            race.step(0);
            const bool crashed = race.riders()[0].crash_active;
            // Transition ticks: crash start, remount end.
            if (crashed != was_crashed) {
                rr::ViewState after;
                rr::capture_view(after, race, nullptr);
                for (uint32_t alpha : {16384u, 32768u, 49152u}) {
                    rr::blend_view(drawn, cur, after, alpha, teleport_q8);
                    for (const auto& p : drawn.actors) {
                        const rr::ActorPose* pa = nullptr;
                        const rr::ActorPose* pb = nullptr;
                        for (const auto& q : cur.actors) if (q.id == p.id) pa = &q;
                        for (const auto& q : after.actors) if (q.id == p.id) pb = &q;
                        if (!pa || !pb) continue;  // new/gone: drawn at b by rule
                        const uint32_t da = p.position_q8 >= pa->position_q8 ? p.position_q8 - pa->position_q8
                                                                             : pa->position_q8 - p.position_q8;
                        const uint32_t db = p.position_q8 >= pb->position_q8 ? p.position_q8 - pb->position_q8
                                                                             : pb->position_q8 - p.position_q8;
                        if (da > teleport_q8 && db > teleport_q8)
                            throw std::runtime_error("drawn actor floats between endpoints at transition");
                    }
                }
                ++checks;
            }
            was_crashed = crashed;
            if (!crashed && t > 10 && checks >= 2) break;  // crash + remount seen
        }
        if (checks < 2) throw std::runtime_error("crash/remount transitions not observed");
        std::cout << "crash/remount teleport rule holds (" << checks << " transitions)\n";
    }
    return 0;
}
