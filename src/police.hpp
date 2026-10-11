#pragma once
#include "bike.hpp"
#include <array>
namespace rr {
struct PoliceConfig {
    std::array<int32_t,26> bike_template{},rider_template{};
    std::array<int32_t,129> personality{};
    std::array<int32_t,16> stats{};
    std::array<uint8_t,5> bikes{};
    std::array<int32_t,8> messages{};
};
PoliceConfig read_police_config(const std::vector<uint8_t>& exe);
struct PolicePursuit {
    std::array<int32_t,129> personality{};
    int32_t player_relationship=0;
};
// Relationship and pursuit-speed portion of the offline 0x00406b80 constructor.
PolicePursuit police_pursuit(const PoliceConfig&,int level,int player_place,int player_personality);
struct PoliceArrestInput {
    int owner=0,crash=0;
    uint8_t bike_flags=0,rider_flags=0;
    bool found_cop=false,cop_crashed=false;
    int32_t bike_speed=0,cop_speed=0,rider_position=0,cop_position=0;
};
// Local arrest outcome in 0x00407f30, after 0x00407860 has located the nearby active cop.
bool police_arrest(const PoliceArrestInput&);
bool police_spawn_due(uint32_t tick,int owner,uint8_t rider_flags,bool cop_present);
}
