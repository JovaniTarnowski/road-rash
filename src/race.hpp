#pragma once
#include "bike.hpp"
#include "crash.hpp"
#include "actor_descriptors.hpp"
#include "road.hpp"
#include "sound_event.hpp"
#include <array>
#include <vector>
#include <functional>
#include <optional>
#include <string>

namespace rr {
class TrafficSystem;
struct TrafficCar;
// Original timing: 0x004163d0 steps the simulation at QueryPerformanceFrequency/0x3c,
// at most 0x14 steps per frame.
constexpr int ticks_per_second = 60;
constexpr int max_steps_per_frame = 20;
// Nitro configuration: six words the setup copies from the player's bike
// spec at +0x14c (0x0040b0e0: recharge, boost Q8 multiplier, max charge,
// charges, minimum activation, tail-rate seed). The EXE defaults at
// 0x00465a28 are {0x11, 0x180, 0x78, 0x64, 0x1e, 0xf}.
struct NitroConfig { int32_t recharge = 0x11, boost = 0x180, max = 0x78, charges = 0x64, minimum = 0x1e, tail = 0xf; };

// Rider fight stats: rider table 0x00463c78 words 12..15 (+0x30 strength, +0x34 base stamina,
// +0x38 maximum, +0x3c current). Hits lower maximum by damage*4 (not below base/2) and current
// by damage*64; current regenerates one point per tick up to maximum (0x00407d40).
struct RiderStats { int32_t strength = 7, base = 2500, max = 2500, current = 2500; };

// Weapons (rider +0x1d4): 0 fists, 1 club, 2 chain, 3 kick. Reach in units of 0x1000 Q8
// (0x00409c10) and damage multipliers (0x00465a00).
constexpr int32_t weapon_reach[4] = {5, 7, 8, 5};
constexpr int32_t weapon_damage[4] = {256, 320, 384, 64};

struct Attack {
    int timer = 0;      // 0 idle; counts ticks while attacking
    int weapon = 0;
    bool right = false; // toward larger lateral (0x004095a0)
    bool backhand = false;
    bool fired = false; // descriptor frame-2 hit event consumed this swing (0x004096f0)
};
constexpr int attack_legacy_hit_tick = 7;   // pre-descriptor stand-in, used only with no animation bank
constexpr int attack_hit_tick = attack_legacy_hit_tick;  // compat alias (game_main attack frames)
constexpr int attack_ticks = 18;             // swing length cap without descriptor completion

// An AI opponent (bike + rider pair built by 0x00405990 with the AI tables).
struct Opponent {
    BikeSpec spec;
    int32_t rider_value;   // rider table +0xc / 2
    int base_speed;        // personality +0x1bc + level*4 (units of (v>>12)+(v>>7)+(v>>9))
    int cur2_speed;        // personality +0x1a8 + level*4
    int lane;              // 0x00405990 draws rand()%3 (stream-dependent); -1 = draw at build
    int slot;              // grid slot 0..7 (0x00464218)
    int rider = 0;         // rider index: 0x00463c78 row and 0x00464558 personality (0x00405990 param_6)
    int32_t grid_pos = 0;  // table sub-slice selector: exact Q8 start position (0 = native two-row grid)
    int32_t grid_lat = 0;  // table lateral (added to the road centre); see grid_pos
    bool frozen = false;   // solo oracle: think/move nulled, body present for scans
    int32_t throttle_percent = 100;   // 0x004642a0[level]
    RiderStats stats{};
    int weapon = 0;                   // rider table word 6 + level
};

// The original opponents for a level: personalities 1..count (0x00464558, 0x204 bytes each),
// bikes from 0x004641c8[level + rider*5], rider values from 0x00463c78 (+0xc / 2), AI throttle
// scale 0x004642a0[level]; grid slots 1..count.
std::vector<Opponent> original_opponents(const std::vector<uint8_t>& exe, const std::vector<BikeSpec>& specs,
                                         int level, int count);
// Original rider personalities (0x00464558, 10 x 0x204 bytes). The AI brain is
// a copy of one record (0x00407990 selects 0x004c6b40 + rider*0x81 words).
struct Personality {
    int32_t cur2_levels[5] = {};    // +0x1a8: reset source for +0x1a0
    int32_t cur_levels[5] = {};     // +0x1bc: reset source for +0x1a4
    int32_t rating[30] = {};        // +0x0: overtake rating currents (10 x 3)
    int32_t rating_tgt[30] = {};    // +0x78: rating targets
    int32_t ease[3] = {};           // +0x12c/+0x130/+0x134: replan easing steps
    int overtake_index = 0;         // +0x165: rating-table index used by 0x004092e0
    int32_t reach = 0;              // +0x1d4: overtake distance scale (0x004092e0)
};
std::vector<Personality> read_personalities(const std::vector<uint8_t>& exe);

struct AiState {
    int32_t target_x = 0;        // +0x194 (world units * 256)
    int32_t target_position = 0; // +0x198 (Q8 samples)
    int32_t target_speed = 0;    // +0x19c
    int32_t current_speed = 0;   // +0x1a4
    int32_t current2 = 0;        // +0x1a0
    int32_t base_speed = 0;      // +0x1bc + level*4
    int32_t replan_ticks = 0;    // +0x188
    int lane = 0;                // +0x18c
    int32_t bias = 0;            // +0x190
    int state = 1;               // +0x160: dispatch via 0x00465980 (1 = racing)
    int target_rider = -1;       // +0x168: riders_ index, -1 null
    int target_step = 0;         // +0x174: rated overtake step of the target
    int nbr_ahead = -1;          // +0x16c: replan scan pick, -1 null
    int nbr_behind = -1;         // +0x170: replan scan pick, -1 null
    int ov_ahead = 0;            // +0x178: overtake-step rating
    int ov_behind = 0;           // +0x17c: overtake-step rating
    int32_t rating[30] = {};     // brain +0x0: easing currents
    int32_t rating_tgt[30] = {}; // brain +0x78: easing targets
    int32_t ease[3] = {0, 0, 0}; // brain +0x12c/+0x130/+0x134
    int overtake_index = 0;      // brain +0x165
    int32_t spd_a[5] = {};       // brain +0x1a8 levels (current2 source)
    int32_t spd_b[5] = {};       // brain +0x1bc levels (current source)
    int overtake_latched = -1;   // brain +0x180: latched overtake node (riders_ index, -1 null)
    int32_t reach = 0;           // brain +0x1d4: overtake distance scale
};

struct Rider {
    Bike bike;
    int route = 0;                          // which branch route (0 = half 0)
    int slot = 0;                           // grid slot 0..7 (0x00464218); 0 is the player
    int rider_idx = 0;                      // 0x00463c78 row / personality index (0x00405990 param_6)
    size_t sample = 0, previous_sample = 0; // bike +0x140 / +0x13c
    bool ai = false;
    bool frozen = false;                    // solo oracle: think/move nulled, scanned but never stepped
    AiState brain;
    bool finished = false;
    int32_t finish_tick = -1;
    CrashActor crash_actor;
    ActorAnimationState crash_bike_animation;
    bool crash_active=false;
    int32_t crash_timer = 0;
    int colour = 0;                         // rider colour remap index (0x0046e158)
    RiderStats stamina;
    int weapon = 0;                         // carried weapon (+0x1c8)
    int32_t steal_until = 0;                // +0x1cc: no steal before this tick (0x004096f0)
    int32_t hit_until = 0;                  // +0x1d0: no hit damage before this tick (0x004096f0)
    ActorAnimationState ride_animation;     // riding/attack block +0x70, advanced in Race::step (0x0043ae90)
    Attack attack;                          // attack.weapon latches +0x1d4 at swing start (0x00434660)
    // Riding animation selected by contact 0x004052c0 reason 4 without a crash
    // (+0x80/+0x81 via 0x0040ac70); -1 when none pending.
    int8_t contact_animation = -1;
    int32_t invulnerable_until = 0;         // legacy alias of hit_until (+0x1d0)
    // Nitro (player-global 0x0040b110/0x0040b170/0x0040b1a0/0x0040adb0):
    // per-rider copies of the charge accumulator (Q8) and throttle force.
    int32_t nitro_charge = 0;               // accumulator Q8 (DAT_004c5f10)
    int32_t nitro_throttle = 0;             // +0x240 throttle force holding 0x0040adb0 output
};

// Per-tick snapshot for the simulation oracle (plan 10). Rider fields are
// keyed "rider.<grid slot>.<field>" from tests/sim_fields.hpp; the tick and
// rng come first. Traffic cars are appended by TrafficSystem.
struct FieldValue { std::string name; int64_t value; };
struct RaceSnapshot { int32_t tick; uint32_t rng; std::vector<FieldValue> fields; };
// Riders on a course: bike physics (bike.hpp), movement along the route
// (0x004395e0/0x00439270/0x00439390/0x004394c0), ground height and vertical motion
// (0x00439b10 case 0, 0x0043a190), branch choice at splits, AI racing (0x00407bd0 state 1)
// and the chase camera (0x00437c30) on rider 0, the player.
class Race {
public:
    // route_a/route_b: make_route(course,false/true). Identical up to the first split.
    // road_base_q8: absolute Q8 anchor of the route. The original sub-slice
    // +0xcc is absolute while RouteSample::offset accumulates from 0 at the
    // course root, so grid placement adds this. Napa measures 7 world units
    // from the solo golden frame 0 (18176 - 16384 = 1792); traversal
    // mechanism open, other courses unmeasured (default 0).
    Race(std::vector<RouteSample> route_a, std::vector<RouteSample> route_b,
         const PhysicsTables&, const BikeSpec&, int32_t rider_value, std::vector<Opponent> opponents = {},
         int level = 0, int32_t road_base_q8 = 0,
         const std::vector<Personality>& personalities = {},
         const std::vector<uint8_t>& exe_image = {});   // level 0..4 (0x004b8a12)
    void set_terrain_source(std::function<const TerrainSlice&(const RouteSample&)> source) { terrain_source_=std::move(source); }
    // Free traffic steps with the race on the player's route (0x0044b3b0 calls
    // 0x0044dd90 with the per-step camera advance). The random-call
    // interleaving against the original's scheduler is not verified.
    void set_traffic(TrafficSystem* t) { traffic_ = t; }
    void set_crash_library(const ActorAnimationLibrary&,const CrashConfig&);
    void set_ride_library(const ActorAnimationLibrary* library,
                          const std::array<std::array<std::array<int8_t,4>,2>,2>* attacks) {
        ride_library_ = library; attack_codes_ = attacks; }
    // Nitro setup 0x0040b0e0: copy six words from the player spec at +0x14c
    // and start the accumulator at maximum<<8. Call once after construction
    // (the game calls it from 0x00405600 right after the player build).
    void set_nitro(const NitroConfig& c) { nitro_ = c; nitro_charges_ = c.charges;
        for (auto& r : riders_) r.nitro_charge = c.max << 8; }
    const NitroConfig& nitro() const { return nitro_; }
    int32_t nitro_charges() const { return nitro_charges_; }
    // Demo hold (DAT_004753c8): attract/demo races and oracle captures run
    // with this set; the player rider think takes the AI branch (0x00434660
    // head: stamina regen, then 0x00407bd0) and ignores the control word.
    // Human races leave it clear. The attract demo (plan 13) will set it.
    // Enabling also seeds the player AI brain like an opponent build
    // (0x00405990/0x00407990/0x00405c80): base/current speed, lane, bias.
    // The oracle demo player is character 1's rider 8 (base 95).
    // Demo hold (DAT_004753c8): attract/demo races and oracle captures run
    // with this set; the player rider think takes the AI branch (0x00434660
    // head: stamina regen, then 0x00407bd0) and ignores the control word.
    // Human races leave it clear. The attract demo (plan 13) will set it.
    // Enabling also seeds the player AI brain like an opponent build
    // (0x00405990/0x00407990/0x00405c80): base/current speed, lane, bias.
    // The oracle demo player is character 1's rider 8 (base 95).
    // Rider params come from its 0x00463c78 row (value +0xc/2, weapon
    // +0x18+level*4, stats +0x30..+0x3c); rider 8: 91, 2, 7/3000/3000/3000.
    void set_demo_hold(bool v, int base_speed = 0, int lane = 0, int32_t bias = 0,
                       int weapon = 0, RiderStats stats = {});
    void step(uint32_t controls);      // one simulation tick
    const std::vector<Rider>& riders() const { return riders_; }
    std::vector<Rider>& riders() { return riders_; }
    const Bike& bike() const { return riders_[0].bike; }
    uint32_t position() const { return uint32_t(bike().i32(0x1c)); }
    int32_t lateral() const { return bike().i32(0x28) >> 8; }
    const std::vector<RouteSample>& route() const { return route_of(riders_[0]); }
    const std::vector<RouteSample>& route_of(const Rider& r) const { return routes_[r.route]; }
    const std::vector<RouteSample>& route_for(int road) const { return routes_[road]; }
    bool finished() const { return riders_[0].finished; }
    bool crashed() const { return bike().i32(0x2f4) != 0; }
    int32_t ticks() const { return ticks_; }
    uint32_t camera_position() const;
    int32_t horizon_scroll() const { return rr::horizon_scroll(camera_heading_); }
    int32_t camera_x() const { return camera_x_; }   // 0x004b7ee8: camera lateral, world units
    Camera camera() const;              // relative to the route's own road at camera_position()
    int32_t speed() const { return bike().i32(0xec); }
    int gear() const { return bike().i32(0x23c); }
    // Simulation oracle hooks (plan 10): read and write the MSVC rand state
    // (0x00455eb0) and dump every snapshotted field.
    uint32_t rng_state() const { return seed_; }
    void set_rng_state(uint32_t s) { seed_ = s; }
    RaceSnapshot snapshot() const;
    int place(size_t rider) const;      // 1-based race order (finished riders by finish tick)
    int last_target() const { return last_target_; }   // rider most recently hit by the player
    // Sound events queued this tick (0x00436830 call sites that involve the player).
    std::vector<SoundEventRecord> take_sound_events() { auto e = std::move(sound_events_); sound_events_.clear(); return e; }
    bool wrecked() const { return bike().i32(0x2f0) == 0; }   // bike condition used up
    // Shared knock-off decision 0x004052c0 (plan 16 Task 1): public for the
    // contact unit tests; the tick path calls it from contact_car. No RNG
    // inside; grunt/crash draws stay upstream in the 0x004049c0 caller.
    bool knock_off(Rider& victim, int32_t stamina_damage, int32_t bike_damage,
                   int32_t reason, bool owner_local = true, int32_t owner_flag = 0,
                   std::optional<std::array<int32_t,3>> rider_impulse = {},
                   std::optional<std::array<int32_t,3>> bike_impulse = {});
private:
    SliceEdges edges(const Rider&, size_t sample) const;
    void move_along(Rider&, int32_t distance, bool accumulate_sweep = true);
    void cross_sample(Rider&, int32_t delta);
    void accumulate(Rider&, uint32_t from, uint32_t to, size_t from_sample, size_t to_sample);
    void move(Rider&, bool player);
    void vertical(Rider&);
    void walls(Rider&);
    void crash(Rider&);
    void step_crash(Rider&,bool player,uint32_t controls);
    void step_rider(Rider&, uint32_t controls, bool player);
    void ai_think(Rider&);
    void ai_replan(Rider&);                 // 0x004082f0
    void ai_transition(Rider&);             // 0x00408ad0
    void ai_state_racing(Rider&);           // 0x00409000
    void ai_state_fight(Rider&);            // 0x0040a4f0 (state 5)
    bool ai_overtake(Rider&);               // 0x00408e40: true when a target was latched
    int ai_scan_replan(const Rider&, bool forward, int32_t cap) const;  // 0x00408cd0 via 0x00449ce0
    int ai_scan_overtake(const Rider&, int32_t& gap) const;  // 0x00449e90: nearest closing rider
    int32_t curvature(const Rider&, size_t sample, int samples) const;
    int32_t ai_curve_limit(const Rider&) const;  // 0x00409020
    void ai_curve_target(Rider&);       // 0x004090a0: inside of the curve
    void ai_straight_target(Rider&);    // 0x004091f0
    void ai_speed_steer(Rider&);        // 0x00409f70: steering + speed control
    int ai_attack_anim(const Rider&) const;  // 0x00409510: nonzero while attacking
    bool ai_behind_target(const Rider&) const;  // 0x004092b0
    bool ai_can_overtake(Rider&);   // 0x004092e0 (consumes rand on reach)
    bool ai_fight_range(const Rider&) const;  // 0x0040a4a0
    void collide(Rider&, Rider&);
    // Rider-against-car pass (0x004368f0 pair loop over the scheduler object
    // list, 0x004049c0 bike side for +0xc4 == 0x10 cars; see
    // analysis/car-notes.md). Runs at the traced point in Race::step for
    // every rider, player and AI. Enters crashes with reason 2 (animation
    // 29) through the crash module and queues the original sound events.
    void collide_traffic(Rider&);
    void contact_car(Rider&, TrafficCar&);
    void crash_car(Rider&);   // 0x00403d60 reason 2 (car contact, animation 29)
    void crash_reason(Rider&, int32_t reason, std::array<int32_t,3> rider_impulse,
                      std::array<int32_t,3> bike_impulse);   // reason-tagged crash entry
    void contact(Rider& self, Rider& other);
    void update_camera();
    void fight(Rider&, uint32_t controls, size_t index);
    bool fight_hit_frame(const Rider&) const;  // descriptor flags + frame-2 gate (0x004096f0 head)
    bool fight_reach(const Rider&, const Rider&, bool side) const;  // 0x00409c10
    void resolve_hit(Rider& attacker, size_t index);
    void nitro_think(Rider&, uint32_t controls, bool player);  // 0x0040b110/0x0040b170 (player, 0x00434660)
    void nitro_think_activate(Rider&);  // 0x0040b110 body
    void nitro_think_stop(Rider&);      // 0x0040b170 body
    void nitro_charge_tick(Rider&);                            // 0x0040b1a0 (player, 0x00407a90)
    int32_t nitro_throttle_force(Rider&, int32_t target);      // 0x0040adb0 boost branch
    void advance_ride_animation(Rider&, bool player);          // riding block in Race::step (0x00407a90/0x00407f30/0x0043ae90)
    uint32_t random();

    std::function<const TerrainSlice&(const RouteSample&)> terrain_source_;
    TrafficSystem* traffic_ = nullptr;
    const ActorAnimationLibrary* crash_library_ = nullptr;
    const ActorAnimationLibrary* ride_library_ = nullptr;
    const std::array<std::array<std::array<int8_t,4>,2>,2>* attack_codes_ = nullptr;
    CrashConfig crash_config_;
    NitroConfig nitro_;
    int32_t nitro_charges_ = 0;
    std::vector<RouteSample> routes_[2];
    PhysicsTables tables_;
    std::vector<Rider> riders_;
    int32_t ticks_ = 0;
    int level_ = 0;
    int last_target_ = -1;
    bool demo_hold_ = false;   // DAT_004753c8 (see set_demo_hold)
    int32_t road_base_ = 0;   // absolute Q8 anchor (ctor param)
    std::vector<uint8_t> personalities_exe_;   // reference image for EXE-resident tables (e.g. 0x00465a10)
    uint32_t seed_ = 1;   // MSVC rand() state (0x00455eb0)
    std::vector<SoundEventRecord> sound_events_;
    void sound(SoundEvent, const Bike&);
    uint32_t previous_camera_position_=0;
    int32_t camera_heading_=0;
    int32_t camera_x_ = 0, camera_height_ = 150, camera_lift_ = 0, camera_back_ = 0;
};
}
