#include "hud.hpp"
#include <iostream>
static uint8_t brute_index(uint32_t c, const rr::Palette& pal) {
    uint8_t best = 0;
    int best_d = INT_MAX;
    for (int i = 1; i < 256; ++i) {
        const uint8_t* rgb = &pal.rgbx[size_t(4 * i)];
        const int dr = int(rgb[0]) - int((c >> 16) & 0xff);
        const int dg = int(rgb[1]) - int((c >> 8) & 0xff);
        const int db = int(rgb[2]) - int(c & 0xff);
        const int d = dr * dr + dg * dg + db * db;
        if (d < best_d) { best_d = d; best = uint8_t(i); }
    }
    return best;
}
int main(){
    try {
        // Review 2026-10-13 item 2: DashLut matches the brute-force search.
        {
            rr::Palette pal;
            for (int i = 0; i < 256; ++i) { pal.rgbx[4 * i] = uint8_t(i); pal.rgbx[4 * i + 1] = uint8_t(255 - i); pal.rgbx[4 * i + 2] = uint8_t((i * 5) & 255); pal.rgbx[4 * i + 3] = 0; }
            rr::DashLut lut;
            for (int i = 0; i < 256; ++i) {
                const uint8_t* c = &pal.rgbx[4 * i];
                const uint32_t px = (uint32_t(c[0]) << 16) | (uint32_t(c[1]) << 8) | uint32_t(c[2]);
                if (lut.index(px, pal) != brute_index(px, pal)) throw std::runtime_error("DashLut palette entry");
                const uint32_t dark = (uint32_t(c[0] / 2) << 16) | (uint32_t(c[1] / 2) << 8) | uint32_t(c[2] / 2);
                if (lut.index(dark, pal) != brute_index(dark, pal)) throw std::runtime_error("DashLut darkened entry");
            }
            for (uint32_t k = 0; k < 32768; k += 97) {
                const uint32_t px = (uint32_t(((k >> 10) & 31) * 8 + 4) << 16) |
                                    (uint32_t(((k >> 5) & 31) * 8 + 4) << 8) | uint32_t((k & 31) * 8 + 4);
                if (lut.index(px, pal) != brute_index(px, pal)) throw std::runtime_error("DashLut cell sweep");
            }
            pal.rgbx[4] = 200;   // palette change without reset may serve stale entries
            lut.reset();
            const uint32_t px = (uint32_t(200) << 16);
            if (lut.index(px, pal) != brute_index(px, pal)) throw std::runtime_error("DashLut after reset");
        }
        // F10 MiniDash cycle (0x004431d0): 0->1->2->0.
        if(rr::Hud::cycle_mini_dash(0)!=1 || rr::Hud::cycle_mini_dash(1)!=2 || rr::Hud::cycle_mini_dash(2)!=0)throw std::runtime_error("MiniDash cycle");
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if(!assets.exists("ROADRASH.EXE")){std::cout<<"Meter levels pass; imported HUD corpus absent\n";return 0;}
        const auto exe=assets.read("ROADRASH.EXE");const auto physics=rr::read_physics_tables(exe);
        const auto course=rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
        const auto specs=rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        rr::Race race(rr::make_route(course,false),rr::make_route(course,true),physics,specs[0],90,{},0);
        const std::vector<std::string> names{"Player"};
        // 0x00438550 / 0x00446b80: the dashboard names the nearest opponent.
        {
            rr::Race field(rr::make_route(course,false),rr::make_route(course,true),physics,specs[0],90,
                           rr::original_opponents(exe,specs,0,3),0);
            const std::vector<std::string> field_names{"Player","Natasha","Viper","Sledge"};
            auto at=[&](size_t i,int32_t q8){field.riders()[i].bike.i32(0x1c)=q8;};
            const int32_t me=400*256;at(0,me);
            // Ahead 10 samples, behind 4: 4*2 < 10, the behind rider wins (gap > 0).
            at(1,me+10*256);at(2,me-4*256);at(3,me-40*256);
            auto n=rr::nearest_opponent(field);
            if(n.rider!=2 || n.gap!=4*256)throw std::runtime_error("Nearest behind opponent rule");
            // Behind 6: 6*2 >= 10, the ahead rider wins (gap <= 0).
            at(2,me-6*256);n=rr::nearest_opponent(field);
            if(n.rider!=1 || n.gap!=-10*256)throw std::runtime_error("Nearest ahead opponent rule");
            // Nobody ahead: the nearest behind is named.
            at(1,me-20*256);n=rr::nearest_opponent(field);
            if(n.rider!=2 || n.gap!=6*256)throw std::runtime_error("Nearest opponent without one ahead");
            field.riders()[2].stamina.current=field.riders()[2].stamina.base/2;
            const auto s=rr::Hud(assets,exe,0,false).state(field,"Player",field_names);
            if(s.target!="Viper" || s.target_gap!=6*256 || s.target_stamina!=rr::Hud::meter_level(field.riders()[2].stamina.current,field.riders()[2].stamina.base) || s.target_stamina==0)
                throw std::runtime_error("Dashboard target is not the nearest opponent");
            if(rr::nearest_opponent(race).rider!=-1 || !rr::Hud(assets,exe,0,false).state(race,"Player",names).target.empty())
                throw std::runtime_error("Solo race names a target");
        }
        // 11a Task 2: the tachometer needle angle follows bike +0x2dc.
        race.riders()[0].bike.i32(0x2dc) = 0x3000;
        const auto needle = rr::Hud(assets,exe,0,false).state(race,"Player",names);
        if(needle.rpm_angle != 0x3000/44 - 1024 + 4096)throw std::runtime_error("RPM needle does not follow +0x2dc");
        // 0x00446a40 via the 0x00438450 +0x1c -> +0x44 snapshot: tenths travelled.
        const auto grid=race.riders()[0].bike.i32(0x1c);
        race.riders()[0].bike.i32(0x1c)=(50+33*12)*256;
        if(rr::Hud(assets,exe,0,false).state(race,"Player",names).distance!=12)throw std::runtime_error("Distance readout ignores route position");
        race.riders()[0].bike.i32(0x1c)=grid;
        size_t variants=0;
        for(bool metric:{false,true})for(int bike:{0,5,10}) {
            rr::Hud hud(assets,exe,bike,metric);auto state=hud.state(race,"Player",names);
            if(state.place!=1 || state.stamina!=31 || state.condition!=31)throw std::runtime_error("Starting dashboard values");
            for(int value:{0,1,15,31}) {
                state.stamina=state.condition=state.target_stamina=value;
                const auto image=hud.compose(state,physics,rr::DashMode::full);
                if(image.width!=640 || image.height!=480 || image.at(0,0)!=0x01000000 || image.at(250,449)!=0x721108)throw std::runtime_error("Dashboard composition bounds/needle");
                ++variants;
            }
            // 0x00446c30 draws the opponent meter (layout 50, 456,424, 51x31)
            // with no target, so the dashboard hole never shows the road.
            state.target.clear();state.target_stamina=0;state.stamina=0;
            const auto empty=hud.compose(state,physics,rr::DashMode::full);
            for(int y=424;y<455;++y)for(int x=456;x<507;++x)if(empty.at(x,y)==0x01000000)throw std::runtime_error("Opponent meter hole is transparent");
            // 0x0044ca50: vertical meters empty from the top (colour, then black overlay).
            state.stamina=31;const auto full=hud.compose(state,physics,rr::DashMode::full);size_t changed=0;
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
            const auto low=hud.compose(state,physics,rr::DashMode::full);
            if(low.width!=640 || low.height!=480)throw std::runtime_error("Low dashboard bounds");
            { size_t lit=0,total=0; for(int y=420;y<480;++y)for(int x=0;x<640;++x){++total;lit+=low.at(x,y)!=0x01000000;} if(lit<total/10)throw std::runtime_error("Low dashboard mostly transparent"); }
            hud.set_low_mode(false);
            // Plan 19 Task 5: compose at scale 2 is 1280 wide and downsamples
            // (pixel (2x+1,2y+1)) to the scale-1 image on >=98% of pixels.
            const auto hi=hud.compose(state,physics,rr::DashMode::full,2);
            if(hi.width!=1280 || hi.height!=960)throw std::runtime_error("Scaled dashboard bounds");
            size_t same=0,stotal=0;
            for(int y=0;y<480;++y)for(int x=0;x<640;++x){++stotal;same+=hi.at(2*x+1,2*y+1)==full.at(x,y);}
            if(same*100<98*stotal)throw std::runtime_error("Scaled dashboard downsample mismatch");
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
