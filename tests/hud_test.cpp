#include "hud.hpp"
#include <iostream>
int main(){
    try {
        if(rr::Hud::meter_level(0,2500)!=0 || rr::Hud::meter_level(2500,2500)!=31 || rr::Hud::meter_level(1250,2500)!=15 || rr::Hud::meter_level(100000,2500)!=31 || rr::Hud::meter_level(1,1)!=31)throw std::runtime_error("Meter levels");
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if(!assets.exists("ROADRASH.EXE")){std::cout<<"Meter levels pass; imported HUD corpus absent\n";return 0;}
        const auto exe=assets.read("ROADRASH.EXE");const auto physics=rr::read_physics_tables(exe);
        const auto course=rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
        const auto specs=rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        rr::Race race(rr::make_route(course,false),rr::make_route(course,true),physics,specs[0],90,{},0);
        // 11a Task 2: the tachometer needle angle follows bike +0x2dc.
        race.riders()[0].bike.i32(0x2dc) = 0x3000;
        const auto needle = rr::Hud(assets,exe,0,false).state(race,"Player","Sledge");
        if(needle.rpm_angle != 0x3000/44 - 1024 + 4096)throw std::runtime_error("RPM needle does not follow +0x2dc");
        size_t variants=0;
        for(bool metric:{false,true})for(int bike:{0,5,10}) {
            rr::Hud hud(assets,exe,bike,metric);auto state=hud.state(race,"Player","Sledge");
            if(state.place!=1 || state.stamina!=31 || state.condition!=31)throw std::runtime_error("Starting dashboard values");
            for(int value:{0,1,15,31}) {
                state.stamina=state.condition=state.target_stamina=value;
                const auto image=hud.compose(state,physics);
                if(image.width!=640 || image.height!=480 || image.at(0,0)!=0x01000000 || image.at(250,449)!=0x721108)throw std::runtime_error("Dashboard composition bounds/needle");
                ++variants;
            }
        }
        std::cout<<variants<<" imported dashboard variants pass\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
