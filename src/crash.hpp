#pragma once
#include "bike.hpp"
#include "actor_animation.hpp"
namespace rr {
struct CrashConfig {
    std::array<int32_t,3> bike_dimensions{},normal_dimensions{};
    std::array<int32_t,26> rider_template{};
};
CrashConfig read_crash_config(const std::vector<uint8_t>& exe);
struct CrashActor {
    Bike motion;
    ActorAnimationState animation;
    int route=0;
    size_t sample=0,previous_sample=0;
};
void init_crash_actor(CrashActor&,const CrashConfig&);
void remount_crash_actor(Bike&,ActorAnimationState& bike_animation,CrashActor&,const CrashConfig&,
    const ActorAnimationBank&,const std::vector<int8_t>& fallback,uint32_t tick);
struct CrashRecoveryInput {
    int32_t state_elapsed=0;
    bool player=false,police_owner=false;
    uint32_t walk_controls=0; // original 0x7800 direction bits, offline play
};
enum class CrashRecoveryAction { none,remount,wrecked };
// Offline recovery decisions from 0x0040a600. Caller handles route bounds,
// input-directed walking and the remount reset separately.
CrashRecoveryAction recover_crash_actor(Bike&,CrashActor&,const ActorAnimationBank&,
    const std::vector<int8_t>& fallback,uint32_t tick,CrashRecoveryInput&);
// Local motion portion of 0x00439920. The callback applies road-relative
// longitudinal movement (0x00439270) before lateral velocity is added.
void advance_actor_motion(Bike&,const PhysicsTables&,const std::function<void(int32_t)>& advance_route);
// Local, offline branch of 0x00403d60. Route bookkeeping and contact ownership
// are supplied by Race; this routine applies the independent motion states.
bool begin_crash(Bike& bike,CrashActor& rider,const CrashConfig&,const ActorAnimationBank&,
    const std::vector<int8_t>& fallback,uint32_t tick,int32_t reason,
    std::optional<std::array<int32_t,3>> rider_impulse={},std::optional<std::array<int32_t,3>> bike_impulse={});
}
