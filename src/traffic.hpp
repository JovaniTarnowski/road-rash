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
    int32_t cruise = 25;                              // 0x004b7e8c: car cruise scale (0x0044e710)
    int32_t rate = 2500;                              // 0x004b7e84 / 0x004b7ea4
    int32_t oncoming_reload = 0, other_reload = 0;    // 0x004b7e88 / 0x004b7e9c
    int32_t oncoming = 0, behind = 0, ahead = 0;      // 0x004b7e90 / 0x004b7e94 / 0x004b7e80
    int density = 6;                                  // 0x004b7ea0
};
// 0x0044dd00. Negative: rate -d*100 and cruise -d, reloads untouched.
// Otherwise density (0 means 6) and both reloads from the current rate; the
// oncoming timer restarts at its reload, behind/ahead keep running.
void traffic_set_density(TrafficTimers&, int density);
// 0x0044db80's sequence: 0x0044dd00(-25) then 0x0044dd00(density), then the
// behind/ahead timer starts. A negative density only sets the rate.
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
    int32_t lateral_vel = 0;   // crossing cars (kind bit 0): +-0x78000/tick
    int32_t speed = 0;         // +0xec
    int32_t phase = 0;           // +0x71: (rand & 0x7f) + 0xc0, 192..319
    uint8_t jitter_a = 0;      // +0x72
    uint8_t jitter_b = 0;      // +0x1c9
    uint8_t jitter_c = 0;      // +0x1ca
    uint32_t serial = 0;       // +0x82 (DAT_004b7e98)
    bool active = false;
};
// Intersection controller (0x0041eca0 object, pool 0x00468ee4, 10 x 0x114,
// type 0x400, think 0x0041f040) and signal (pool 0x00468ee0, 40 x 0x108,
// type 0x40, draw 0x0041f220). Field comments name the original offsets.
// Durations live on the controller (+0xfc/+0x100/+0x104); phase state,
// accumulator and spawn index live per signal (+0xf0/+0xf4/+0xf8).
struct IntersectionSignal {
    bool active = false;
    int32_t phase = 0;            // +0xf0: 0..2, init (~index & 1) * 2
    int32_t accum = 0x30c;        // +0xf4: phase accumulator seed
    int index = 0;                // +0xf8: signal index; odd indices spawn
    int lane = 0;                 // +0xfc [0x3f]: lane byte (cVar2/cVar3)
    int32_t lateral_q8 = 0x50;    // ef60 param_3: 0x50 or 0xb0
    int32_t position_q8 = 0;      // +0x1c: signal sample position
    int controller = -1;
};
struct IntersectionController {
    bool active = false;
    int32_t durations[3] = {600, 180, 780};   // +0xfc..: {600,0xb4,0x30c},
    int32_t base_q8 = 0;          // +0x108: event word0 * 0x100 + sample delta
    int sample = 0;               // ring sample of the zone end
    int kind_mode = 0;            // +0x110: 1 -> kind 1 narrow, 2 -> kind 3
    int lane_range = 1;           // +0x111: spawn lane range (rand % range)
    int signals[4] = {-1, -1, -1, -1};   // +0xec/+0xf0/+0xf4/+0xf8 slots
};
class TrafficSystem {
public:
    static constexpr size_t pool_size = 16;   // 0x00438b70(0x210, 0x10): 16 objects
    static constexpr size_t controller_pool = 10;   // 0x00468ee4: 10 x 0x114
    static constexpr size_t signal_pool = 40;       // 0x00468ee0: 40 x 0x108
    TrafficSystem(const VehicleLibrary&, std::vector<uint8_t> exe);
    void configure(int density) { traffic_configure(timers_, density); }
    // One simulation tick on the player's route. camera/front are the ring
    // cursors 0x004b6e5c (camera) and 0x004b6e50 (front, ring camera +
    // 0x5000). The ring camera starts at 0x3200 (0x0044b250) against the
    // view camera's 0x3100 and both advance by the same delta (0x0044b3b0),
    // so front = view camera + 0x5100 (static trace, not capture-checked).
    // Slices entering the ring front apply their zone events (see
    // PathSample::zone_end) before the spawn timers run, as 0x00454120
    // precedes 0x0044dd90; the first step also replays the initial ring
    // fill (0x004552e0, back to front over 128 samples).
    // 0x0041eca0 crossing traffic at intersections: controllers are placed
    // from type-1..3 zone ends in build_slice; think_intersections advances
    // phases and spawns crossing cars (kinds 1/3) before the car move pass,
    // as the 0x00438170 scheduler runs after 0x0044dd90. Signal heads are
    // never drawn: DAT_00468ee8/ef0 have no writer anywhere in the
    // executable (REA xrefs: only the 0x0041f220 reads), so 0x0041f220's
    // draws are skipped and the port draws nothing, matching the original.
    // distance_moved is the per-step camera advance (0x0044b3b0). player_q8
    // and player_lateral feed the horn rule (0x0044efa0). tick is the global
    // tick (DAT_004b7ecc bit 3 gates crossing spawns). random is the
    // shared CRT rand stream; the original call order is preserved.
    void step(const std::vector<RouteSample>& route, int32_t camera_q8, int32_t front_q8,
              int32_t distance_moved, int32_t player_q8, int32_t player_lateral, const std::function<uint32_t()>& random,
              uint32_t tick);
    const std::vector<TrafficCar>& cars() const { return cars_; }
    // 0x004368f0 car pass: the contact response writes the car's share of
    // the separation (lateral push; the original also runs 0x00436a40 on the
    // car object, which has no scratch equivalent — see analysis/car-notes.md).
    std::vector<TrafficCar>& cars_mut() { return cars_; }
    const TrafficTimers& timers() const { return timers_; }
    int32_t zone_events() const { return zone_events_; }   // density changes applied so far
    const std::vector<IntersectionController>& controllers() const { return controllers_; }
    const std::vector<IntersectionSignal>& signals() const { return signals_; }
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
    bool filled_ = false;                       // initial ring fill replayed
    int32_t built_end_ = 0;                     // ring front sample (exclusive) already built
    int32_t zone_events_ = 0;
    void build_slice(const std::vector<RouteSample>& route, int32_t sample);
    // 0x0041eca0 placement from a type-1..3 zone end: one controller and up
    // to four signals (variant = event word 2 + 1 selects the 3rd/4th).
    // Short phases {75,0,240} need the cross-node type-2 + flag data the
    // port does not import yet; the long table is used (capture check open).
    void place_intersection(int32_t sample, int variant, int lanes);
    // 0x0041f040 think over controllers in creation order, signals 0..3.
    void think_intersections(int32_t back_q8, int32_t front_q8, uint32_t tick,
                             const std::vector<RouteSample>& route, const std::function<uint32_t()>& random);
    bool place_crossing(IntersectionController& c, int lane, const std::vector<RouteSample>& route,
                        const std::function<uint32_t()>& random);
    std::vector<IntersectionController> controllers_{controller_pool};
    std::vector<IntersectionSignal> signals_{signal_pool};
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