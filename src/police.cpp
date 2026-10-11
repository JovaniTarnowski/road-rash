#include "police.hpp"
#include "exe_tables.hpp"
#include <stdexcept>
namespace rr {
namespace {
int32_t word(const std::vector<uint8_t>& bytes,size_t p) {
    const uint32_t u=uint32_t(bytes[p])|uint32_t(bytes[p+1])<<8|uint32_t(bytes[p+2])<<16|uint32_t(bytes[p+3])<<24;
    return u<=INT32_MAX?int32_t(u):int32_t(int64_t(u)-0x100000000LL);
}
int64_t magnitude(int32_t value){return value<0?-int64_t(value):int64_t(value);}
int32_t difference(int32_t a,int32_t b){const uint32_t u=uint32_t(a)-uint32_t(b);return u<=INT32_MAX?int32_t(u):int32_t(int64_t(u)-0x100000000LL);}
}
PoliceConfig read_police_config(const std::vector<uint8_t>& exe) {
    PoliceConfig config;
    const auto bike=read_exe_data(exe,0x464310,104),rider=read_exe_data(exe,0x464378,104);
    const auto personality=read_exe_data(exe,0x464558+9*0x204,0x204),stats=read_exe_data(exe,0x463c78+15*0x40,64);
    const auto bikes=read_exe_data(exe,0x4641c8+15*5,5),messages=read_exe_data(exe,0x464410,32);
    for(size_t i=0;i<26;++i){config.bike_template[i]=word(bike,i*4);config.rider_template[i]=word(rider,i*4);}
    for(size_t i=0;i<129;++i)config.personality[i]=word(personality,i*4);
    for(size_t i=0;i<16;++i)config.stats[i]=word(stats,i*4);
    for(size_t i=0;i<5;++i)config.bikes[i]=bikes[i];
    for(size_t i=0;i<8;++i)config.messages[i]=word(messages,i*4);
    if(config.bike_template[1]!=4 || config.rider_template[1]!=8)throw std::runtime_error("Invalid police behavior templates");
    return config;
}
PolicePursuit police_pursuit(const PoliceConfig& config,int level,int place,int player_personality) {
    if(level<0 || level>4 || place<0 || place>15 || player_personality<0 || player_personality>9)throw std::runtime_error("Invalid police pursuit input");
    PolicePursuit pursuit;pursuit.personality=config.personality;
    for(size_t i=0;i<10;++i)pursuit.personality[i*3+1]=pursuit.personality[0x7c/4+i*3];
    pursuit.personality[0x1a4/4]=pursuit.personality[0x1bc/4+size_t(level)];
    const int factor=16-(place<4?4-place:0);
    const int32_t relationship=factor*4000-level*6000;
    pursuit.personality[0x80/4+size_t(player_personality)*3]=relationship;
    pursuit.personality[0x8/4+size_t(player_personality)*3]=relationship;
    const int32_t base=factor*2000;
    pursuit.player_relationship=base+(base-level*3000)/2-level*3000;
    return pursuit;
}
bool police_arrest(const PoliceArrestInput& input) {
    if(input.owner!=0 || (input.rider_flags&0x10) || (input.bike_flags&0x10) || !input.found_cop || input.cop_crashed)return false;
    if(input.crash==2?magnitude(input.bike_speed)>=1800:input.bike_speed>=300)return false;
    return magnitude(difference(input.cop_position,input.rider_position))<512 && magnitude(difference(input.cop_speed,input.bike_speed))<5000;
}
bool police_spawn_due(uint32_t tick,int owner,uint8_t rider_flags,bool cop_present) {
    return owner==0 && !(rider_flags&0x10) && !cop_present && !(tick&0xfff);
}
}
