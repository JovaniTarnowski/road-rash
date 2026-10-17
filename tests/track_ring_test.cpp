// TrackRing (src/track_ring.hpp): synthetic single-section course with a
// wall profile, one random-spacing BORR record and one DZHR actor. Checks the
// original draw counts of the ring setup (0x004552e0) and of slice crossings
// (0x00454120), and the actor the slice build fires.
#include "track_ring.hpp"
#include <iostream>

namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { std::cerr << "FAIL " << what << '\n'; ++failures; } }

rr::Course synthetic_course() {
    rr::Course c;
    c.root = 20;
    rr::CourseSection s;
    s.samples.resize(300, rr::PathSample{0, 0, 500, 500, 512, 512});
    auto& t = s.terrain;
    t.kind = rr::TerrainDefinition::Kind::wall;
    for (int side = 0; side < 2; ++side) {
        rr::TerrainWallRecord r;
        r.parameters = {0, 1, 4, 1, 4, 0, 1, 4};   // variations +1/+6/+3 = 1, bounds +2/+7/+4 = 4
        t.walls[side].push_back(r);
    }
    t.selectors.resize(300, {0, 0});
    // units 15 (bits 24..27), period 0, random spacing (bit 11), two objects.
    s.roadside_profiles.push_back(0x0f000000u | 0x00010000u | 0x800u | 0x100u);
    // Actor (bit 31) at sample 20, no jitter range, probability 15 (always
    // kept), selector 5, pedestrian type 4.
    s.scenery_events.push_back({0x80000000u | (20u << 24) | (15u << 20) | (5u << 8) | 4u, 0});
    c.sections.push_back(s);
    c.nodes.emplace(20u, rr::CourseNode{20, 0, 40, 0, 0, 300});
    c.nodes.emplace(40u, rr::CourseNode{40, 4});
    return c;
}

// Road A -> split 40 -> branches 60 / 80 (wall terrain) -> merge 100 ->
// road B. Only the branches have terrain, so every setup draw comes from
// the 0x00453b60 window walks the DZHR pass (0x00453e60) runs at the split
// and at the merge.
rr::Course window_course(uint32_t branch_length) {
    rr::Course c;
    c.root = 20;
    rr::CourseSection plain;
    plain.samples.resize(300, rr::PathSample{0, 0, 500, 500, 512, 512});
    rr::CourseSection walled = plain;
    walled.samples.resize(branch_length, rr::PathSample{0, 0, 500, 500, 512, 512});
    walled.terrain.kind = rr::TerrainDefinition::Kind::wall;
    for (int side = 0; side < 2; ++side) {
        rr::TerrainWallRecord r;
        r.parameters = {0, 1, 4, 1, 4, 0, 1, 4};
        walled.terrain.walls[side].push_back(r);
    }
    walled.terrain.selectors.resize(branch_length, {0, 0});
    c.sections = {plain, walled, walled, plain};
    c.nodes.emplace(20u, rr::CourseNode{20, 0, 40, 0, 0, 300});
    rr::CourseNode split{40, 1, 60, 80, 20, 0};
    split.primary = 1;
    c.nodes.emplace(40u, split);
    c.nodes.emplace(60u, rr::CourseNode{60, 0, 100, 40, 1, branch_length});
    c.nodes.emplace(80u, rr::CourseNode{80, 0, 100, 40, 2, branch_length});
    c.nodes.emplace(100u, rr::CourseNode{100, 2, 60, 80, 120, 0});
    c.nodes.emplace(120u, rr::CourseNode{120, 0, 140, 100, 3, 300});
    c.nodes.emplace(140u, rr::CourseNode{140, 4});
    return c;
}

void window_walks() {
    auto run = [](uint32_t branch_length, int& draws) {
        const rr::Course course = window_course(branch_length);
        rr::TrackRing ring(course, 0);
        uint32_t state = 777;
        draws = 0;
        std::function<uint32_t()> random = [&] { ++draws; state = state * 0x343fdu + 0x269ec3u; return (state >> 16) & 0x7fffu; };
        std::function<void(const rr::RingActor&, uint32_t)> sink = [](const rr::RingActor&, uint32_t) {};
        ring.setup(0x3200, random, sink);
        return ring.unsupported();
    };
    int draws = 0;
    // Split: the first step enters both branches at 0 and steps them by 0
    // (no sample change); 80 more steps move both halves one sample (2 x 6
    // wall draws). Merge: both inputs entered at their ends, then 81
    // backward steps of both halves.
    check(!run(200, draws), "window walks supported");
    check(draws == 80 * 12 + 81 * 12, "split and merge window walk draws");
    // Branches shorter than the 0x51-slice window reach the other node.
    check(run(60, draws), "window walk through a short branch reported unsupported");
}
}

int main() {
    window_walks();
    const rr::Course course = synthetic_course();
    rr::TrackRing ring(course, 0);
    uint32_t state = 12345;
    int draws = 0;
    std::function<uint32_t()> random = [&] { ++draws; state = state * 0x343fdu + 0x269ec3u; return (state >> 16) & 0x7fffu; };
    int slices = 0, wall_slices = 0;
    ring.set_slice_sink([&](uint32_t section, uint32_t local, const rr::PathSample&, const rr::TerrainSlice& t) {
        ++slices;
        if (section == 0 && local >= 2 && local < 130 && t.type == 1) ++wall_slices;
    });
    std::vector<rr::RingActor> actors;
    std::function<void(const rr::RingActor&, uint32_t)> sink = [&](const rr::RingActor& a, uint32_t) { actors.push_back(a); };
    ring.setup(0x3200, random, sink);
    // DZHR jitter 2 + two walks to sample 2 (2 x 2 wall steps x 6) + BORR
    // slices 2..15 (14 x two passes x two objects) + actor jitter 1 + 128
    // front wall steps x 6.
    check(draws == 2 + 24 + 56 + 1 + 768, "setup draw count");
    check(actors.size() == 1 && actors[0].ring_sample == 20 && (actors[0].word0 & 15) == 4, "actor fired once at sample 20");
    check(ring.back_q8() == 0x200 && ring.front_q8() == 0x8200, "ring cursors");
    check(!ring.unsupported(), "single road supported");
    check(slices == 128 && wall_slices == 128, "every slice build exports its wall terrain slice");
    draws = 0;
    ring.advance(0x80, random, sink);
    check(draws == 0, "no crossing, no draws");
    ring.advance(0x80, random, sink);
    check(draws == 12, "one crossing: back and front wall steps");
    draws = 0;
    ring.advance(-0x100, random, sink);
    check(ring.unsupported() && draws == 0, "backward crossing reported unsupported");
    if (failures) return 1;
    std::cout << "track ring ok\n";
    return 0;
}
