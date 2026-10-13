#include <cstdlib>
#include "pedestrians.hpp"
#include "traffic.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <map>

// Pedestrians (plan 24 Task 4) on the imported CITY course: DZHR actor
// records, spawning through the ring, crossing, and both hit outcomes of
// 0x00440760. Skips without the disc corpus.
namespace {
struct Fixture {
    rr::Assets assets{RR_SOURCE_DIR "/../cdrom/ROADRASH"};
    std::vector<uint8_t> exe;
    rr::PhysicsTables physics;
    std::vector<rr::BikeSpec> specs;
    rr::DepthTable depths;
    rr::Course course;
    std::unique_ptr<rr::CourseScene> scene;
    std::unique_ptr<rr::VehicleLibrary> vehicles;
    explicit Fixture(const std::string& name) {
        exe=assets.read("ROADRASH.EXE");physics=rr::read_physics_tables(exe);depths=rr::read_depth_table(exe);
        specs=rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        course=rr::read_course(assets.read("DATA/COURSES/"+name+".CRS"));
        scene=std::make_unique<rr::CourseScene>(assets,exe,course,name,1,0);
        vehicles=std::make_unique<rr::VehicleLibrary>(assets.read("DATA/CARS/"+name+".CAR"),exe);
    }
    std::unique_ptr<rr::PedestrianSystem> pedestrians() const {
        return std::make_unique<rr::PedestrianSystem>(
            [this](const rr::RouteSample& s)->const std::vector<rr::DzhrActor>&{return scene->dzhr_actors(s.section,s.local);},
            [this](const rr::RouteSample& s,uint8_t selector){return scene->pedestrian_look(s.section,s.local,selector);},exe);
    }
    std::unique_ptr<rr::Race> race(int opponents) const {
        auto r=std::make_unique<rr::Race>(rr::make_route(course,false),rr::make_route(course,true),physics,specs.at(0),90,
            rr::original_opponents(exe,specs,0,opponents),0,0,rr::read_personalities(exe),exe);   // personality speed tables (replan resets from them)
        r->set_terrain_source([this](const rr::RouteSample& s)->const rr::TerrainSlice&{return scene->terrain(s.section,s.local);});
        return r;
    }
};
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}

struct Summary {int spawned=0,max_active=0,crossers=0,knocked=0;int64_t travel=0;uint32_t rng=0;};
Summary natural_race(const Fixture& f,const char* frame_path) {
    auto race=f.race(7);auto pedestrians=f.pedestrians();
    rr::TrafficSystem traffic(*f.vehicles,f.exe);traffic.configure(6);
    race->set_traffic(&traffic);race->set_pedestrians(pedestrians.get());
    race->set_demo_hold(true,95,0,0,2,{7,3000,3000,3000});   // the player drives as the AI does
    Summary out;
    std::map<uint16_t,int32_t> start_lateral;std::map<uint16_t,int32_t> furthest;
    bool saved=false;
    // Pre-green countdown: the AI thinks (replan, first targets) before the
    // race starts, as in the original (the solo captures hold 59 ticks).
    for(int tick=0;tick<59;++tick)race->countdown_step();
    for(int tick=0;tick<12000 && !race->finished();++tick) {
        race->step(0);
        if(std::getenv("RR_PED_DEBUG") && tick%600==0){const auto& b=race->riders()[0].brain;std::cerr<<"tick "<<tick<<" pos "<<race->riders()[0].bike.i32(0x1c)<<" speed "<<race->riders()[0].bike.i32(0xec)<<" state "<<b.state<<" tspd "<<b.target_speed<<" tgt "<<b.target_rider<<" latched "<<b.overtake_latched<<"\n";}
        int active=0;
        for(const auto& p:pedestrians->pool()) {
            if(!p.active)continue;
            ++active;
            const int32_t lateral=p.body.bike.i32(0x28);
            if(!start_lateral.count(p.serial))start_lateral[p.serial]=lateral;
            furthest[p.serial]=std::max(furthest[p.serial],std::abs(lateral-start_lateral[p.serial]));
            if(p.state==2 && p.look)++out.knocked;
            const int32_t ahead=p.body.bike.i32(0x1c)-int32_t(race->camera_position());
            if(frame_path && !saved && p.state==1 && p.look && ahead>6*256 && ahead<14*256) {
                saved=true;
                rr::Framebuffer fb(640,480);f.scene->draw_horizon(fb,race->horizon_scroll());
                const auto& route=race->route();const auto camera=race->camera();
                const auto slices=rr::project_road(route,race->camera_position(),camera,f.depths);
                auto actors=[&](const rr::RoadSlice& near,const rr::RoadSlice& far,int road,int32_t ceiling,bool back) {
                    std::vector<rr::DrawNode> nodes;
                    if(road!=near.own)return nodes;
                    for(const auto& q:pedestrians->pool()) {
                        const uint32_t pos=uint32_t(q.body.bike.i32(0x1c));
                        if(!q.active || (pos>>8)!=near.sample || ((pos&255)>=128)!=back)continue;
                        if(auto node=rr::pedestrian_node(q,near,far,road,route.at(near.sample),camera,f.depths,ceiling))nodes.push_back(*node);
                    }
                    return nodes;
                };
                rr::render_nodes(fb,f.scene->draw_nodes(route,slices,camera,f.depths,actors),rr::view_mode(true),16);
                std::vector<uint8_t> rgba(640*480*4);fb.to_rgba(rr::Palette::from_raw(f.assets.read("DATA/PALETTE.RAW")),rgba.data(),640*4);
                SDL_Surface* surface=SDL_CreateSurfaceFrom(640,480,SDL_PIXELFORMAT_RGBA32,rgba.data(),640*4);
                if(!surface || !SDL_SaveBMP(surface,frame_path))throw std::runtime_error(SDL_GetError());
                SDL_DestroySurface(surface);
            }
        }
        check(active<=int(rr::PedestrianSystem::pool_size),"pedestrian pool exceeds 15");
        out.max_active=std::max(out.max_active,active);
    }
    out.spawned=pedestrians->spawned();
    for(const auto& [serial,distance]:furthest)if(distance>250*256){++out.crossers;out.travel+=distance;}
    out.rng=race->rng_state();
    return out;
}

// A pedestrian staged just ahead of the player in its lane, kept still.
rr::Pedestrian& stage(const Fixture& f,rr::Race& race,rr::PedestrianSystem& system) {
    const auto& route=race.route();
    const rr::PedestrianLook* look=nullptr;
    for(size_t s=0;s<route.size() && !look;++s)for(const auto& record:f.scene->dzhr_actors(route[s].section,route[s].local))
        if((record.word0&15)<0xb){look=f.scene->pedestrian_look(route[s].section,route[s].local,uint8_t(record.word0>>8));if(look)break;}
    check(look!=nullptr,"no CITY pedestrian look");
    auto& p=system.pool()[0];
    p=rr::Pedestrian{};p.active=true;p.look=look;p.type=0;p.state=0;
    system.apply_template(p.body.bike);
    auto& rider=race.riders()[0];auto& q=p.body.bike;
    q.u8(0xc0)|=0x10;
    q.i32(0x1c)=rider.bike.i32(0x1c)+0x30;q.i32(0x28)=rider.bike.i32(0x28);
    for(int k=0;k<5;++k)q.i32(0x30+4*k)=q.i32(0x1c+4*k);
    p.body.route=rider.route;p.body.sample=p.body.previous_sample=size_t(q.i32(0x1c))>>8;p.anchor=q.i32(0x1c);
    p.next_tick=1<<30;   // no think: it stands where it was put
    rr::set_animation(p.animation,look->bank,1,0);
    return p;
}
void staged_hit(const Fixture& f,int32_t speed,int expected_state,bool bump) {
    auto race=f.race(0);auto system=f.pedestrians();race->set_pedestrians(system.get());
    // Fill the ring first so staged slot 0 is not reused by a natural spawn.
    race->step(0);
    for(auto& p:system->pool())p.active=false;
    auto& rider=race->riders()[0];
    auto& p=stage(f,*race,*system);
    rider.bike.i32(0xec)=speed;rider.bike.i32(0x108)=speed;
    const int32_t height=rider.bike.i32(0x20);
    bool hit=false;int32_t bumped=0;
    for(int t=0;t<120 && !hit;++t) {
        race->step(0);
        if(p.state!=0){hit=true;bumped=rider.bike.i32(0x20)-height;}
    }
    check(hit,"staged pedestrian was never hit");
    if(p.state!=expected_state)std::cerr<<"speed "<<speed<<" state "<<p.state<<" anim "<<int(p.animation.animation)<<" rider speed now "<<rider.bike.i32(0xec)<<" bump "<<bumped<<"\n";
    check(p.state==expected_state,"pedestrian hit state");
    if(expected_state==2)check(p.animation.animation==3,"knocked pedestrian plays animation 3");
    check(rider.bike.i32(0x2f4)==0,"a pedestrian never knocks the rider off");
    (void)bump;(void)bumped;
}
}

int main(int argc,char** argv) {
    try {
        rr::Assets probe(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if(!probe.exists("DATA/COURSES/CITY.CRS")){std::cout<<"No imported course corpus\n";return 0;}
        if(!SDL_Init(0))std::cerr<<SDL_GetError()<<'\n';
        Fixture city("CITY");
        // DZHR actor records survive placement (0x00451fc0 gate, level 1).
        size_t pedestrians=0,cops=0,looks=0;
        for(size_t s=0;s<city.course.sections.size();++s)for(size_t j=0;j<city.course.sections[s].samples.size();++j)
            for(const auto& record:city.scene->dzhr_actors(uint32_t(s),uint32_t(j))) {
                if((record.word0&15)<0xb){++pedestrians;looks+=city.scene->pedestrian_look(uint32_t(s),uint32_t(j),uint8_t(record.word0>>8))!=nullptr;}
                else ++cops;
            }
        std::cout<<"CITY level 1: "<<pedestrians<<" pedestrian and "<<cops<<" police records, "<<looks<<" with artwork\n";
        check(pedestrians>0 && looks>0,"CITY has no placed pedestrians");
        // Plan 24 Task 5 pedestrian counterpart: every course keeps DZHR
        // actor records through the level-1 gate wherever they occur.
        for(const char* other:{"HIWAY","MEDLY","NAPA","CANYN"}) {
            Fixture f(other);
            size_t op=0,oc=0;
            for(size_t s=0;s<f.course.sections.size();++s)for(size_t j=0;j<f.course.sections[s].samples.size();++j)
                for(const auto& record:f.scene->dzhr_actors(uint32_t(s),uint32_t(j)))
                    ((record.word0&15)<0xb)?++op:++oc;
            std::cout<<other<<" level 1: "<<op<<" pedestrian and "<<oc<<" police records\n";
            if(op==0)throw std::runtime_error(std::string(other)+" has no placed pedestrians");
        }
        const auto a=natural_race(city,argc>1?argv[1]:nullptr);
        std::cout<<"natural race: "<<a.spawned<<" spawned, at most "<<a.max_active<<" active, "<<a.crossers
                 <<" crossed more than a lane, "<<a.knocked<<" pedestrian-ticks knocked down\n";
        check(a.spawned>0,"no pedestrian spawned");
        check(a.crossers>0,"no pedestrian crossed the road");
        const auto b=natural_race(city,nullptr);
        check(a.spawned==b.spawned && a.crossers==b.crossers && a.travel==b.travel && a.rng==b.rng,"pedestrians are not deterministic");
        // 0x00440760: a fast rider knocks the pedestrian down (state 2,
        // animation 3); a slow one only makes it stagger (state 3).
        staged_hit(city,12000,2,true);
        staged_hit(city,800,3,false);   // 0x00440760: knocked above |speed| 1000
        std::cout<<"staged hits: fast -> knocked down, slow -> stagger\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"pedestrian: "<<e.what()<<'\n';return 1;}
}
