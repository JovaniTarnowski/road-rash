#include "hud.hpp"
#include <iostream>
int main(){
    try {
        // F10 MiniDash cycle (0x004431d0): 0->1->2->0.
        if(rr::Hud::cycle_mini_dash(0)!=1 || rr::Hud::cycle_mini_dash(1)!=2 || rr::Hud::cycle_mini_dash(2)!=0)throw std::runtime_error("MiniDash cycle");
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
        // 0x00446a40 via the 0x00438450 +0x1c -> +0x44 snapshot: tenths travelled.
        const auto grid=race.riders()[0].bike.i32(0x1c);
        race.riders()[0].bike.i32(0x1c)=(50+33*12)*256;
        if(rr::Hud(assets,exe,0,false).state(race,"Player","").distance!=12)throw std::runtime_error("Distance readout ignores route position");
        race.riders()[0].bike.i32(0x1c)=grid;
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
            // 0x00446c30 draws the opponent meter (layout 50, 456,424, 51x31)
            // with no target, so the dashboard hole never shows the road.
            state.target.clear();state.target_stamina=0;state.stamina=0;
            const auto empty=hud.compose(state,physics);
            for(int y=424;y<455;++y)for(int x=456;x<507;++x)if(empty.at(x,y)==0x01000000)throw std::runtime_error("Opponent meter hole is transparent");
            // 0x0044ca50: vertical meters empty from the top (colour, then black overlay).
            state.stamina=31;const auto full=hud.compose(state,physics);size_t changed=0;
            for(int y=424;y<455;++y)for(int x=134;x<185;++x)changed+=full.at(x,y)!=empty.at(x,y);
            // MiniDash 1 (0x00446899/0x004468e1 skip tach/speedo; 0x00446c76 skips
            // name bars): numeric readout only, no 3x3 needle pixels at the dial.
            const auto mini=hud.compose(state,physics,rr::DashMode::mini);
            if(mini.width!=640 || mini.height!=480)throw std::runtime_error("Mini dashboard bounds");
            if(mini.at(250,449)==0x721108)throw std::runtime_error("Mini mode still draws needles");
            // MiniDash 2: 0x00446c30 draws nothing (fully transparent frame).
            const auto off=hud.compose(state,physics,rr::DashMode::off);
            for(int y=0;y<480;y+=7)for(int x=0;x<640;x+=11)if(off.at(x,y)!=0x01000000)throw std::runtime_error("Off mode is not empty");
            // Low-mode assets (0x004457f0 choices 1/2): 320-wide dashboard blits
            // through the same 640x480 frame without transparency holes.
            hud.set_low_mode(true);
            const auto low=hud.compose(state,physics);
            if(low.width!=640 || low.height!=480)throw std::runtime_error("Low dashboard bounds");
            { size_t lit=0,total=0; for(int y=420;y<480;++y)for(int x=0;x<640;++x){++total;lit+=low.at(x,y)!=0x01000000;} if(lit<total/10)throw std::runtime_error("Low dashboard mostly transparent"); }
            hud.set_low_mode(false);
        }
        std::cout<<variants<<" imported dashboard variants pass\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
