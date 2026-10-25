#include "crash.hpp"
#include "exe_tables.hpp"
namespace rr {
namespace {
int32_t word(const std::vector<uint8_t>& bytes, size_t p) {
    const uint32_t u = uint32_t(bytes[p]) | uint32_t(bytes[p + 1]) << 8 | uint32_t(bytes[p + 2]) << 16 |
                       uint32_t(bytes[p + 3]) << 24;
    return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
}
int32_t negate(int32_t a) {
    const uint32_t u = 0u - uint32_t(a);
    return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
}
int32_t add(int32_t a, int32_t b) {
    const uint32_t u = uint32_t(a) + uint32_t(b);
    return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
}
} // namespace
CrashConfig read_crash_config(const std::vector<uint8_t>& exe) {
    CrashConfig config;
    const auto dimensions = read_exe_data(exe, 0x4643f8, 12), behavior = read_exe_data(exe, 0x463c10, 26 * 4),
               normal = read_exe_data(exe, 0x4643e0, 12);
    for (size_t i = 0; i < 3; ++i) {
        config.bike_dimensions[i] = word(dimensions, i * 4);
        config.normal_dimensions[i] = word(normal, i * 4);
    }
    for (size_t i = 0; i < 26; ++i) config.rider_template[i] = word(behavior, i * 4);
    return config;
}
void init_crash_actor(CrashActor& actor, const CrashConfig& config) {
    actor = {};
    PhysicsTables tables;
    tables.bike_object = config.rider_template;
    init_bike_object(actor.motion, tables);
}
void remount_crash_actor(Bike& bike, ActorAnimationState& bike_animation, CrashActor& rider,
                         const CrashConfig& config, const ActorAnimationBank& bank,
                         const std::vector<int8_t>& fallback, uint32_t tick) {
    bike.i32(0x2f4) = 0;
    for (auto state : {&bike_animation, &rider.animation}) {
        set_animation_with_fallback(*state, bank, 0, tick, fallback);
        queue_animation_with_fallback(*state, bank, 0, fallback);
    }
    bike_animation.selected.reset();
    bike.i32(0x154) = 8;
    rider.motion.u8(0xc0) &= 0xfc;
    bike.u8(0x244) = 0;
    for (int offset : {0x114, 0x118, 0xf0, 0x288}) bike.i32(offset) = 0;
    for (size_t i = 0; i < 3; ++i) bike.i32(0x174 + int(i) * 4) = config.normal_dimensions[i];
    for (int offset = 0x1c; offset <= 0x40; offset += 4) rider.motion.i32(offset) = bike.i32(offset);
    for (int offset : {0x13c, 0x140, 0x134, 0x138, 0x58, 0x5c, 0x60, 0xec, 0xf0, 0xf4, 0x108, 0x10c, 0x110})
        rider.motion.i32(offset) = bike.i32(offset);
}
CrashRecoveryAction recover_crash_actor(Bike& bike, CrashActor& rider, const ActorAnimationBank& bank,
                                        const std::vector<int8_t>& fallback, uint32_t tick,
                                        CrashRecoveryInput& input) {
    auto& body = rider.motion;
    auto& state = rider.animation;
    const auto set = [&](int8_t code) { set_animation_with_fallback(state, bank, code, tick, fallback); };
    const auto queue = [&](int8_t code) { queue_animation_with_fallback(state, bank, code, fallback); };
    if (bike.i32(0x2f4) == 3)
        return state.animation != 41 ? CrashRecoveryAction::remount : CrashRecoveryAction::none;
    if (bike.i32(0x2f4) != 2) return CrashRecoveryAction::none;
    if (input.state_elapsed > 1200 && !input.police_owner) {
        if (!bike.i32(0x2f0)) {
            body.i32(0xec) = 0;
            if (state.animation != 37) set(37);
            if (input.player) return CrashRecoveryAction::wrecked;
        } else {
            set(41);
            queue(2);
            bike.i32(0x2f4) = 3;
        }
    }
    if (state.animation == 38) {
        const int32_t longitudinal = add(bike.i32(0x1c), negate(body.i32(0x1c)));
        const int32_t lateral = add(add(bike.i32(0x28), negate(body.i32(0x28))), -6000);
        const auto magnitude = [](int32_t value) { return value < 0 ? -int64_t(value) : int64_t(value); };
        if (magnitude(longitudinal) <= 9 && magnitude(lateral) < 6001) {
            body.i32(0xec) = 0;
            if (bike.i32(0x2f0)) {
                body.i32(0x28) = add(body.i32(0x28), 6000);
                bike.i32(0x28) = body.i32(0x28);
                set(41);
                queue(2);
                bike.i32(0x2f4) = 3;
            } else
                set(37);
        } else {
            int32_t toward = longitudinal, side = lateral;
            if (input.player && (input.walk_controls & 0x7800)) {
                const auto controls = input.walk_controls;
                toward = (controls & 0x1000) && longitudinal < 0   ? -1
                         : (controls & 0x800) && longitudinal >= 1 ? 1
                                                                   : 0;
                side = controls & 0x4000 ? -256 : int32_t((controls & 0x2000) >> 5);
                input.state_elapsed = 0;
            }
            const uint32_t angle = uint32_t(approx_atan2(toward, side / 256) & 255) * 65536;
            body.i32(0x128) =
                angle > 0x7fffff && angle != 0x800000 ? int32_t(int64_t(angle) - 0x1000000) : int32_t(angle);
            body.i32(0x5c) = body.i32(0x128);
            body.i32(0xec) = 1200;
        }
    } else if (state.animation == 37) {
        // 0x00449c20 uses a logical sign shift, preserving its unusual result
        // for a negative longitudinal difference.
        const uint32_t difference = uint32_t(bike.i32(0x1c)) - uint32_t(body.i32(0x1c));
        const uint32_t sign = difference >> 31;
        const uint32_t bits = (difference ^ sign) - sign;
        const int64_t distance = bits <= INT32_MAX ? int64_t(bits) : int64_t(bits) - 0x100000000LL;
        if (!bike.i32(0x2f0) && distance < 80) {
            if (state.loops >= 1) {
                if (input.player) return CrashRecoveryAction::wrecked;
                bike.i32(0x2f0) = bike.i32(0x2ec) / 4;
            }
        } else if (state.loops > 1)
            queue(38);
    }
    return CrashRecoveryAction::none;
}
void advance_actor_motion(Bike& actor, const PhysicsTables& tables,
                          const std::function<void(int32_t)>& advance_route) {
    const auto mul = [](int32_t a, int32_t b) {
        const uint32_t u = uint32_t(a) * uint32_t(b);
        return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
    };
    if (actor.u8(0x19c)) {
        actor.i32(0xf0) = add(actor.i32(0xf0), actor.i32(0x188) / 2);
        actor.i32(0xf4) = add(actor.i32(0xf4), actor.i32(0x18c) / 2);
        actor.i32(0xec) = add(actor.i32(0xec), actor.i32(0x184) / 2);
        actor.i32(0x188) = actor.i32(0x18c) = actor.i32(0x184) = 0;
    }
    if (!actor.i32(0x128)) {
        actor.i32(0x10c) = 0;
        actor.i32(0x108) = actor.i32(0xec);
    } else {
        const uint32_t angle = (uint32_t(actor.i32(0x128)) & 0xfff000) >> 12;
        actor.i32(0x108) = mul(sine(tables, angle + 1024) / 256, actor.i32(0xec)) / 256;
        actor.i32(0x10c) = mul(sine(tables, angle) / 256, actor.i32(0xec)) / 256;
    }
    for (int offset = 0; offset < 20; offset += 4) actor.i32(0x30 + offset) = actor.i32(0x1c + offset);
    const int32_t dt = actor.i32(0x18);
    const uint32_t term_bits = uint32_t(mul(actor.i32(0x108), 2)) - uint32_t(mul(actor.accel, dt));
    const int32_t term =
        term_bits <= INT32_MAX ? int32_t(term_bits) : int32_t(int64_t(term_bits) - 0x100000000LL);
    advance_route(mul(term, dt) >> 9);
    actor.i32(0x28) = add(actor.i32(0x28), mul(dt, actor.i32(0x10c)));
}
bool begin_crash(Bike& bike, CrashActor& rider, const CrashConfig& config, const ActorAnimationBank& bank,
                 const std::vector<int8_t>& fallback, uint32_t tick, int32_t reason,
                 std::optional<std::array<int32_t, 3>> rider_impulse,
                 std::optional<std::array<int32_t, 3>> bike_impulse) {
    if (bike.i32(0x2f4)) return false;
    auto& body = rider.motion;
    for (int offset = 0x1c; offset <= 0x40; offset += 4) body.i32(offset) = bike.i32(offset);
    for (int offset : {0x58, 0x5c, 0x60, 0xec, 0xf0, 0xf4, 0x104, 0x108, 0x10c, 0x110, 0x12c, 0x130, 0x134,
                       0x138, 0x13c, 0x140, 0x158, 0x160})
        body.i32(offset) = bike.i32(offset);
    body.i32(0x60) = 0;
    bike.i32(0x154) = 0x5f;
    body.i32(0x154) = 0x46;
    for (int offset : {0x118, 0x11c, 0x114, 0x120, 0x124, 0x128, 0x198, 0x1a0}) body.i32(offset) = 0;
    body.u8(0x164) = 0;
    body.u8(0x19c) = 0;
    const std::array<int, 3> velocity_offsets{0xec, 0xf0, 0xf4};
    for (size_t i = 0; i < 3; ++i) {
        if (rider_impulse)
            body.i32(velocity_offsets[i]) = add(body.i32(velocity_offsets[i]), rider_impulse->at(i));
        if (bike_impulse)
            bike.i32(velocity_offsets[i]) = add(bike.i32(velocity_offsets[i]), bike_impulse->at(i));
    }
    body.i32(0x20) = add(body.i32(0x20), 0x4b00);
    bike.i32(0x2f8) = reason;
    bike.i32(0x20) = add(bike.i32(0x20), bike.i32(0x16c));
    bike.i32(0x2f4) = 1;
    body.u8(0xc0) |= 3;
    bike.i32(0xbc) = 0;
    bike.i32(0x288) = 0;
    bike.u8(0x244) = 1;
    for (size_t i = 0; i < 3; ++i) bike.i32(0x174 + int(i) * 4) = config.bike_dimensions[i];
    const int8_t animation = reason == 2 ? 29 : reason == 4 || reason == 5 ? 35 : 33;
    set_animation_with_fallback(rider.animation, bank, animation, tick, fallback);
    queue_animation_with_fallback(rider.animation, bank, -1, fallback);
    return true;
}
} // namespace rr
