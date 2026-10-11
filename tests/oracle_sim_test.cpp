#include "assets.hpp"
#include "bike.hpp"
#include "course.hpp"
#include "course_scene.hpp"
#include "golden.hpp"
#include "race.hpp"
#include "traffic.hpp"
#include <filesystem>
#include <iostream>

// Plan 10 task 4: replay a sim golden through rr::Race and report the first
// divergent tick and field. Exit 0 on full match, 1 on divergence, 77 when
// the golden is missing. Car/node comparison is plan 10 task 7 work.
int main(int argc, char** argv) {
    const std::string file = argc > 1 ? argv[1] : rr::golden::oracle_dir() + "/sim-napa-solo.tsv";
    if (!std::filesystem::exists(file)) { std::cout << "SKIP missing " << file << '\n'; return rr::golden::skip; }
    const auto g = rr::golden::load(file);
    if (g.exe != "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2") throw std::runtime_error("different executable");
    if (g.frames.empty()) throw std::runtime_error("no frames");
    const auto& h = g.frames.at(0);
    auto get = [&](const char* k) { auto it = h.find(k); if (it == h.end()) throw std::runtime_error(std::string("header lacks ") + k); return it->second; };
    static const char* original_courses[] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};  // 0x0046df58
    const int course_entry = int(get("course"));
    if (course_entry < 1 || course_entry > 5) throw std::runtime_error("course out of range");
    const int level = int(get("level"));
    const std::string assets_root = argc > 2 ? argv[2] : std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(assets_root + "/ROADRASH.EXE")) { std::cout << "SKIP no disc\n"; return rr::golden::skip; }
    // Same construction as Game::load_course/restart in src/game_main.cpp:
    // default bike/character, seven opponents, traffic density 6.
    // A solo golden (header solo=1) builds the player alone with no traffic.
    rr::Assets assets(assets_root);
    std::vector<uint8_t> exe = assets.read("ROADRASH.EXE");
    const std::string course_name = original_courses[course_entry - 1];
    rr::Course course = rr::read_course(assets.read("DATA/COURSES/" + course_name + ".CRS"));
    const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
    const auto physics = rr::read_physics_tables(exe);
    const bool solo = h.find("solo") != h.end() && h.at("solo") != 0;
    const auto opponents = solo ? std::vector<rr::Opponent>{} : rr::original_opponents(exe, specs, level, 7);
    rr::CourseScene scene(assets, exe, course, course_name, 1, level);
    rr::VehicleLibrary vehicles(assets.read("DATA/CARS/" + course_name + ".CAR"));
    rr::TrafficSystem traffic(vehicles, exe);
    traffic.configure(6);
    rr::Race race(rr::make_route(course, false), rr::make_route(course, true),
                  physics, specs.at(0), 90, opponents, level);
    race.set_terrain_source([&](const rr::RouteSample& s) -> const rr::TerrainSlice& { return scene.terrain(s.section, s.local); });
    if (!solo) race.set_traffic(&traffic);
    race.set_rng_state(uint32_t(get("seed")));
    const int64_t tick0 = get("tick");
    std::vector<std::pair<int64_t, uint64_t>> rng_history;  // (golden, native)
    for (size_t i = 0; i < g.frames.size(); ++i) {
        const auto& frame = g.frames[i];
        auto fc = frame.find("controls");
        if (fc == frame.end()) throw std::runtime_error("frame lacks controls");
        race.step(uint32_t(fc->second));
        const auto snap = race.snapshot();
        const auto fr = frame.find("rng");
        const uint64_t want_rng = fr == frame.end() ? 0 : uint64_t(fr->second);
        rng_history.push_back({want_rng, snap.rng});
        if (rng_history.size() > 8) rng_history.erase(rng_history.begin());
        const auto ft = frame.find("tick");
        if (ft != frame.end() && ft->second != tick0 + int64_t(i)) {
            std::cout << "tick not contiguous at frame " << i << '\n';
            return 1;
        }
        if (want_rng != snap.rng) {
            std::cout << "first divergence at frame " << i << " (tick " << tick0 + int64_t(i) << "): rng original " << want_rng << " native " << snap.rng << '\n';
            const int64_t first = tick0 + int64_t(i) - int64_t(rng_history.size()) + 1;
            for (size_t k = 0; k < rng_history.size(); ++k)
                std::cout << "  rng tick " << (first + int64_t(k)) << " original " << rng_history[k].first << " native " << rng_history[k].second << '\n';
            return 1;
        }
        for (const auto& f : snap.fields) {
            auto it = frame.find(f.name);
            if (it == frame.end()) continue;  // node-mapped fields are task 6/7 work
            if (it->second != f.value) {
                std::cout << "first divergence at frame " << i << " (tick " << tick0 + int64_t(i) << "): " << f.name << " original " << it->second << " native " << f.value << '\n';
                return 1;
            }
        }
    }
    std::cout << g.frames.size() << " ticks identical\n";
    return 0;
}
