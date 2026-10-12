#pragma once
#include "animation.hpp"
#include "actor_descriptors.hpp"
#include "cel.hpp"
#include "course.hpp"
#include "race.hpp"
#include "resource.hpp"
#include "road.hpp"
#include "sound_event.hpp"
#include "world_renderer.hpp"
#include <array>
#include <functional>
#include <vector>

namespace rr {

// Vehicle library from a course's DATA/CARS/<name>.CAR container, following
// the original loader 0x004465f0: vehicle i pairs the i-th 'ANIM' (RRAN)
// animation with the i-th 'CANS' wrapper when present. The vehicle mask
// 0x004b8fe0[i] is the CCB flags word inside the CANS 'SNAC' wrapper, read
// through the 0x0040b5f0/0x0040b620 chain; vehicles without a CANS inherit the
// previous pair's wrapper, as measured against the original initializer.
struct Vehicle {
    Animation animation;
    uint32_t mask = 0;
    ActorGraphicsGroups views;
    ActorAnimationBank playback;
};
class VehicleLibrary {
public:
    // Throws std::runtime_error on malformed container entries.
    explicit VehicleLibrary(const std::vector<uint8_t>& cars_container,const std::vector<uint8_t>& exe={});
    const std::vector<Vehicle>& vehicles() const { return vehicles_; }
    // 0x0044df60 type choice: accepted vehicles are those whose mask selects
    // no speed class ((mask & 0x1c0000) == 0) or the class matching the kind
    // filter; the pick is rand() % accepted count. Returns -1 when none.
    int choose(uint8_t kind, uint32_t random) const;
private:
    std::vector<Vehicle> vehicles_;
};

// Spawn timers, ports of 0x0044dd00 / 0x0044dd90. The rate is 0x19*100
// (0x0044dd00(-25)); the reloads scale with the density argument.
struct TrafficTimers {
    int32_t rate = 2500;                              // 0x004b7e84 / 0x004b7ea4
    int32_t oncoming_reload = 0, other_reload = 0;    // 0x004b7e88 / 0x004b7e9c
    int32_t oncoming = 0, behind = 0, ahead = 0;      // 0x004b7e90 / 0x004b7e94 / 0x004b7e80
    int density = 6;                                  // 0x004b7ea0
};
// 0x0044db80's sequence: 0x0044dd00(-25) then 0x0044dd00(density), then the
// behind/ahead timer starts.
void traffic_configure(TrafficTimers&, int density);

// Lane count of a ring sub-slice lane word in 256ths, with the original's
// signed-char semantics: (int8_t)((value + (value < 0 ? 0xff : 0)) >> 8).
int traffic_lane_count(int32_t lanes_q8);

// One car object. Field comments name the original object offsets
// (0x004387a0 template copies and 0x0044df60 writes).
struct TrafficCar {
    uint8_t kind = 0;          // +0x73
    uint8_t vehicle = 0;       // +0x7f
    int8_t lane = 0;           // +0x7a
    int32_t position = 0;      // +0x1c: Q8 samples along the route
    int32_t lateral = 0;       // +0x28: Q8 world lateral
    int32_t speed = 0;         // +0xec
    int32_t phase = 0;           // +0x71: (rand & 0x7f) + 0xc0, 192..319
    uint8_t jitter_a = 0;      // +0x72
    uint8_t jitter_b = 0;      // +0x1c9
    uint8_t jitter_c = 0;      // +0x1ca
    uint32_t serial = 0;       // +0x82 (DAT_004b7e98)
    bool active = false;
};

class TrafficSystem {
public:
    static constexpr size_t pool_size = 16;   // 0x00438b70(0x210, 0x10): 16 objects
    TrafficSystem(const VehicleLibrary&, std::vector<uint8_t> exe);
    void configure(int density) { traffic_configure(timers_, density); }
    // One simulation tick on the player's route. camera/front are the ring
    // cursors 0x004b6e5c (camera) and 0x004b6e50 (front, camera + 0x5000);
    // distance_moved is the per-step camera advance (0x0044b3b0). player_q8
    // and player_lateral feed the horn rule (0x0044efa0). random is the
    // shared CRT rand stream; the original call order is preserved.
    void step(const std::vector<RouteSample>& route, int32_t camera_q8, int32_t front_q8,
              int32_t distance_moved, int32_t player_q8, int32_t player_lateral, const std::function<uint32_t()>& random);
    const std::vector<TrafficCar>& cars() const { return cars_; }
    // 0x004368f0 car pass: the contact response writes the car's share of
    // the separation (lateral push; the original also runs 0x00436a40 on the
    // car object, which has no scratch equivalent — see analysis/car-notes.md).
    std::vector<TrafficCar>& cars_mut() { return cars_; }
    const TrafficTimers& timers() const { return timers_; }
    const VehicleLibrary& library() const { return library_; }
    std::vector<SoundEventRecord> take_sound_events() { auto e = std::move(events_); events_.clear(); return e; }
    // Sprite size class from the car template (0x00475698 +0x28).
    uint8_t sprite_size() const { return sprite_size_; }
    int32_t drift_rate(uint8_t kind) const { return drift_rates_[kind & 3]; }
    // Plan 10: append "car.<pool index>.<field>" rows (tests/sim_fields.hpp
    // car_fields) for every pool slot, active or not.
    void append_snapshot(std::vector<FieldValue>& out) const;
private:
    bool place(uint8_t kind, int8_t lane, const RouteSample& at, int32_t spawn_q8, const std::function<uint32_t()>& random);
    void move(TrafficCar&, const std::vector<RouteSample>& route, int32_t front_q8, int32_t back_q8,
              int32_t player_q8, int32_t player_lateral, const std::function<uint32_t()>& random);
    const VehicleLibrary& library_;
    TrafficTimers timers_;
    std::array<int32_t, 4> drift_rates_{};      // 0x00475694[kind] low byte * 800
    std::array<int32_t, 4> lookahead_{};        // 0x00468f00[kind], Q8 samples
    std::vector<TrafficCar> cars_{pool_size};
    std::vector<SoundEventRecord> events_;
    uint32_t serial_ = 0;                       // DAT_004b7e98
    uint8_t sprite_size_ = 10;
};

// 0x004467e0 / 0x0044e710 selects one of three one-frame descriptor
// sequences by distance; 0x00446830 emits the actor billboard.
std::optional<DrawNode> traffic_car_node(const Vehicle&,const TrafficCar&,
    const RoadSlice&,const RoadSlice&,int road,const RouteSample&,
    const Camera&,const DepthTable&,int32_t camera_q8,int32_t ceiling);

// Display nodes for the active cars, ordered far to near. The projection
// follows the verified scenery path (scenery_node) with the depth-table scale
// at each car's Q8 depth relative to the camera; frame cycling is a native
// approximation of the RRAN advancement 0x0043ae10/0x0043ae90, which is not
// ported.
std::vector<DrawNode> traffic_draw_nodes(const TrafficSystem&,
    const std::vector<RouteSample>& route, const std::vector<RoadSlice>& slices,
    const Camera&, const DepthTable&, int32_t camera_q8, uint32_t tick);

}