/* Shared per-tick field table for the simulation oracle (plan 10). C mirror
 * of tests/sim_fields.hpp; the test checks names, offsets, widths and order
 * agree. Offsets are original object words. */
#ifndef RR_SIM_FIELDS_H
#define RR_SIM_FIELDS_H

#include <stdint.h>

typedef struct { const char* name; uint32_t offset; uint8_t width; uint8_t is_signed; } rr_sim_field;

static const rr_sim_field rr_sim_rider_fields[] = {
    {"position", 0x1c, 4, 1},
    {"lateral", 0x28, 4, 1},
    {"height", 0x20, 4, 1},
    {"velocity", 0xf4, 4, 1},
    {"speed", 0xec, 4, 1},
    {"gear", 0x23c, 4, 1},
    {"condition", 0x2f0, 4, 1},
    {"crash", 0x2f4, 4, 1},
    {"slice", 0x140, 4, 1},
    {"flags", 0xc0, 1, 0},
    {"steer", 0x288, 4, 1},
    {"steer_rate", 0xf0, 4, 1},
    {"lat_vel", 0x10c, 4, 1},
    {"ground", 0x160, 4, 1},
    {"stamina_current", 0x3c, 4, 1},
    {"stamina_max", 0x38, 4, 1},
    {"weapon", 0x1c8, 4, 1},
    {"ai_target_x", 0x194, 4, 1},
    {"ai_target_position", 0x198, 4, 1},
    {"ai_target_speed", 0x19c, 4, 1},
    {"ai_current_speed", 0x1a4, 4, 1},
    {"ai_base_speed", 0x1bc, 4, 1},
    {"ai_replan", 0x188, 4, 1},
    {"ai_lane", 0x18c, 4, 1},
    {"ai_bias", 0x190, 4, 1},
    {"anim", 0x80, 1, 1},
    {"anim_frame", 0x81, 1, 1},
    {"anim_queued", 0x82, 1, 1},
    {"anim_loops", 0x88, 1, 0},
    {"anim_deadline", 0x84, 4, 1},
    {"attack_weapon", 0x1d4, 4, 1},
    {"steal_until", 0x1cc, 4, 1},
    {"hit_until", 0x1d0, 4, 1},
    {"nitro_charge", 0xfffffffeu, 4, 1},
    {"nitro_charges", 0xfffffffc, 4, 1}
};

static const rr_sim_field rr_sim_car_fields[] = {
    {"active", 0xffffffffu, 1, 0},
    {"kind", 0x73, 1, 0},
    {"vehicle", 0x7f, 1, 0},
    {"lane", 0x7a, 1, 1},
    {"position", 0x1c, 4, 1},
    {"lateral", 0x28, 4, 1},
    {"speed", 0xec, 4, 1},
    {"phase", 0x71, 4, 1}
};

#define RR_SIM_RIDER_FIELD_COUNT (sizeof rr_sim_rider_fields / sizeof rr_sim_rider_fields[0])
#define RR_SIM_CAR_FIELD_COUNT (sizeof rr_sim_car_fields / sizeof rr_sim_car_fields[0])

#endif
