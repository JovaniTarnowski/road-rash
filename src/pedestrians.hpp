#pragma once
#include "course_scene.hpp"
#include "race.hpp"
#include <array>
#include <functional>
#include <set>
#include <tuple>
#include <vector>

namespace rr {
// Pedestrians (plan 24 Task 4): the 15-object pool 0x004c1274 (0x00440640),
// template 0x0046dde0 (type 0x20; draw 0x00440680, think 0x00440dd0,
// move 0x00440990, contact 0x00440760), spawned from DZHR actor records by
// 0x004520d0 -> 0x004409e0 when the ring builds their slice. Race steps them
// (src/race_pedestrians.cpp); this module owns the pool and course hooks.
struct Pedestrian {
    bool active=false;
    Rider body;                        // object image (+0x1c position, +0x28 lateral, motion words) and route bookkeeping
    const PedestrianLook* look=nullptr;// 0x00440b80: null when category-2 resources 0/1 are missing (+0x1e0 stays 0)
    ActorAnimationState animation;     // block +0x70
    uint8_t selector=0;                // +0x1dc family selector
    int type=0;                        // +0x1e4: 4..7 cross (4 faces back, no pace), 10 performs, others stand
    int state=0;                       // +0x1e8: 0 wait, 1 cross, 2 knocked down, 3 hit/hesitate, 4/5 recover, 7 perform
    int side=1;                        // +0x1ec: 1 walks toward larger lateral, 3 toward smaller (index into 0x0046df30)
    int32_t heading=0;                 // +0x1f0
    int32_t pace=0;                    // +0x1f4: speed cap and acceleration are pace*100
    int32_t trigger=0;                 // +0x1f8: player position that releases the wait
    uint32_t order=0;                  // creation order (scan-list ties, Race::build_scan_list)
    int8_t lane=0;                     // +0x1fc: lane at the start of the crossing step
    int32_t voice_tick=0;              // +0x200: last scream tick (one per 0xb4 ticks)
    int32_t anchor=0;                  // +0x1c8: spawn position; 0x00440990 keeps within 0x1ff of it
    int32_t last_tick=0,next_tick=0;   // scheduler +0x10 / +0xc
    uint16_t serial=0;                 // +0x1c4 short, DAT_004c1278
    bool loaded() const { return look!=nullptr; }
    bool has(int code) const { return look && size_t(code)<look->bank.size() && look->bank[size_t(code)].has_value(); }
};

class PedestrianSystem {
public:
    static constexpr size_t pool_size=15;    // 0x00438b70(0x468, 0xf)
    using ActorSource=std::function<const std::vector<DzhrActor>&(const RouteSample&)>;
    using LookSource=std::function<const PedestrianLook*(const RouteSample&,uint8_t selector)>;
    // template is 0x0046dde0 (100 bytes) from the executable.
    PedestrianSystem(ActorSource actors,LookSource looks,std::vector<uint8_t> exe);
    void reset();                      // new race: empty pool, every DZHR record armed again
    std::vector<Pedestrian>& pool() { return pool_; }
    const std::vector<Pedestrian>& pool() const { return pool_; }
    const std::vector<DzhrActor>& actors(const RouteSample& s) const { return actors_(s); }
    const PedestrianLook* look(const RouteSample& s,uint8_t selector) const { return looks_(s,selector); }
    // 0x004520d0 marks a fired record with |= 0x3f00; the course data here is
    // shared and const, so the mark lives in this set (section, local, index).
    bool consume(uint32_t section,uint32_t local,size_t index);
    // Copies template 0x0046dde0 into a fresh object image (0x00438930 / 0x004387a0).
    void apply_template(Bike&) const;
    int32_t spawned() const { return spawned_; }
    uint16_t next_serial() { return serial_++; }
    // Ring slices whose DZHR records have been walked (0x00454240 -> 0x004520d0).
    bool filled=false;
    int32_t built_back=0,built_front=0;
    int32_t spawned_=0;
private:
    ActorSource actors_;
    LookSource looks_;
    std::array<int32_t,25> template_{};
    std::vector<Pedestrian> pool_;
    std::set<std::tuple<uint32_t,uint32_t,size_t>> consumed_;
    uint16_t serial_=0;
};

// Native draw node for one pedestrian through the actor billboard path
// (0x00440680 -> 0x0043af60 view selection -> 0x0044ba40 at size 0x200,
// ground (height >> 8) + ground). Shadows (+0xa8 second billboard) are not
// drawn yet.
std::optional<DrawNode> pedestrian_node(const Pedestrian&,const RoadSlice& near,const RoadSlice& far,int road,
    const RouteSample&,const Camera&,const DepthTable&,int32_t ceiling,int32_t logical_width=320);
}
