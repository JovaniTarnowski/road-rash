// TrackRing (src/track_ring.hpp): synthetic single-section course with a
// wall profile, one random-spacing BORR record and one DZHR actor. Checks the
// original draw counts of the ring setup (0x004552e0) and of slice crossings
// (0x00454120), and the actor the slice build fires.
#include "track_ring.hpp"
#include <iostream>

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL " << what << '\n';
        ++failures;
    }
}

rr::Course synthetic_course() {
    rr::Course c;
    c.root = 20;
    rr::CourseSection s;
    s.samples.resize(300, rr::PathSample{0, 0, 500, 500, 512, 512});
    auto& t = s.terrain;
    t.kind = rr::TerrainDefinition::Kind::wall;
    for (int side = 0; side < 2; ++side) {
        rr::TerrainWallRecord r;
        r.parameters = {0, 1, 4, 1, 4, 0, 1, 4}; // variations +1/+6/+3 = 1, bounds +2/+7/+4 = 4
        t.walls[side].push_back(r);
    }
    t.selectors.resize(300, {0, 0});
    t.end_selectors = std::array<uint8_t, 2>{7, 8};
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

void backward_iterators() {
    auto course = synthetic_course();
    auto& section = course.sections[0];
    section.roadside_profiles.push_back(0x0f020000u); // second selector, no random slots
    // Keep a later record so the backwards DZHR comparison is inside the table.
    section.scenery_events = {{(4u << 24) | (15u << 20) | (1u << 8), 0},
                              {(6u << 24) | (15u << 20) | (2u << 8), 0},
                              {(30u << 24) | (15u << 20) | (3u << 8), 0}};
    rr::TrackRing ring(course, 0);
    rr::TrackRing::Random random = [] { return 0u; };
    ring.setup(0x3200, random, {});
    for (int i = 0; i < 15; ++i) ring.advance(0x100, random, {});
    check(ring.diagnostic_state().back.borr_record == 1, "BORR moves to second record");
    int billboards = 0, selector = -1;
    ring.set_static_sink([&](int32_t sample, uint32_t, uint32_t, const rr::RingStatics& s) {
        if (sample == 10) billboards = s.billboards;
        selector = s.borr_selector;
    });
    ring.advance(-0x100, random, {});
    ring.advance(-0x100, random, {});
    auto back = ring.diagnostic_state().back;
    check(back.borr_record == 0 && back.borr_start == 0 && back.borr_end == 15,
          "BORR rewinds at start, restoring the previous interval");
    check(selector == 1, "back slice uses rewound BORR selector");
    for (int i = 0; i < 5; ++i) ring.advance(-0x100, random, {});
    check(billboards == 1 && ring.diagnostic_state().back.dzhr_index == 1,
          "DZHR rewinds and rebuilds the billboard at sample ten");

    rr::TrackRing start(course, 0);
    start.setup(0x3200, random, {});
    for (int i = 0; i < 3; ++i) start.advance(-0x100, random, {});
    check(!start.unsupported() && start.diagnostic_state().back.position == 0,
          "missing previous node leaves the walker at the course start");
    start.advance(0x100, random, {});
    check(start.diagnostic_state().back.position == 256, "walker can move away from the course start");

    for (uint32_t following : {0u, 0x52534543u}) {
        auto tail = synthetic_course();
        tail.sections[0].scenery_following_word = following;
        rr::TrackRing exhausted(tail, 0);
        exhausted.setup(0x3200, random, {});
        for (int i = 0; i < 20; ++i) exhausted.advance(0x100, random, {});
        check(exhausted.diagnostic_state().back.dzhr_index == 1, "DZHR exhausted forward index");
        exhausted.advance(-0x100, random, {});
        check(exhausted.diagnostic_state().back.dzhr_index == (following ? 0 : 1),
              "DZHR rewind tests the physical following word before decrementing");
    }

    // Put the front in road B and walk it back to A. The back stays in A,
    // so no slice is built from the front's boundary position.
    auto roads = synthetic_course();
    roads.sections.push_back(roads.sections[0]);
    roads.nodes.at(20).next = 60;
    roads.nodes.emplace(60u, rr::CourseNode{60, 0, 40, 20, 1, 300});
    rr::TrackRing traversal(roads, 0);
    traversal.setup(0x3200, random, {});
    for (int i = 0; i < 170; ++i) traversal.advance(0x100, random, {});
    check(traversal.diagnostic_state().front.node == 60, "front enters road B");
    traversal.advance(-0x100, random, {});
    const auto front = traversal.diagnostic_state().front;
    check(!traversal.unsupported() && front.node == 20 && front.position == 300 * 256,
          "backward node entry uses previous road end minus old position");
    check(front.terrain && front.terrain->position == 300 * 256 && front.borr_position == 299 * 256 &&
              front.dzhr_position == 299 * 256,
          "previous-road terrain, BORR and DZHR use their original distinct origins");
    // Move the back walker across the same boundary. Its rebuilt end slice
    // must use the backwards initializers, including the physical selector tail.
    rr::TrackRing back_traversal(roads, 0);
    back_traversal.setup(0x3200, random, {});
    for (int i = 0; i < 298; ++i) back_traversal.advance(0x100, random, {});
    int endpoint_slices = 0;
    back_traversal.set_slice_sink(
        [&](uint32_t section, uint32_t local, const rr::PathSample& path, const rr::TerrainSlice& slice) {
            if (section == 0 && local == 300) {
                ++endpoint_slices;
                check(path.left == 500 && slice.selectors == std::array<uint8_t, 2>{7, 8},
                      "reverse boundary slice uses the original endpoint values");
            }
        });
    back_traversal.advance(-0x100, random, {});
    check(!back_traversal.unsupported() && endpoint_slices == 1,
          "back walker rebuilds the previous road endpoint");
}

void window_walks() {
    auto run = [](uint32_t branch_length, int& draws) {
        const rr::Course course = window_course(branch_length);
        rr::TrackRing ring(course, 0);
        uint32_t state = 777;
        draws = 0;
        std::function<uint32_t()> random = [&] {
            ++draws;
            state = state * 0x343fdu + 0x269ec3u;
            return (state >> 16) & 0x7fffu;
        };
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
} // namespace

int main() {
    window_walks();
    backward_iterators();
    const rr::Course course = synthetic_course();
    rr::TrackRing ring(course, 0);
    uint32_t state = 12345;
    int draws = 0;
    std::function<uint32_t()> random = [&] {
        ++draws;
        state = state * 0x343fdu + 0x269ec3u;
        return (state >> 16) & 0x7fffu;
    };
    int slices = 0, wall_slices = 0;
    ring.set_slice_sink(
        [&](uint32_t section, uint32_t local, const rr::PathSample&, const rr::TerrainSlice& t) {
            ++slices;
            if (section == 0 && local >= 2 && local < 130 && t.type == 1) ++wall_slices;
        });
    std::vector<rr::RingActor> actors;
    std::function<void(const rr::RingActor&, uint32_t)> sink = [&](const rr::RingActor& a, uint32_t) {
        actors.push_back(a);
    };
    ring.setup(0x3200, random, sink);
    // DZHR jitter 2 + two walks to sample 2 (2 x 2 wall steps x 6) + BORR
    // slices 2..15 (14 x two passes x two objects) + actor jitter 1 + 128
    // front wall steps x 6.
    check(draws == 2 + 24 + 56 + 1 + 768, "setup draw count");
    check(actors.size() == 1 && actors[0].ring_sample == 20 && (actors[0].word0 & 15) == 4,
          "actor fired once at sample 20");
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
    check(!ring.unsupported() && draws == 16, "backward crossing: wall steps and two BORR passes");
    if (failures) return 1;
    std::cout << "track ring ok\n";
    return 0;
}
