#include "police.hpp"
#include "exe_tables.hpp"
#include <stdexcept>
namespace rr {
namespace {
int32_t word(const std::vector<uint8_t>& bytes, size_t p) {
    const uint32_t u = uint32_t(bytes[p]) | uint32_t(bytes[p + 1]) << 8 | uint32_t(bytes[p + 2]) << 16 |
                       uint32_t(bytes[p + 3]) << 24;
    return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
}
// 0x00407f76/0x00407faa/0x00407fbe: CDQ; XOR; SUB, then a signed
// threshold comparison. INT32_MIN remains negative under the x86 wrap.
int32_t magnitude(int32_t value) {
    const uint32_t u = value < 0 ? 0u - uint32_t(value) : uint32_t(value);
    return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
}
int32_t difference(int32_t a, int32_t b) {
    const uint32_t u = uint32_t(a) - uint32_t(b);
    return u <= INT32_MAX ? int32_t(u) : int32_t(int64_t(u) - 0x100000000LL);
}
} // namespace
PoliceConfig read_police_config(const std::vector<uint8_t>& exe) {
    PoliceConfig config;
    const auto bike = read_exe_data(exe, 0x464310, 104), rider = read_exe_data(exe, 0x464378, 104);
    const auto personality = read_exe_data(exe, 0x464558 + 9 * 0x204, 0x204),
               stats = read_exe_data(exe, 0x463c78 + 15 * 0x40, 64);
    const auto bikes = read_exe_data(exe, 0x4641c8 + 15 * 5, 5), messages = read_exe_data(exe, 0x464410, 32),
               throttle = read_exe_data(exe, 0x4642a0, 20);
    for (size_t i = 0; i < 26; ++i) {
        config.bike_template[i] = word(bike, i * 4);
        config.rider_template[i] = word(rider, i * 4);
    }
    for (size_t i = 0; i < 129; ++i) config.personality[i] = word(personality, i * 4);
    for (size_t i = 0; i < 16; ++i) config.stats[i] = word(stats, i * 4);
    for (size_t i = 0; i < 5; ++i) {
        config.bikes[i] = bikes[i];
        config.throttle_percent[i] = word(throttle, i * 4);
    }
    for (size_t i = 0; i < 8; ++i) config.messages[i] = word(messages, i * 4);
    if (config.bike_template[1] != 4 || config.rider_template[1] != 8)
        throw std::runtime_error("Invalid police behavior templates");
    return config;
}
PolicePursuit police_pursuit(const PoliceConfig& config, int level, int place, int player_personality) {
    if (level < 0 || level > 4 || place < 0 || place > 15 || player_personality < 0 || player_personality > 9)
        throw std::runtime_error("Invalid police pursuit input");
    PolicePursuit pursuit;
    pursuit.personality = config.personality;
    for (size_t i = 0; i < 10; ++i) pursuit.personality[i * 3 + 1] = pursuit.personality[0x7c / 4 + i * 3];
    pursuit.personality[0x1a4 / 4] = pursuit.personality[0x1bc / 4 + size_t(level)];
    const int factor = 16 - (place < 4 ? 4 - place : 0);
    const int32_t relationship = factor * 4000 - level * 6000;
    pursuit.personality[0x80 / 4 + size_t(player_personality) * 3] = relationship;
    pursuit.personality[0x8 / 4 + size_t(player_personality) * 3] = relationship;
    const int32_t base = factor * 2000;
    pursuit.player_relationship = base + (base - level * 3000) / 2 - level * 3000;
    return pursuit;
}
PoliceActor make_police_actor(const PoliceConfig& config, const PhysicsTables& tables,
                              const std::vector<BikeSpec>& specs, const ActorAnimationLibrary& library,
                              const PoliceSpawnInput& input) {
    if (input.lane < 0 || input.lane > 2) throw std::runtime_error("Invalid police lane");
    PoliceActor actor;
    actor.pursuit = police_pursuit(config, input.level, input.player_place, input.player_personality);
    actor.stats = config.stats;
    const size_t spec = config.bikes[size_t(input.level)];
    if (spec >= specs.size()) throw std::runtime_error("Police bike spec unavailable");
    for (int a = 0; a < 2; ++a) {
        Bike& body = a ? actor.rider : actor.bike;
        const auto& behavior = a ? config.rider_template : config.bike_template;
        PhysicsTables constants = tables;
        constants.bike_object = behavior;
        init_bike_object(body, constants);
        for (int i = 0; i < 4; ++i) {
            body.i32(0xdc + i * 4) = behavior[size_t(i) + 3];
            body.i32(0x1a4 + i * 4) = behavior[size_t(i) + 22];
        }
        body.i32(0x1c) = body.i32(0x30) = body.i32(0x134) = body.i32(0x138) = input.position;
        body.i32(0x24) = body.i32(0x38) = input.ground;
        body.i32(0x28) = body.i32(0x3c) = input.lateral_q8;
        body.u8(0xc0) |= 4;
        auto& state = a ? actor.rider_animation : actor.bike_animation;
        set_animation_with_fallback(state, library.playback[1], 2, input.tick, library.fallback);
        queue_animation_with_fallback(state, library.playback[1], 2, library.fallback);
        advance_animation(state, library.playback[1], input.tick);
    }
    auto& bike = actor.bike;
    apply_spec(bike, specs[spec], config.stats[3] / 2);
    bike.i32(0x2f0) = bike.i32(0x2ec);
    bike.i32(0x324) = 2;
    bike.u8(0x328) = 15;
    bike.u8(0x300) = 1;
    bike.i32(0x260) = bike.i32(0x260) * config.throttle_percent[size_t(input.level)] / 100;
    bike.i32(0x1d0) -= 25;
    bike.i32(0x1e0) -= 15;
    bike.i32(0x1f0) -= 8;
    bike.i32(0x200) -= 4;
    bike.i32(0x28) = input.lane_target_q8;
    actor.rider.i32(0x1c8) = actor.rider.i32(0x1d4) = config.stats[size_t(6 + input.level)];
    actor.stats[14] = actor.stats[15] = actor.stats[13];
    actor.stats[1] = int32_t((uint32_t(actor.stats[1]) & 0xffff00ffu) | (uint32_t(spec) << 8));
    actor.stats[0] = actor.stats[4] = 0;
    actor.pursuit.personality[0x18c / 4] = input.lane;
    actor.pursuit.personality[0x194 / 4] = input.lane_target_q8;
    actor.pursuit.personality[0x198 / 4] = input.position + 960;
    actor.player_relationship = actor.pursuit.player_relationship;
    advance_animation(actor.bike_animation, library.playback[1], input.tick);
    return actor;
}
bool police_arrest(const PoliceArrestInput& input) {
    if (input.owner != 0 || (input.rider_flags & 0x10) || (input.bike_flags & 0x10) || !input.found_cop ||
        input.cop_crashed)
        return false;
    if (input.crash == 2 ? magnitude(input.bike_speed) >= 1800 : input.bike_speed >= 300) return false;
    return magnitude(difference(input.cop_position, input.rider_position)) < 512 &&
           magnitude(difference(input.cop_speed, input.bike_speed)) < 5000;
}
bool police_spawn_due(uint32_t tick, int owner, uint8_t rider_flags, bool cop_present) {
    return owner == 0 && !(rider_flags & 0x10) && !cop_present && !(tick & 0xfff);
}
} // namespace rr
