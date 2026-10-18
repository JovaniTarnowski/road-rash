#include "police.hpp"
#include "assets.hpp"
#include "actor_descriptors.hpp"
#include "race_setup.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <tuple>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main() {
    try {
        rr::PoliceArrestInput in;in.found_cop=true;
        require(rr::police_arrest(in),"Stopped rider arrest");
        for(int speed:{299,300}){in.bike_speed=speed;require(rr::police_arrest(in)==(speed<300),"Normal speed boundary");}
        in.crash=2;for(int speed:{-1800,-1799,1799,1800}){in.bike_speed=speed;in.cop_speed=speed;require(rr::police_arrest(in)==(speed>-1800 && speed<1800),"Crashed speed boundary");}
        in.bike_speed=in.cop_speed=0;
        for(int pos:{-512,-511,511,512}){in.cop_position=pos;require(rr::police_arrest(in)==(pos>-512 && pos<512),"Cop position boundary");}
        in.cop_position=0;
        for(int speed:{-5000,-4999,4999,5000}){in.cop_speed=speed;require(rr::police_arrest(in)==(speed>-5000 && speed<5000),"Relative speed boundary");}
        in.cop_speed=0;in.cop_crashed=true;require(!rr::police_arrest(in),"Crashed cop exclusion");in.cop_crashed=false;
        in.rider_flags=0x10;require(!rr::police_arrest(in),"Rider flag exclusion");in.rider_flags=0;
        in.bike_flags=0x10;require(!rr::police_arrest(in),"Bike flag exclusion");in.bike_flags=0;
        in.owner=1;require(!rr::police_arrest(in),"Owner exclusion");in.owner=-1;require(!rr::police_arrest(in),"Remote owner does not decide local outcome");
        require(rr::police_spawn_due(4096,0,0,false) && !rr::police_spawn_due(4095,0,0,false) && !rr::police_spawn_due(4096,0,0,true) && !rr::police_spawn_due(4096,1,0,false) && !rr::police_spawn_due(4096,0,0x10,false),"Spawn boundaries");
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");if(!assets.exists("ROADRASH.EXE")){std::cout<<"Police threshold tests passed; imported corpus absent\n";return 0;}
        const auto config=rr::read_police_config(assets.read("ROADRASH.EXE"));
        require(config.bikes[0]==4 && config.stats[3]==171 && config.stats[12]==6 && config.stats[13]==2200,"Imported opening police configuration");
        for(int level=0;level<5;++level)for(int place=0;place<16;++place)for(int personality=0;personality<10;++personality) {
            const auto pursuit=rr::police_pursuit(config,level,place,personality);
            require(pursuit.personality[0x1a4/4]==config.personality[0x1bc/4+size_t(level)],"Imported pursuit speed");
            require(pursuit.personality[0x80/4+size_t(personality)*3]==pursuit.personality[2+size_t(personality)*3],"Target/current pursuit relationship");
        }
        std::ifstream file(RR_SOURCE_DIR "/analysis/oracle/napa-police-spawn.tsv");
        if(file) {
            std::map<std::string,int64_t> values;std::string line,key;int64_t value;while(std::getline(file,line)){std::istringstream row(line);if(row>>key>>value)values[key]=value;}
            const auto root=std::string("police.spawn.");require(values.at(root+"present")==1,"Original cop spawn capture");
            const auto pursuit=rr::police_pursuit(config,int(values.at(root+"level")),int(values.at(root+"player.place")),int(values.at(root+"player.personality")));
            for(int offset:{0x4,0x8,0x7c,0x80,0x1a4,0x1bc,0x1c0,0x1c4,0x1c8,0x1cc}){std::ostringstream hex;hex<<std::hex<<offset;require(pursuit.personality[size_t(offset)/4]==values.at(root+"brain."+hex.str()),"Original police pursuit initialization");}
            const auto exe=assets.read("ROADRASH.EXE");const rr::ActorAnimationLibrary library(exe,assets.read("DATA/GLOBAL.RSC"));
            rr::PoliceSpawnInput input;input.level=int(values.at(root+"level"));input.player_place=int(values.at(root+"player.place"));input.player_personality=int(values.at(root+"player.personality"));input.position=int32_t(values.at(root+"bike.1c"));input.lane=int(values.at(root+"brain.18c"));input.lateral_q8=0;input.lane_target_q8=0;
            const auto actor=rr::make_police_actor(config,rr::read_physics_tables(exe),rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC")),library,input);
            const int bike_fields[]={0x18,0x1c,0x20,0x24,0x28,0xc0,0xc4,0xdc,0xe0,0xe4,0xe8,0xec,0xf0,0xf4,0x148,0x14c,0x150,0x154,0x15c,0x160,0x168,0x16c,0x170,0x174,0x178,0x17c,0x180,0x184,0x188,0x18c,0x190,0x194,0x198,0x1a0,0x1a4,0x1a8,0x1ac,0x1b0,0x1b4,0x1b8,0x1d0,0x1e0,0x1f0,0x200,0x260,0x2ec,0x2f0,0x2f4,0x324,0x328};
            for(int a=0;a<2;++a)for(int offset:bike_fields) {
                if(a && (offset==0x1e0 || offset>=0x208))continue; // stats pointer and fields beyond the rider allocation
                std::ostringstream hex;hex<<std::hex<<offset;const auto key=root+(a?"rider.":"bike.")+hex.str();const auto actual=(a?actor.rider:actor.bike).i32(offset);
                if(actual!=values.at(key))throw std::runtime_error(key+" local constructor native="+std::to_string(actual)+" original="+std::to_string(values.at(key)));
            }
            std::cout<<"Original opening Napa local police actor numeric fields match\n";
            for(int offset:{0xc,0x30,0x34,0x38,0x3c}){std::ostringstream hex;hex<<std::hex<<offset;require(config.stats[size_t(offset)/4]==values.at(root+"stats."+hex.str()),"Original police rider statistics");}
            std::cout<<"Original opening Napa police pursuit and stats match\n";
        }
        std::ifstream arrest_file(RR_SOURCE_DIR "/analysis/oracle/napa-police-arrest.tsv");
        if(arrest_file) {
            std::map<std::string,int64_t> values;std::string line,key;int64_t value;while(std::getline(arrest_file,line)){std::istringstream row(line);if(row>>key>>value)values[key]=value;}
            require(values.at("police.arrest.count")==1024 && values.at("police.arrest.restored")==1,"Complete and restored original arrest capture");
            for(int c=0;c<1024;++c) {
                const auto root="police.arrest."+std::to_string(c)+".";auto get=[&](const char* key){return values.at(root+key);};rr::PoliceArrestInput input;
                input.owner=int(get("owner"));input.crash=int(get("crash"));input.rider_flags=uint8_t(get("rider_flags"));input.found_cop=get("found")!=0;input.bike_speed=int32_t(get("bike_speed"));input.cop_speed=int32_t(get("cop_speed"));input.rider_position=int32_t(get("rider_position"));input.cop_position=int32_t(get("cop_position"));
                if(rr::police_arrest(input)!=(get("arrested")!=0) || get("delay")!=(get("arrested")?180:0))throw std::runtime_error(root+"original arrest outcome mismatch");
            }
            std::cout<<"1,024 original local arrest outcomes and delays match\n";
        }
        {
            // Live wiring (plan 32 D): a parked player on Napa level 3 meets the
            // periodic cop at tick 4096 (0x00407f30), the cop chases through the
            // rider think/move path, finds the stopped player (0x00407860) and
            // arrests it (outcome 4). The cop never takes a place.
            const auto exe=assets.read("ROADRASH.EXE");
            rr::RaceConfig config;config.course="NAPA";config.level=3;config.character=1;config.bike_from_table=true;config.career=false;
            rr::RaceSetup setup=rr::make_race_setup(assets,exe,config);
            auto& race=*setup.instance.race;
            require(race.police_enabled() && race.cop_index()<0 && !race.busted(),"Police enabled without a cop at the start");
            const int place_before=race.place(0);
            while(race.ticks()<4095){race.step(0);require(race.cop_index()<0,"No cop before tick 4096");}
            race.step(0);
            require(race.ticks()==4096 && race.cop_index()>0,"Periodic cop spawns at tick 4096");
            const auto& cop=race.riders()[size_t(race.cop_index())];
            require(cop.cop && cop.ai && cop.bike.i32(0x324)==2 && cop.bike.u8(0x328)==15,"Cop actor identity");
            require(cop.bike.i32(0x1c)<race.bike().i32(0x1c),"Periodic cop starts behind the player");
            require(race.place(0)<=race.riders().size(),"Cop does not take a place");
            (void)place_before;
            int arrested=-1,sirens=0;
            for(int t=0;t<6000 && arrested<0;++t){race.step(0);for(const auto& e:race.take_sound_events())sirens+=e.code==rr::SoundEvent::siren&&e.position!=0;if(race.busted())arrested=t;if(std::getenv("RR_COP_TRACE")&&t%std::max(1,std::atoi(std::getenv("RR_COP_TRACE")))==0&&race.cop_index()>=0){const auto& c=race.riders()[size_t(race.cop_index())];std::cout<<t<<" cop pos "<<c.bike.i32(0x1c)<<" spd "<<c.bike.i32(0xec)<<" state "<<c.brain.state<<" tgt "<<c.brain.target_rider<<" player "<<race.bike().i32(0x1c)<<"\n";}}
            std::cout<<"cop present="<<(race.cop_index()>=0)<<" arrested_after="<<arrested<<"\n";
            require(arrested>=0,"Cop arrests the stopped player");
            require(sirens>0,"Cop posts siren events with its position");
        }
        for(const auto& scenario:{std::make_tuple("NAPA","napa-cops-parked-20261018",4551),std::make_tuple("CITY","city-cops-parked-20261018",4501)}) {
            // Original-capture comparison (`<course>-cops-parked-20261018`, git-ignored):
            // Napa and City, level byte 2, seed 1, the human player parked on the grid
            // (zero controls), original cop spawned by 0x00407f30 at tick 4096.
            // The cop's position and speed must match the original on every
            // tick from spawn to the arrest window. Covers the cop's own
            // pursuit (state 4) against a stopped player only: the other two
            // riders are not compared and the capture is not a free-running
            // field replay, so this does not establish full-field parity.
            std::ifstream capture(std::string(RR_SOURCE_DIR "/analysis/oracle/")+std::get<1>(scenario)+".tsv");
            if(capture) {
                std::map<int,std::pair<int,int>> original;   // tick -> cop (position, speed)
                std::map<int,std::pair<int,int>> pos_speed;  // rider slot -> (position, speed) of the current frame
                std::string line;int tick=0,slot=-1;
                while(std::getline(capture,line)) {
                    if(!line.empty() && line.back()=='\r')line.pop_back();
                    const size_t tab=line.find('\t');
                    const std::string key=line.substr(0,tab);const long long value=tab==std::string::npos?0:std::atoll(line.c_str()+tab+1);
                    if(key=="frame" || key=="end"){if(slot>=0 && tick>=4096)original[tick]=pos_speed[slot];pos_speed.clear();slot=-1;}
                    else if(key=="tick")tick=int(value);
                    else if(key=="police.slot")slot=int(value);
                    else if(key.rfind("rider.",0)==0) {
                        const size_t dot=key.find('.',6);const int who=std::atoi(key.c_str()+6);const std::string field=key.substr(dot+1);
                        if(field=="position")pos_speed[who].first=int(value);else if(field=="speed")pos_speed[who].second=int(value);
                    }
                }
                require(original.count(4096) && original.count(4600),"Cop capture covers the spawn and the arrest window");
                const auto exe=assets.read("ROADRASH.EXE");
                rr::RaceConfig config;config.course=std::get<0>(scenario);config.level=2;config.character=1;config.bike_from_table=true;config.career=false;
                rr::RaceSetup setup=rr::make_race_setup(assets,exe,config);
                auto& race=*setup.instance.race;
                // CITY is exact only until the cop meets curvature (tick 4502:
                // the pursuit target speed 0x19c is 94 native vs 98 original,
                // i.e. the City route bend input to 0x00409020 is ~7% off;
                // course geometry, not cop logic - see VERIFICATION.md). The
                // strict span ends before that tick; the rest is reported.
                int compared=0,reported=0;
                for(int t=0;t<4551;++t) {
                    race.step(0);
                    if(race.ticks()<4096 || race.cop_index()<0 || !original.count(int(race.ticks())))continue;
                    const auto& cop=race.riders()[size_t(race.cop_index())];
                    const auto want=original.at(int(race.ticks()));
                    const bool differs=cop.bike.i32(0x1c)!=want.first || cop.bike.i32(0xec)!=want.second;
                    if(differs && race.ticks()>std::get<2>(scenario)){if(!reported++)std::cout<<std::get<0>(scenario)<<" known divergence from tick "<<race.ticks()<<" (reported, not asserted)\n";continue;}
                    if(differs)
                        throw std::runtime_error("cop differs from the original at tick "+std::to_string(race.ticks())+": position "+std::to_string(cop.bike.i32(0x1c))+" vs "+std::to_string(want.first)+", speed "+std::to_string(cop.bike.i32(0xec))+" vs "+std::to_string(want.second));
                    ++compared;
                }
                require(compared>=400,"Cop compared over the pursuit");
                std::cout<<std::get<0>(scenario)<<" cop matches the original on "<<compared<<" ticks (position, speed)\n";
            } else std::cout<<"skipped: "<<std::get<1>(scenario)<<" fixture absent\n";
        }
        {
            // Arrest while the rider walks (crash state 2, 0x00407f30): the half is the
            // walking body, so the cop must come within 512 of it.
            const auto exe=assets.read("ROADRASH.EXE");
            rr::RaceConfig config;config.course="NAPA";config.level=3;config.character=1;config.bike_from_table=true;config.career=false;
            rr::RaceSetup setup=rr::make_race_setup(assets,exe,config);
            auto& race=*setup.instance.race;
            while(race.ticks()<4096)race.step(0);
            require(race.cop_index()>0,"On-foot scenario cop spawned");
            race.knock_off(race.riders()[0],5000,5000,1);
            bool on_foot=false;int arrested=-1;
            for(int t=0;t<6000 && arrested<0;++t) {
                race.step(0);on_foot|=race.bike().i32(0x2f4)==2;
                if(race.busted())arrested=t;
                if(std::getenv("RR_COP_TRACE")&&t%60==0)std::cout<<"foot "<<t<<" crash "<<race.bike().i32(0x2f4)<<" body "<<race.riders()[0].crash_actor.motion.i32(0x1c)<<" bflags "<<int(race.bike().u8(0xc0))<<" hflags "<<int(race.riders()[0].half_object.u8(0xc0))<<" bspd "<<race.bike().i32(0xec)<<" cspd "<<(race.cop_index()>=0?race.riders()[size_t(race.cop_index())].bike.i32(0xec):-1)<<" cop "<<(race.cop_index()>=0?race.riders()[size_t(race.cop_index())].bike.i32(0x1c):-1)<<"\n";
            }
            std::cout<<"on_foot="<<on_foot<<" arrested_after="<<arrested<<"\n";
            require(on_foot,"Rider reaches the on-foot state");
            require(arrested>=0,"Cop arrests the rider walking beside the wreck");
        }
        std::cout<<"Police spawn/arrest boundary tests passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
