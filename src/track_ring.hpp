#pragma once
#include "course.hpp"
#include "terrain.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace rr {
// The random-consuming half of the original track ring: the two course
// walkers 0x0049ee30 (front) and 0x0049ea20 (back) that 0x004552e0 builds
// and 0x00454120 steps one slice at a time. Each walker carries the section
// feature iterators that draw from the shared CRT stream:
//   terrain  0x004510c0/0x004516e0 (LIHR init 0x00450fc0, steps 0x00451380;
//            NTMR/DWDR steps 0x00451220), via TerrainIterator;
//   BORR     0x00451b60/0x00451e20, slot walk 0x00451cb0 (run twice per
//            slice build by 0x00454240, drawing when spacing is random);
//   DZHR     0x00452060/0x00452230, actor spawns 0x004520d0 -> 0x004409e0.
// 0x00453e60 jitters and sorts every section's DZHR records (0x00451ee0,
// 0x00451fc0) in course-graph order before the walkers start.
// Only single-road sections are walked; a split or merge node stops the
// walker (unsupported() reports it) until dual walkers are ported.
// One DZHR actor record fired by a slice build: ring_sample is the slice's
// route sample (ring position >> 8), position_q8 the record's position.
struct RingActor { int32_t ring_sample=0; uint32_t position_q8=0, word0=0, word1=0, section=0, local=0; };
// Value-only walker snapshots for optional original terrain diagnostics.
struct TrackWalkerState {
    int32_t position=0;
    std::optional<TerrainIteratorState> terrain;
};
struct TrackRingState {
    TrackWalkerState front,back;
};
class TrackRing {
public:
    using Random = std::function<uint32_t()>;
    // Called for each DZHR actor record 0x004520d0 fires, with its lateral
    // jitter draw (lateral = (centre + signed 12 bits) * 256 - 2s + jitter %
    // (4s + 1), s = (word1 & 0xff0000) >> 14). The sink makes the 0x004409e0
    // draws (pool allocation, police).
    using ActorSink = std::function<void(const RingActor&, uint32_t jitter)>;
    TrackRing(const Course&, int level);
    TrackRing(const TrackRing&) = delete;
    TrackRing& operator=(const TrackRing&) = delete;
    // 0x004552e0 (random part): DZHR jitter, walker init, walk to camera -
    // 0x3000, then 128 slice builds from the front walker.
    void setup(int32_t camera_q8, const Random&, const ActorSink&);
    // 0x00454120 with one slice crossing forward: back walker step, slice
    // build at the old front, front walker step. Smaller moves only advance
    // the cursors.
    void advance(int32_t delta_q8, const Random&, const ActorSink&);
    int32_t front_q8() const { return front_; }
    int32_t back_q8() const { return back_; }
    bool unsupported() const { return unsupported_; }
    TrackRingState diagnostic_state() const;
    // Terrain slice exported by a walker at its current sample.
    TerrainSlice front_terrain(int32_t left_edge, int32_t right_edge) const;
private:
    struct Borr {
        const std::vector<uint32_t>* words = nullptr;
        int32_t position = 0;            // [2] Q8
        int32_t record = 0;              // [3]
        int32_t start = 0, end = 0;      // [9], [10] samples
        int32_t current = 0;             // [11] sample
        uint8_t alternate = 0, left = 0, selector = 0x3f, random = 0, count = 0, units = 0, period = 0;
        int32_t spacing = 0;             // [8]
        void load(int32_t index);        // 0x00451a90
        void init(const std::vector<uint32_t>*);   // 0x00451b60 forward
        void step(int32_t delta);        // 0x00451e20
        void slots(const PathSample&, const Random&) const;   // 0x00451cb0 (draws only)
    };
    struct Dzhr {
        size_t node = 0;                 // index into events_ (per course node)
        int32_t position = 0;            // [1] Q8
        int32_t index = 0;               // [2]
    };
    struct Walker {
        uint32_t node = 0;
        int32_t position = 0;            // Q8 within the node's section
        std::optional<TerrainIterator> terrain;
        Borr borr;
        Dzhr dzhr;
        bool stopped = false;
    };
    struct Events { uint32_t section = 0; std::vector<std::array<uint32_t,2>> records; };
    // Terrain part of one section walker inside a 0x00453b60 window probe.
    struct ProbeHalf {
        uint32_t node = 0;
        int32_t position = 0;            // Q8 within the node's section
        std::optional<TerrainIterator> terrain;
    };
    void probe_enter(ProbeHalf&, uint32_t node, bool start);    // 0x00452cb0 terrain init
    int probe_step(ProbeHalf&, int32_t delta);                  // 0x00452e10 (0/1/2/3)
    void probe_window(uint32_t node, int32_t direction);        // 0x00453b60 draws
    void jitter_events(uint32_t node, std::vector<uint32_t>& visited, const Random&);   // 0x00453e60
    void enter(Walker&, uint32_t node);                         // 0x00452cb0 forward
    void step(Walker&, int32_t delta);                          // 0x00453030 -> 0x00452e10
    void build_slice(Walker&, int32_t ring_q8, const ActorSink&);   // 0x00454c80 -> 0x00454240
    const Course& course_;
    int level_;
    std::vector<Events> events_;                                // one per node, DFS order
    std::vector<std::pair<uint32_t,size_t>> event_index_;       // node offset -> events_
    const Random* random_ = nullptr;                            // stream for the current call
    Random forward_;                                            // TerrainIterator's source
    Walker front_walker_, back_walker_;
    int32_t front_ = 0, back_ = 0;
    bool unsupported_ = false;
};
}
