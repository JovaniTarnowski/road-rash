#pragma once
#include <cstdint>

namespace rr {
// Sound event codes queued by the simulation through 0x00436830 and consumed by 0x004351f0.
enum class SoundEvent : uint8_t {
    crash_light = 0,
    crash_medium = 1,
    crash_hard = 2,
    grunt_a = 3,
    grunt_b = 4,
    grunt_c = 5,
    swish_chain = 6,
    swish_club = 7,
    impact_chain = 8,
    impact_club = 9,
    impact_kick = 10,
    impact_punch = 11,
    footstep = 12,
    skid_on = 13,
    nitro_on = 14,
    slide = 16,
    skid_off = 17,
    horn = 18,
    siren = 19,
    bounce = 20,
    nitro_off = 21,
    speech = 22,
};

// One queued event (0x00497698, 0x1c bytes): code, emitter lateral/height/position and speed.
struct SoundEventRecord {
    SoundEvent code{};
    int32_t lateral = 0, height = 0, position = 0, speed = 0;
    int32_t argument = 0; // speech: character table index
    int32_t source = -1;  // native stable emitter ID, presentation only; not serialized simulation input
};

} // namespace rr
