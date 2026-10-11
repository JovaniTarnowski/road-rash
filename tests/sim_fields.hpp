#pragma once
#include <cstddef>
#include <cstdint>

// Shared per-tick field table for the simulation oracle (plan 10). The
// offsets are original object words (see docs/REVERSE_ENGINEERING.md
// "Simulation step and object lists"); the native snapshot reads the matching
// members by name. oracle/sim_fields.h carries the same list in C; tests
// check the two agree.
namespace rr {
namespace sim {

struct SimField {
    const char* name;
    uint32_t offset;
    uint8_t width;
    bool is_signed;
};

// Bike words read through Bike::i32/u8; rider words read from Rider members.
inline constexpr SimField rider_fields[] = {
    {"position", 0x1c, 4, true},
    {"lateral", 0x28, 4, true},
    {"height", 0x20, 4, true},
    {"velocity", 0xf4, 4, true},
    {"speed", 0xec, 4, true},
    {"gear", 0x23c, 4, true},
    {"condition", 0x2f0, 4, true},
    {"crash", 0x2f4, 4, true},
    {"slice", 0x140, 4, true},
    {"flags", 0xc0, 1, false},
    {"stamina_current", 0x3c, 4, true},
    {"stamina_max", 0x38, 4, true},
    {"weapon", 0x1c8, 4, true},
    {"ai_target_x", 0x194, 4, true},
    {"ai_target_position", 0x198, 4, true},
    {"ai_target_speed", 0x19c, 4, true},
    {"ai_current_speed", 0x1a4, 4, true},
    {"ai_base_speed", 0x1bc, 4, true},
    {"ai_replan", 0x188, 4, true},
    {"ai_lane", 0x18c, 4, true},
    {"ai_bias", 0x190, 4, true},
};

// TrafficCar members; "active" has no original offset (pool slot state).
inline constexpr SimField car_fields[] = {
    {"active", 0xffffffffu, 1, false},
    {"kind", 0x73, 1, false},
    {"vehicle", 0x7f, 1, false},
    {"lane", 0x7a, 1, true},
    {"position", 0x1c, 4, true},
    {"lateral", 0x28, 4, true},
    {"speed", 0xec, 4, true},
    {"phase", 0x71, 4, true},
};

}  // namespace sim
}  // namespace rr
