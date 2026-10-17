// Diagnostic (plan 10 Task 5): locate the original ring setup in the race-setup
// RNG stream. The MSVC LCG is invertible, so for each candidate K the stream
// state K draws before the green anchor seeds TrackRing::setup; the candidate
// is scored by how many in-race slice crossings then consume exactly the
// draws the golden shows (traffic timers may add 1-2 after the ring).
// Usage: ring-probe <golden.tsv> <disc root> [max K] [police-at-setup 0|1]
#include "assets.hpp"
#include "course.hpp"
#include "golden.hpp"
#include "track_ring.hpp"
#include <cstdio>
#include <iostream>
#include <map>
#include <string>

namespace {
uint32_t step_back(uint32_t s) {
    uint32_t inv = 0x343fd;   // Newton iteration for the inverse of 214013 mod 2^32
    for (int i = 0; i < 5; ++i) inv *= 2u - 0x343fdu * inv;
    return (s - 0x269ec3u) * inv;
}
uint32_t step_fwd(uint32_t s) { return s * 0x343fdu + 0x269ec3u; }
int draws_between(uint32_t a, uint32_t b, int limit) {
    for (int n = 0; n <= limit; ++n) { if (a == b) return n; a = step_fwd(a); }
    return -1;
}
struct Sim {
    uint32_t state = 0;
    int count = 0;
    uint32_t operator()() { ++count; state = step_fwd(state); return (state >> 16) & 0x7fff; }
};
}

int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "usage: ring-probe golden.tsv disc [maxK] [police 0|1]\n"; return 2; }
    const auto g = rr::golden::load(argv[1]);
    const int max_k = argc > 3 ? std::stoi(argv[3]) : 12000;
    const bool police_at_setup = argc > 4 && std::stoi(argv[4]) != 0;
    rr::Assets assets(argv[2]);
    static const char* courses[] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};
    const auto& h = g.frames.at(0);
    const std::string name = courses[h.at("course") - 1];
    const int level = int(h.at("level"));
    const rr::Course course = rr::read_course(assets.read("DATA/COURSES/" + name + ".CRS"));
    // Green: first frame where the player moves; anchor = previous post-step rng.
    size_t green = 1;
    for (; green < g.frames.size(); ++green)
        if (g.frames[green].at("rider.0.position") != h.at("rider.0.position") ||
            g.frames[green].at("rider.0.speed") != h.at("rider.0.speed")) break;
    const uint32_t anchor = uint32_t(g.frames.at(green - 1).at("rng"));
    // The ring camera (0x004b6e5c); a walker trace pins it as back + 0x3000.
    const int32_t camera0 = h.count("ring.walk.back.position") ? int32_t(h.at("ring.walk.back.position")) + 0x3000
                                                               : int32_t(h.at("rider.0.position"));
    // Golden crossings: ticks where the player's sample changes (camera proxy).
    struct Crossing { size_t frame; int golden; };
    std::vector<Crossing> crossings;
    for (size_t i = green; i < g.frames.size() && crossings.size() < 400; ++i) {
        const int32_t a = int32_t(g.frames[i - 1].at("rider.0.position")), b = int32_t(g.frames[i].at("rider.0.position"));
        const int n = draws_between(uint32_t(g.frames[i - 1].at("rng")), uint32_t(g.frames[i].at("rng")), 4000);
        if ((a >> 8) != (b >> 8)) crossings.push_back({i, n});
    }
    std::printf("course %s level %d anchor %u green frame %zu camera %d crossings %zu\n",
                name.c_str(), level, anchor, green, camera0, crossings.size());
    const bool terrain_keys = h.count("ring.terrain.schema") != 0;
    int best = -1, best_k = -1;
    std::map<int, int> hist;
    for (int k = 1; k <= max_k; ++k) {
        uint32_t start = anchor;
        for (int i = 0; i < k; ++i) start = step_back(start);
        Sim sim{start, 0};
        int pool = 0;
        bool player = police_at_setup;
        auto sink = [&](const rr::RingActor& a, uint32_t) {
            if ((a.word0 & 15) < 0xb) { if (pool < 15) { ++pool; sim(); } }
            else if (player) { sim(); sim(); }
        };
        rr::TrackRing ring(course, level);
        std::function<uint32_t()> random = [&] { return sim(); };
        try { ring.setup(camera0, random, sink); } catch (const std::exception&) { continue; }
        const int setup_draws = sim.count;
        if (setup_draws > k) continue;
        if (terrain_keys) {
            // Walker terrain trace (ring.terrain.schema): the setup must leave
            // both walkers' hidden hill state exactly as frame 0 shows it.
            const auto state = ring.diagnostic_state();
            bool same = true;
            for (int w = 0; w < 2 && same; ++w) {
                const auto& walk = w ? state.back : state.front;
                const std::string prefix = std::string("ring.walk.") + (w ? "back" : "front") + ".terrain.";
                if (!walk.terrain) { same = !h.count(prefix + "position"); continue; }
                auto eq = [&](const std::string& key, int64_t v) { auto it = h.find(prefix + key); return it == h.end() || it->second == v; };
                same = eq("position", walk.terrain->position) && eq("last_sample", walk.terrain->last_sample) && eq("record", int64_t(walk.terrain->record));
                for (size_t i = 0; i < walk.terrain->points.size() && same; ++i) {
                    const auto& pt = walk.terrain->points[i];
                    const std::string pk = "point." + std::to_string(i) + ".";
                    same = eq(pk + "start", pt.start) && eq(pk + "end", pt.end) && eq(pk + "x", pt.x) &&
                           eq(pk + "height", pt.height) && eq(pk + "dx", pt.dx) && eq(pk + "dh", pt.dh);
                }
            }
            if (same) std::printf("K %d setup %d after %d: frame-0 walker terrain matches\n", k, setup_draws, k - setup_draws);
        }
        player = true;
        int matched = 0;
        int32_t camera = camera0;
        for (const auto& c : crossings) {
            const int32_t to = int32_t(g.frames[c.frame].at("rider.0.position"));
            sim.state = uint32_t(g.frames[c.frame - 1].at("rng"));
            sim.count = 0;
            // Advance the ring cursors to the previous position (no crossing), then cross.
            const int32_t from = int32_t(g.frames[c.frame - 1].at("rider.0.position"));
            if (from != camera) { ring.advance(from - camera, random, sink); camera = from; }
            sim.count = 0;
            sim.state = uint32_t(g.frames[c.frame - 1].at("rng"));
            ring.advance(to - camera, random, sink);
            camera = to;
            const int extra = c.golden - sim.count;
            if (c.golden >= 0 && extra >= 0 && extra <= 2) ++matched; else break;
        }
        ++hist[matched];
        if (matched > best) { best = matched; best_k = k; std::printf("K %d setup %d after %d matched %d\n", k, setup_draws, k - setup_draws, matched); }
    }
    std::printf("best K %d matched %d of %zu\n", best_k, best, crossings.size());
    return 0;
}
