#include "police.hpp"
#include "assets.hpp"
#include <fstream>
#include <sstream>
#include <map>
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
        std::cout<<"Police spawn/arrest boundary tests passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
