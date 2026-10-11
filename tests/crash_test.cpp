#include "crash.hpp"
#include "actor_descriptors.hpp"
#include "assets.hpp"
#include "race.hpp"
#include "course.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>
int main(int argc,char** argv) {
    try {
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if(!assets.exists("DATA/GLOBAL.RSC")){std::cout<<"Imported crash corpus absent\n";return 0;}
        const auto exe=assets.read("ROADRASH.EXE");const auto config=rr::read_crash_config(exe);const rr::ActorAnimationLibrary library(exe,assets.read("DATA/GLOBAL.RSC"));
        rr::CrashActor initialized;rr::init_crash_actor(initialized,config);if(initialized.motion.i32(0xc4)!=8)throw std::runtime_error("Imported rider behavior type");
        std::ifstream file(argc>1?argv[1]:RR_SOURCE_DIR "/analysis/oracle/napa-crash-entry-fixtures.tsv");if(!file){std::cout<<"Original crash fixture absent\n";return 0;}
        std::map<std::string,int64_t> values;std::string line,key;int64_t value;while(std::getline(file,line)){std::istringstream row(line);if(row>>key>>value)values[key]=value;}
        std::ifstream walking_file(RR_SOURCE_DIR "/analysis/oracle/napa-crash-walking.tsv");
        if(walking_file) {
            std::map<std::string,int64_t> walking;while(std::getline(walking_file,line)){std::istringstream row(line);if(row>>key>>value)walking[key]=value;}
            if(walking.at("crash.walk.count")!=256)throw std::runtime_error("Incomplete original walking fixture");
            const int bike_offsets[]={0x1c,0x28,0x2f0,0x2f4},rider_offsets[]={0x1c,0x28,0x5c,0xec,0x128};
            for(int c=0;c<256;++c) {
                const auto root="crash.walk."+std::to_string(c)+".";rr::Bike bike;rr::CrashActor rider;rider.animation.animation=38;rider.animation.deadline=UINT32_MAX;
                auto key_for=[&](const char* label,const char* who,int offset){std::ostringstream hex;hex<<std::hex<<offset;return root+label+who+hex.str();};
                for(int offset:bike_offsets)bike.i32(offset)=int32_t(walking.at(key_for("input.","bike.",offset)));
                for(int offset:rider_offsets)rider.motion.i32(offset)=int32_t(walking.at(key_for("input.","rider.",offset)));
                rr::CrashRecoveryInput input;input.player=true;input.walk_controls=uint32_t(walking.at(root+"controls"));input.state_elapsed=int32_t(walking.at(root+"elapsed"));
                rr::recover_crash_actor(bike,rider,library.playback[0],library.fallback,100,input);
                for(int a=0;a<2;++a)for(int i=0;i<(a?5:4);++i){const int offset=(a?rider_offsets:bike_offsets)[i];const auto key=key_for("output.",a?"rider.":"bike.",offset);const auto actual=(a?rider.motion:bike).i32(offset);if(actual!=walking.at(key))throw std::runtime_error(key+" walking native="+std::to_string(actual)+" original="+std::to_string(walking.at(key)));}
                if(input.state_elapsed!=walking.at(root+"output.elapsed") || rider.animation.animation!=walking.at(root+"animation") || rider.animation.queued!=walking.at(root+"queued"))throw std::runtime_error(root+"walking state mismatch");
            }
            std::cout<<"256 original player walking decisions match\n";
        }
        const int remount_offsets[]={0x1c,0x20,0x24,0x28,0x2c,0x30,0x34,0x38,0x3c,0x40,0x58,0x5c,0x60,0xc0,0xec,0xf0,0xf4,0x104,0x108,0x10c,0x110,0x114,0x118,0x134,0x138,0x13c,0x140,0x154,0x174,0x178,0x17c,0x244,0x288,0x2f4};
        for(int c=0;c<values.at("crash.remount.count");++c) {
            const auto root="crash.remount."+std::to_string(c)+".";rr::Bike bike;rr::CrashActor rider;rr::ActorAnimationState bike_animation;
            auto key_for=[&](const char* label,const char* actor,int offset){std::ostringstream hex;hex<<std::hex<<offset;return root+label+actor+hex.str();};
            for(int offset:remount_offsets){bike.i32(offset)=int32_t(values.at(key_for("input.","bike.",offset)));rider.motion.i32(offset)=int32_t(values.at(key_for("input.","rider.",offset)));}
            for(auto state:{&bike_animation,&rider.animation}){state->animation=41;state->loops=uint8_t(values.at(root+"input.loops"));state->deadline=UINT32_MAX;}
            rr::remount_crash_actor(bike,bike_animation,rider,config,library.playback[0],library.fallback,uint32_t(values.at(root+"tick")));
            for(int actor=0;actor<2;++actor) {
                for(int offset:remount_offsets){const auto key=key_for("output.",actor?"rider.":"bike.",offset);const auto actual=(actor?rider.motion:bike).i32(offset);if(actual!=values.at(key))throw std::runtime_error(key+" remount native="+std::to_string(actual)+" original="+std::to_string(values.at(key)));}
                const auto& state=actor?rider.animation:bike_animation;const auto key=root+(actor?"rider.":"bike.");
                if(state.animation!=values.at(key+"animation") || state.frame!=values.at(key+"frame") || state.queued!=values.at(key+"queued") || state.loops!=values.at(key+"loops") || state.deadline!=values.at(key+"deadline"))throw std::runtime_error(key+"remount animation mismatch");
            }
        }
        std::cout<<values.at("crash.remount.count")<<" original local remount resets match\n";
        const int recovery_bike[]={0x1c,0x28,0x2ec,0x2f0,0x2f4},recovery_rider[]={0x1c,0x28,0x5c,0xec,0x128};
        for(int c=0;c<values.at("crash.recovery.count");++c) {
            const auto root="crash.recovery."+std::to_string(c)+".";rr::Bike bike;rr::CrashActor rider;
            auto key_for=[&](const char* label,const char* actor,int offset){std::ostringstream hex;hex<<std::hex<<offset;return root+label+actor+hex.str();};
            for(int offset:recovery_bike)bike.i32(offset)=int32_t(values.at(key_for("input.","bike.",offset)));
            for(int offset:recovery_rider)rider.motion.i32(offset)=int32_t(values.at(key_for("input.","rider.",offset)));
            rider.animation.animation=int8_t(values.at(root+"input.animation"));rider.animation.loops=uint8_t(values.at(root+"input.loops"));rider.animation.deadline=UINT32_MAX;
            rr::CrashRecoveryInput input;input.state_elapsed=int32_t(values.at(root+"elapsed"));
            rr::recover_crash_actor(bike,rider,library.playback[0],library.fallback,uint32_t(values.at(root+"tick")),input);
            for(int actor=0;actor<2;++actor) {
                const auto offsets=actor?recovery_rider:recovery_bike;
                for(int i=0;i<5;++i){const auto key=key_for("output.",actor?"rider.":"bike.",offsets[i]);const auto actual=(actor?rider.motion:bike).i32(offsets[i]);if(actual!=values.at(key))throw std::runtime_error(key+" native="+std::to_string(actual)+" original="+std::to_string(values.at(key)));}
            }
            const auto& state=rider.animation;
            if(state.animation!=values.at(root+"animation") || state.frame!=values.at(root+"frame") || state.queued!=values.at(root+"queued") || state.loops!=values.at(root+"loops") || state.deadline!=values.at(root+"deadline"))throw std::runtime_error(root+"recovery animation mismatch");
        }
        std::cout<<values.at("crash.recovery.count")<<" original offline recovery decisions match\n";
        const auto tables=rr::read_physics_tables(exe);const auto course=rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
        const auto specs=rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        rr::Race race(rr::make_route(course,false),rr::make_route(course,true),tables,specs.at(0),90);
        race.set_crash_library(library,config);
        auto& live_bike=const_cast<rr::Bike&>(race.bike());live_bike.i32(0xec)=6000;live_bike.i32(0x2f4)=1;
        race.step(0);
        if(!race.riders()[0].crash_active || !race.riders()[0].crash_actor.animation.selected || !race.riders()[0].crash_bike_animation.selected)throw std::runtime_error("Live crash did not start independent actors");
        bool separated=false;int elapsed=0;
        while(race.crashed() && elapsed++<4000) {
            race.step(0);const auto& rider=race.riders()[0];
            separated|=rider.bike.i32(0x1c)!=rider.crash_actor.motion.i32(0x1c);
        }
        if(!separated || race.crashed() || race.riders()[0].crash_active)throw std::runtime_error("Live crash did not separate and remount: elapsed="+std::to_string(elapsed)+" state="+std::to_string(race.bike().i32(0x2f4))+" speed="+std::to_string(race.bike().i32(0xec)));
        if(race.bike().i32(0x154)!=8 || race.bike().i32(0x174)!=config.normal_dimensions[0])throw std::runtime_error("Live remount did not restore bike dimensions");
        std::cout<<"Live Napa crash separated and remounted after "<<elapsed<<" ticks\n";
        const int motion_offsets[]={0x18,0x1c,0x20,0x24,0x28,0x2c,0x30,0x34,0x38,0x3c,0x40,0xec,0xf0,0xf4,0x108,0x10c,0x128,0x12c,0x130,0x134,0x138,0x13c,0x140,0x184,0x188,0x18c,0x19c};
        for(int c=0;c<values.at("actor.motion.count");++c) {
            const auto root="actor.motion."+std::to_string(c)+".";rr::Bike actor;
            for(int offset:motion_offsets){std::ostringstream hex;hex<<std::hex<<offset;actor.i32(offset)=int32_t(values.at(root+"input."+hex.str()));}
            actor.accel=int32_t(values.at(root+"accel"));
            rr::advance_actor_motion(actor,tables,[&](int32_t delta) {
                if(!delta){actor.i32(0x130)=actor.i32(0x12c)=0;return;}
                const int32_t old=actor.i32(0x1c);actor.i32(0x134)=old;actor.i32(0x1c)+=delta;actor.i32(0x138)=actor.i32(0x1c);
                if((uint32_t(old)^uint32_t(actor.i32(0x1c)))&0xffffff00u)actor.i32(0x13c)=actor.i32(0x140);
                actor.i32(0x130)=actor.i32(0x12c)=0;
            });
            for(int offset:motion_offsets) {
                std::ostringstream hex;hex<<std::hex<<offset;const auto key=root+"output."+hex.str();
                if(actor.i32(offset)!=values.at(key))throw std::runtime_error(key+" motion mismatch native="+std::to_string(actor.i32(offset))+" original="+std::to_string(values.at(key)));
            }
        }
        std::cout<<values.at("actor.motion.count")<<" original local actor motion cases match\n";
        const int offsets[]={0x1c,0x20,0x24,0x28,0x2c,0x30,0x34,0x38,0x3c,0x40,0x58,0x5c,0x60,0xbc,0xc0,0xec,0xf0,0xf4,0x104,0x108,0x10c,0x110,0x114,0x118,0x11c,0x120,0x124,0x128,0x12c,0x130,0x134,0x138,0x13c,0x140,0x154,0x158,0x160,0x164,0x16c,0x174,0x178,0x17c,0x198,0x19c,0x1a0,0x244,0x288,0x2f4,0x2f8};
        for(int c=0;c<values.at("crash.entry.count");++c) {
            const auto root="crash.entry."+std::to_string(c)+".";auto get=[&](const std::string& key){return values.at(root+key);};rr::Bike bike;rr::CrashActor rider;rider.animation.animation=2;rider.animation.deadline=UINT32_MAX;
            for(int offset:offsets) {
                std::ostringstream hex;hex<<std::hex<<offset;bike.i32(offset)=int32_t(get("input.bike."+hex.str()));rider.motion.i32(offset)=int32_t(get("input.rider."+hex.str()));
            }
            std::optional<std::array<int32_t,3>> rider_impulse,bike_impulse;
            if(get("rider_impulse")){rider_impulse.emplace();for(int i=0;i<3;++i)rider_impulse->at(i)=int32_t(get("rider_impulse."+std::to_string(i)));}
            if(get("bike_impulse")){bike_impulse.emplace();for(int i=0;i<3;++i)bike_impulse->at(i)=int32_t(get("bike_impulse."+std::to_string(i)));}
            const bool accepted=rr::begin_crash(bike,rider,config,library.playback[0],library.fallback,uint32_t(get("tick")),int32_t(get("reason")),rider_impulse,bike_impulse);
            if(accepted!=(get("input.bike.2f4")==0))throw std::runtime_error(root+"entry guard mismatch");
            for(int offset:offsets) {
                std::ostringstream hex;hex<<std::hex<<offset;
                for(int actor=0;actor<2;++actor) {
                    const std::string key="output."+std::string(actor?"rider.":"bike.")+hex.str();const int32_t actual=(actor?rider.motion:bike).i32(offset);
                    if(actual!=get(key))throw std::runtime_error(root+key+" native="+std::to_string(actual)+" original="+std::to_string(get(key)));
                }
            }
            const auto& state=rider.animation;if(state.animation!=get("animation") || state.frame!=get("frame") || state.queued!=get("queued") || state.loops!=get("loops") || state.deadline!=get("deadline"))throw std::runtime_error(root+"entry animation mismatch");
        }
        std::cout<<values.at("crash.entry.count")<<" original local crash entries match across 49 motion fields per actor\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
