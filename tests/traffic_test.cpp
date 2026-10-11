#include "traffic.hpp"
#include "assets.hpp"
#include "course.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

// Synthetic tests always run; the CARS-container corpus part skips when the
// original disc is not mounted. Every rule is traced in src/traffic.cpp.

static int32_t div256(int32_t x){return (x + ((x >> 31) & 0xff)) >> 8;}

static rr::RouteSample lane_sample(int32_t left_q8,int32_t right_q8) {
    rr::RouteSample s;
    s.path.left_q8=left_q8;s.path.right_q8=right_q8;s.offset=1000;
    return s;
}

static void test_configure() {
    rr::TrafficTimers t;
    traffic_configure(t,-25);    // 0x0044dd00(-25): rate only
    if(t.rate!=2500)throw std::runtime_error("rate");
    traffic_configure(t,6);      // 0x0044dd00(6) + 0x0044db80 tail
    const int32_t reload=div256(2500*6*0x3c);
    if(t.oncoming_reload!=reload || t.other_reload!=reload)throw std::runtime_error("reload");
    if(t.oncoming!=reload || t.behind!=0 || t.ahead!=-reload)throw std::runtime_error("timer starts");
    traffic_configure(t,0);      // density 0 means 6 in the original
    if(t.density!=6)throw std::runtime_error("default density");
}

static void test_lane_count() {
    if(rr::traffic_lane_count(0)!=0)throw std::runtime_error("zero lanes");
    if(rr::traffic_lane_count(0x300)!=3)throw std::runtime_error("three lanes");
    if(rr::traffic_lane_count(0x2ff)!=2)throw std::runtime_error("fractional floor");
    if(rr::traffic_lane_count(0x100)!=1)throw std::runtime_error("one lane");
    if(rr::traffic_lane_count(-0x200)!=int8_t((-0x200+0xff)>>8))throw std::runtime_error("negative lane word");
    if(rr::traffic_lane_count(-0x100)!=int8_t((-0x100+0xff)>>8))throw std::runtime_error("negative single");
}

// One synthetic vehicle: a valid three-frame RRAN and a cel whose CCB flags
// select only the same-direction speed class, packed into a CRSR container.
static std::vector<uint8_t> synthetic_container(uint32_t cel_flags) {
    std::vector<uint8_t> anim;
    auto aw=[&](uint32_t v){for(int b=0;b<4;++b)anim.push_back(uint8_t(v>>(b*8)));};
    const uint32_t info_off=28+3*28,ccb_off=info_off+20;
    aw(0x5252414e);aw(20);aw(info_off);aw(ccb_off);aw(0);aw(0x52524644);aw(28*3+8);
    for(int f=0;f<3;++f)for(int i=0;i<7;++i)aw(0);          // three empty frames
    aw(0x414e494d);aw(20);aw(0);aw(0);aw(3);                // 'MINA' info, count at +16
    aw(0x43434220);for(int i=0;i<19;++i)aw(0);              // 'CCB ' template
    std::vector<uint8_t> cel;
    auto cw=[&](uint32_t v){for(int b=0;b<4;++b)cel.push_back(uint8_t(v>>(b*8)));};
    // 'SNAC' wrapper: size word, type 4, then the embedded sub-container at
    // +size whose own size word leads to the cel block (0x0040b5f0 chain).
    cw(0x43414e53);cw(12);cw(4);
    cw(0x5352434e);cw(0x18);cw(1);cw(0);cw(0);      // sub-container header+name
    cel.resize(48,0);
    cw(9);cw(cel_flags);                            // cel block: count, CCB flags
    std::vector<uint8_t> container;
    auto put=[&](const std::vector<uint8_t>& v){container.insert(container.end(),v.begin(),v.end());};
    auto kw=[&](uint32_t v){for(int b=0;b<4;++b)container.push_back(uint8_t(v>>(b*8)));};
    auto tag=[&](const char* t){for(int i=0;i<4;++i)container.push_back(uint8_t(t[i]));};
    tag("CRSR");kw(0);kw(0);tag("LBTR");kw(24);kw(0);      // 24-byte header, table at 24
    tag("LBTR");kw(0);kw(3);kw(0);                        // table: three entries
    // Two identical vehicles: the original's type loop excludes the last one,
    // so a single-vehicle library can never accept a spawn.
    const uint32_t anim_off=24+16+3*32,cel_off=anim_off+uint32_t(anim.size())*2;
    tag("MINA");kw(1);kw(anim_off);kw(uint32_t(anim.size()));   // displays as ANIM 1
    for(int i=0;i<16;++i)container.push_back(0);
    tag("MINA");kw(2);kw(anim_off+uint32_t(anim.size()));kw(uint32_t(anim.size()));
    for(int i=0;i<16;++i)container.push_back(0);
    tag("SNAC");kw(1);kw(cel_off);kw(uint32_t(cel.size()));     // displays as CANS 1
    for(int i=0;i<16;++i)container.push_back(0);
    put(anim);put(anim);put(cel);
    return container;
}

// Minimal i386 PE image with one section so read_exe_data can serve the
// traffic tables at their original virtual addresses.
static std::vector<uint8_t> synthetic_exe() {
    std::vector<uint8_t> exe(0x200+0x75000,0);
    auto w16=[&](size_t p,uint16_t v){exe[p]=uint8_t(v);exe[p+1]=uint8_t(v>>8);};
    auto w32=[&](size_t p,uint32_t v){for(int b=0;b<4;++b)exe[p+b]=uint8_t(v>>(b*8));};
    w16(0,0x5a4d);w32(60,0x80);                       // MZ, PE header at 0x80
    w32(0x80,0x4550);w16(0x84,0x14c);w16(0x86,1);     // PE, i386, one section
    w16(0x80+20,0xe0);w16(0x80+24,0x10b);             // optional header size/magic
    w32(0x80+24+28,0x400000);                         // image base
    const size_t table=0x80+24+0xe0;
    w32(table+8,0x75000);w32(table+12,0x1000);        // virtual size/address
    w32(table+16,0x75000);w32(table+20,0x200);        // raw size/pointer
    auto data=[&](uint32_t va)->size_t{return 0x200+va-0x1000;};
    for(int k=0;k<4;++k)w32(data(0x75694+4*k),uint32_t(k+1));          // drift words
    exe[data(0x75698+0x28)]=10;                                        // template size
    for(int k=0;k<4;++k)w32(data(0x68f00+4*k),0x40u<<k);               // lookahead windows
    return exe;
}

static void test_library_choose() {
    rr::VehicleLibrary library(synthetic_container(0x100000));
    if(library.vehicles().size()!=2)throw std::runtime_error("synthetic vehicle count");
    if(library.vehicles()[0].animation.frames.size()!=3)throw std::runtime_error("synthetic frames");
    if(library.vehicles()[0].mask!=0x100000)throw std::runtime_error("synthetic mask");
    if(library.choose(0,0)!=0)throw std::runtime_error("kind 0 accepted");
    if(library.choose(2,0)!=-1)throw std::runtime_error("kind 2 rejected by mask");
}

static void test_spawn_and_move() {
    rr::VehicleLibrary library(synthetic_container(0));
    std::vector<uint8_t> exe=synthetic_exe();
    rr::TrafficSystem system(library,exe);
    system.configure(6);
    std::vector<rr::RouteSample> route(300);
    for(auto& s:route)s=lane_sample(0x200,0x300);   // 2 left, 3 right lanes
    const int32_t camera=64*256,front=camera+0x5000;
    uint32_t seed=1;
    auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
    // No movement: the oncoming timer decays by rate/256 = 9 per tick and
    // the ahead timer rises by 9; both cross their thresholds on the same
    // tick (their reloads are equal).
    const int32_t reload=system.timers().oncoming_reload;
    const int steps=reload/9+1;
    for(int i=0;i<steps;++i)system.step(route,camera,front,0,camera,0,random);
    size_t oncoming=0,active=0;
    const rr::TrafficCar* spawned=nullptr;
    for(const auto& car:system.cars()) {
        if(!car.active)continue;
        ++active;
        if(car.kind==2) {++oncoming;spawned=&car;}
    }
    if(active==0)throw std::runtime_error("no spawn after timer decay");
    if(oncoming!=1)throw std::runtime_error("oncoming spawn count");
    if(spawned->position!=front-0x100)throw std::runtime_error("spawn position");
    if(spawned->lane!=-1 && spawned->lane!=-2)throw std::runtime_error("oncoming lane");    // -1 - (rand % 2)
    if(spawned->lateral!=(1000+spawned->lane*250+125)*256)throw std::runtime_error("oncoming lateral");
    if(spawned->phase<0xc0 || spawned->phase>0x1df)throw std::runtime_error("phase range");
    if(spawned->serial!=1)throw std::runtime_error("first serial");
    const uint32_t serial=spawned->serial;
    const int32_t lane_centre=(1000+spawned->lane*250+125)*256;
    // Movement with the player centred in the car's lane: the oncoming car
    // accelerates toward its cruise speed, drives backward through the horn
    // window and parks at the back cursor.
    int horns=0;
    for(int i=0;i<6000;++i) {
        system.step(route,camera,front,0,camera,lane_centre,random);
        for(const auto& e:system.take_sound_events()) {
            if(e.code!=rr::SoundEvent::horn)throw std::runtime_error("unexpected traffic event");
            ++horns;
        }
    }
    for(const auto& car:system.cars())
        if(car.active&&car.serial==serial)throw std::runtime_error("oncoming despawn at back cursor");
    if(horns==0)throw std::runtime_error("no horn while the player blocks the lane");
    // Pool limit is the original 0x00438b70(0x210,0x10) allocation.
    if(rr::TrafficSystem::pool_size!=16)throw std::runtime_error("pool size");
}

static void test_corpus() {
    const std::string root=RR_SOURCE_DIR "/../cdrom/ROADRASH";
    if(!std::filesystem::exists(root+"/DATA/CARS/NAPA.CAR")) {std::cout<<"No CARS corpus\n";return;}
    rr::Assets assets(root);
    rr::VehicleLibrary library(assets.read("DATA/CARS/NAPA.CAR"));
    if(library.vehicles().size()!=10)throw std::runtime_error("Napa vehicle count");
    for(size_t i=0;i<library.vehicles().size();++i) {
        const auto& v=library.vehicles()[i];
        if(v.animation.frames.empty())throw std::runtime_error("vehicle animation frames");
        // The first six vehicles pair with CANS wrappers and carry real masks.
        if(i<6 && v.mask==0)throw std::runtime_error("vehicle mask missing");
    }
    for(uint8_t kind=0;kind<4;kind+=2)
        for(uint32_t r=0;r<64;++r) {
            const int pick=library.choose(kind,r);
            if(pick<0)throw std::runtime_error("no vehicle accepted");
            if(size_t(pick)+1>=library.vehicles().size())throw std::runtime_error("choose range");
        }
    // Live route: spawn cars through the real course and render their nodes.
    rr::Course course=rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
    const auto route=rr::make_route(course,false);
    std::vector<uint8_t> exe=assets.read("ROADRASH.EXE");
    rr::TrafficSystem system(library,exe);
    system.configure(6);
    uint32_t seed=1;
    auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
    const int32_t camera=12544,front=camera+0x5000;
    for(int i=0;i<2000;++i)system.step(route,camera,front,i<100?300:0,camera,0,random);
    size_t active=0;
    for(const auto& car:system.cars())if(car.active)++active;
    if(active==0)throw std::runtime_error("no corpus traffic spawned");
    rr::DepthTable depths=rr::read_depth_table(exe);
    rr::Camera cam{0,150};
    const auto slices=rr::project_road(route,uint32_t(camera),cam,depths);
    const auto nodes=rr::traffic_draw_nodes(system,route,slices,cam,depths,camera,300);
    if(nodes.empty())throw std::runtime_error("no corpus traffic nodes");
    for(const auto& node:nodes)if(!node.texture)throw std::runtime_error("traffic node without texture");
}

int main() {
    try {
        test_configure();
        test_lane_count();
        test_library_choose();
        test_spawn_and_move();
        test_corpus();
        std::cout<<"traffic tests passed\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"traffic: "<<e.what()<<"\n";
        return 1;
    }
}
