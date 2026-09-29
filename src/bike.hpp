#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rr {
// SPEC resource from BIKESPEC.RSC: 89 little-endian dwords (356 bytes). 0x00405ea0 reads
// d[0] (byte), d[1..5], d[6] (mass) and copies d[7..82] to bike object +0x1c4..+0x2f3.
struct BikeSpec { std::array<int32_t,89> d{}; };
std::vector<BikeSpec> read_bike_specs(const std::vector<uint8_t>& bikespec_rsc);

// Constant tables the riding code reads from ROADRASH.EXE .data.
struct PhysicsTables {
    int32_t contact_base = 74;                 // 0x0046dcd8
    std::array<int8_t,4> surface_drag{};      // 0x0046dce4, by surface class
    std::array<int32_t,4> grip{};             // 0x0046dce8
    std::array<int32_t,4> steer_gain{};       // 0x0046dcf8
    std::array<int32_t,4> steer_damp{};       // 0x0046dd08
    std::array<int32_t,4> skid_loss{};        // 0x0046dd18
    std::array<int32_t,32> curve{};           // 0x0046dd28: [surface + 4*((position>>8)&7)]
    std::array<int32_t,1024> sine{};          // 0x00475768: quarter wave, Q15
    std::array<int32_t,26> bike_object{};     // 0x00463ba8: bike behaviour/constant table
    static PhysicsTables synthetic();          // simple values for unit tests; not original data
};
PhysicsTables read_physics_tables(const std::vector<uint8_t>& exe);

// Bike object. Stored as the original byte layout so ported code addresses the same
// offsets as the executable (comments name them); only the fields the riding code uses
// are meaningful here.
struct Bike {
    std::array<uint8_t,0x340> m{};
    int32_t accel = 0;        // 0x004c3cec: last acceleration computed by 0x004390c0
    int32_t skid_ticks = 0;   // 0x0046dce0: player skid duration (a global in the original; one player)
    int32_t& i32(int offset) { return *reinterpret_cast<int32_t*>(m.data() + offset); }
    int32_t i32(int offset) const { int32_t v; std::memcpy(&v, m.data() + offset, 4); return v; }
    uint8_t& u8(int offset) { return m[size_t(offset)]; }
    uint8_t u8(int offset) const { return m[size_t(offset)]; }
};

void init_bike_object(Bike&, const PhysicsTables&);            // 0x00438930/0x004387a0 table copy
void apply_spec(Bike&, const BikeSpec&, int32_t rider_value);  // 0x00405ea0
void throttle(Bike&, int32_t target);                          // 0x0040adb0
void brake(Bike&, int32_t target);                             // 0x0040aea0
void drive_mode(Bike&, int mode);                              // 0x0040acd0: 0 coast, 1 throttle, 2 brake
void steer(Bike&, const PhysicsTables&, int32_t steer_q16, int32_t lean);   // 0x0040af50
void steer_input(Bike&, const PhysicsTables&, int mode, int32_t lean);     // 0x0040ad30
void apply_controls(Bike&, const PhysicsTables&, uint32_t controls);       // player part of 0x00434660
void shift_gears(Bike&);                                        // 0x004391d0
void integrate_speed(Bike&, const PhysicsTables&, int surface = 0);        // 0x004390c0
void traction(Bike&, const PhysicsTables&, int surface, bool player);      // 0x00438c70

// Sub-slice edges in world units (0x00454240 fields [0x38],[0x39],[0x3a],[0x3b]).
struct SliceEdges { int32_t left, right, left_shoulder, right_shoulder; uint8_t flags, type; };
int surface_class(const SliceEdges&, int32_t x_q8);              // 0x0043a0c0
int32_t approx_atan2(int32_t a, int32_t b);                      // 0x0044f5a0
int32_t sine(const PhysicsTables&, uint32_t angle);              // 0x0044f4a0: 4096 per turn, Q16

// Control bits (tables 0x00465fb9 / 0x00476c00 order).
constexpr uint32_t control_accelerate = 0x1, control_brake = 0x2, control_left = 0x4,
                   control_lean_down = 0x8, control_nitro = 0x10, control_right = 0x200, control_lean_up = 0x400;
}
