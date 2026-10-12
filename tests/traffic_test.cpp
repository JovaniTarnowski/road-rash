#include "traffic.hpp"
#include "race.hpp"
#include "assets.hpp"
#include "course.hpp"
#include "course_scene.hpp"
#include <filesystem>
#include <iostream>

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
    // 0x0044dd00 from a zone event: reloads follow the density, the
    // oncoming timer restarts, behind/ahead keep running.
    t.behind=123;t.ahead=-77;
    rr::traffic_set_density(t,30);
    if(t.oncoming_reload!=div256(2500*30*0x3c) || t.oncoming!=t.oncoming_reload || t.behind!=123 || t.ahead!=-77)
        throw std::runtime_error("zone density");
    rr::traffic_set_density(t,-45);   // rate and cruise only
    if(t.rate!=4500 || t.cruise!=45 || t.oncoming_reload!=div256(2500*30*0x3c))throw std::runtime_error("zone rate");
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

static void test_zone_events() {
    // Zone ends apply as their slice enters the ring front (and during the
    // initial fill of the 128 slices behind the first front).
    rr::VehicleLibrary library(synthetic_container(0));
    rr::TrafficSystem system(library,synthetic_exe());
    system.configure(6);
    std::vector<rr::RouteSample> route(400);
    for(auto& s:route)s=lane_sample(0,0);   // no lanes: nothing spawns
    auto zone=[&](size_t i,int8_t d){route[i].path.zone_end=true;route[i].path.event_parameters[1]=uint8_t(d);};
    zone(20,40);zone(200,-45);zone(260,15);
    uint32_t seed=1;
    auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
    int32_t camera=0x3100;uint32_t tick=0;
    auto step=[&](int32_t moved){camera+=moved;system.step(route,camera+0x100,camera+0x5100,moved,camera,0,random,tick++);};
    step(0);   // fill [front-128, front): sample 20 is inside
    if(system.zone_events()!=1 || system.timers().density!=40)throw std::runtime_error("initial fill zone");
    while(((camera+0x5100)>>8)<=200)step(0x80);
    if(system.zone_events()!=2 || system.timers().rate!=4500)throw std::runtime_error("zone at ring front");
    step(-0x400);step(0x400);   // back off and return: the front slice is rebuilt
    if(system.zone_events()!=3)throw std::runtime_error("rebuilt front slice");
    while(((camera+0x5100)>>8)<=260)step(0x100);
    if(system.zone_events()!=4 || system.timers().density!=15 || system.timers().oncoming_reload!=div256(4500*15*0x3c))
        throw std::runtime_error("density after rate change");
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
    for(int i=0;i<steps;++i)system.step(route,camera,front,0,camera,0,random,uint32_t(i));
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
        system.step(route,camera,front,0,camera,lane_centre,random,uint32_t(i));
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

static void test_zone_corpus() {
    // Napa at a steady 0x60 Q8/tick: spawns with the course's zone events
    // against the old fixed density 6, over the same random stream.
    const std::string root=RR_SOURCE_DIR "/../cdrom/ROADRASH";
    if(!std::filesystem::exists(root+"/DATA/CARS/NAPA.CAR")) return;
    rr::Assets assets(root);
    rr::VehicleLibrary library(assets.read("DATA/CARS/NAPA.CAR"));
    const auto exe=assets.read("ROADRASH.EXE");
    const rr::Course course=rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
    auto run=[&](bool zones,int32_t& events) {
        auto route=rr::make_route(course,false);
        if(!zones)for(auto& s:route){s.path.zone_end=false;s.other.zone_end=false;}
        rr::TrafficSystem system(library,exe);
        system.configure(6);
        uint32_t seed=1,spawned=0,last=0,tick=0;
        auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
        for(int32_t camera=0x3100;((camera+0x5100)>>8)+1<int32_t(route.size());camera+=0x60) {
            system.step(route,camera+0x100,camera+0x5100,0x60,camera,0,random,tick++);
            for(const auto& car:system.cars())if(car.active && car.serial>last){++spawned;last=std::max(last,car.serial);}
        }
        events=system.zone_events();
        return spawned;
    };
    int32_t with_events=0,without_events=0;
    const uint32_t with=run(true,with_events),without=run(false,without_events);
    std::cout<<"Napa spawns at 0x60/tick: "<<with<<" with "<<with_events<<" zone events, "<<without<<" at fixed density 6\n";
    if(with_events==0 || without_events!=0)throw std::runtime_error("Napa zone events");
    if(with>=without)throw std::runtime_error("Napa zones should thin traffic");
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
    for(int i=0;i<2000;++i)system.step(route,camera,front,i<100?300:0,camera,0,random,uint32_t(i));
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

// Plan 11a Task 1 Step 3: synthetic rider–car contact boundaries. A live
// Napa race with one seeded car, driven through the real Race::step path:
// - boundary: gap 128 still enters contact (0x004368f0 breaks only when the
//   summed half-lengths are STRICTLY below the gap) but both separation
//   axes are stale, so the hit is silent and only the 0x00404f4d nudge moves
//   the rider; gap 129 misses the gate entirely (no nudge);
// - crashed rider: no new crash is entered (0x004049c0 rejects).
static void test_car_contact_synthetic() {
    const std::string root = RR_SOURCE_DIR "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(root + "/ROADRASH.EXE")) { std::cout << "No disc for contact tests\n"; return; }
    rr::Assets assets(root);
    const auto exe = assets.read("ROADRASH.EXE");
    const auto tables = rr::read_physics_tables(exe);
    const auto course = rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
    const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
    const rr::VehicleLibrary library(assets.read("DATA/CARS/NAPA.CAR"), exe);
    rr::CourseScene scene(assets, exe, course, "NAPA", 1, 0);
    // Boundary: bike half-length 64 + car half-length 64 = 128. Probe 0
    // seeds the car exactly at the edge (gap 128: contact), probe 1 one
    // unit outside it (gap 129: no contact). Lateral/height aligned.
    // Detection (0x004368f0: longitudinal gate breaks only when
    // sum < gap, so equality contacts; port src/race.cpp:362) admits the
    // edge probe. The separation axis choice (0x00436a40 prev-copy
    // strict <, port src/race.cpp:369-372) then sees both axes stale:
    // Race::move (0x004395e0, port src/race.cpp:615) recopies the
    // previous-position words at step start, so prev == current and
    // 128 < 128 is false on both axes. Impulses stay 0 and the handler
    // (0x004049c0 LAB_00404c2d: hit 0 < 0x7d1 with lateral 0 < 0x3e9 is
    // silent, port src/race.cpp:448-455,473) leaves steer rate and speed
    // alone; the tail (0x00404feb, port src/race.cpp:522) clears the
    // impulses/flags. The ONLY observable is the unconditional
    // 0x00404f4d nudge ((7680 + 16384) >> 4 * dt = 1504, port
    // src/race.cpp:519-520): car lateral equals rider lateral, so the
    // else branch pushes the rider -1504 laterally.
    for (int probe = 0; probe < 2; ++probe) {
        rr::Race race(rr::make_route(course, false), rr::make_route(course, true),
                      tables, specs.at(0), 90, {}, 0);
        rr::TrafficSystem traffic(library, exe);
        traffic.configure(6);
        race.set_traffic(&traffic);
        race.set_terrain_source([&](const rr::RouteSample& s) -> const rr::TerrainSlice& {
            return scene.terrain(s.section, s.local);
        });
        auto& me = race.riders()[0];
        me.bike.i32(0x178) = 64; me.bike.i32(0x174) = 7680; me.bike.i32(0x17c) = 8960;
        // Pre-roll one step so walls()/vertical() settle the rider inside
        // the shoulder (the grid lateral starts outside it); the car is
        // seeded against the SETTLED position, which is what
        // collide_traffic sees. Speed words are zeroed after the pre-roll
        // so travel does not move the rider during the measured step.
        race.set_rng_state(1);
        race.step(0);
        const int32_t p0 = me.bike.i32(0x1c), l0 = me.bike.i32(0x28);
        auto& cars = traffic.cars_mut();
        for (auto& c : cars) c.active = false;
        cars[0].active = true;
        cars[0].position = p0 + 128 + probe;   // probe 0: edge, probe 1: outside
        cars[0].lateral = l0;
        cars[0].speed = 0;
        me.bike.i32(0xec) = 0; me.bike.i32(0x108) = 0; me.bike.i32(0x10c) = 0;
        me.bike.i32(0xf0) = 0;
        const int32_t lat_before = me.bike.i32(0x28);
        const int32_t f0_before = me.bike.i32(0xf0);
        race.set_rng_state(1);
        race.step(0);
        const int32_t lat_after = me.bike.i32(0x28);
        if (probe == 0) {
            // Edge contact: silent hit, nudge only.
            if (lat_after != lat_before - 1504) throw std::runtime_error("edge nudge missed");
            if (me.bike.i32(0xf0) != f0_before) throw std::runtime_error("edge hit was not silent (f0)");
            if (me.bike.i32(0xec) != 0) throw std::runtime_error("edge hit was not silent (speed)");
            if (me.bike.i32(0x2f4) != 0) throw std::runtime_error("edge hit crashed the rider");
        } else {
            // Outside the gate: no contact, rider untouched.
            if (lat_after != lat_before) throw std::runtime_error("outside contact fired (lateral)");
            if (me.bike.i32(0xf0) != f0_before) throw std::runtime_error("outside contact fired (f0)");
            if (me.bike.i32(0xec) != 0) throw std::runtime_error("outside contact fired (speed)");
            if (me.bike.i32(0x2f4) != 0) throw std::runtime_error("outside contact fired (crash)");
        }
    }
    // Crashed rider: contact must not enter a new crash.
    {
        rr::Race race(rr::make_route(course, false), rr::make_route(course, true),
                      tables, specs.at(0), 90, {}, 0);
        rr::TrafficSystem traffic(library, exe);
        traffic.configure(6);
        race.set_traffic(&traffic);
        race.set_terrain_source([&](const rr::RouteSample& s) -> const rr::TerrainSlice& {
            return scene.terrain(s.section, s.local);
        });
        auto& me = race.riders()[0];
        me.bike.i32(0x178) = 64; me.bike.i32(0x174) = 7680; me.bike.i32(0x17c) = 8960;
        me.bike.i32(0x2f4) = 1;   // already crashed
        const int32_t p0 = me.bike.i32(0x1c), l0 = me.bike.i32(0x28);
        auto& cars = traffic.cars_mut();
        for (auto& c : cars) c.active = false;
        cars[0].active = true;
        cars[0].position = p0; cars[0].lateral = l0; cars[0].speed = 0;
        race.set_rng_state(1);
        race.step(0);
        if (me.bike.i32(0x2f8) == 2) throw std::runtime_error("crashed rider took car crash");
    }
    // Split roadway: a rider on the other branch at the same position as
    // the car must not collide (0x004368f0 +0x36 & 3 shared-half test;
    // port src/race.cpp:358-361). LIMIT (Oct 12): this probe is NOT YET
    // PASSING — the parked AI rider's lateral still moves during the step,
    // which means either the exclusion misses (route/sample desync between
    // route_of(r) and the dual flag) or AI-adjacent motion (walls on route
    // documents the gap; do not claim split parity until it passes.
    {
        const auto route_a = rr::make_route(course, false);
        size_t dual_sample = 0;
        for (size_t i = 0; i < route_a.size(); ++i)
            if (route_a[i].dual) { dual_sample = i; break; }
        if (dual_sample == 0) throw std::runtime_error("no dual sample on Napa route 0");
        rr::Opponent opp{specs.at(0), 90, 40, 40, 0, 1};
        rr::Race race(rr::make_route(course, false), rr::make_route(course, true),
                      tables, specs.at(0), 90, {opp}, 0);
        rr::TrafficSystem traffic(library, exe);
        traffic.configure(6);
        race.set_traffic(&traffic);
        auto& ai = race.riders()[1];
        ai.bike.i32(0x178) = 64; ai.bike.i32(0x174) = 7680; ai.bike.i32(0x17c) = 8960;
        ai.route = 1;   // other branch; player (riders_[0]) stays on route 0
        ai.sample = ai.previous_sample = dual_sample;
        ai.bike.i32(0x1c) = int32_t(dual_sample << 8);
        ai.bike.i32(0x138) = ai.bike.i32(0x1c);
        // Let walls() settle first: read the clamped lateral, THEN seed
        // the car dead-center on it. Otherwise the clamp moves the rider
        // after seeding and the test measures the clamp, not the exclusion.
        ai.bike.i32(0x114) = 0; ai.bike.i32(0x240) = 0;
        // Settle walls() BEFORE seeding the car: step once with no cars,
        // then seed dead-center on the clamped lateral.
        race.set_rng_state(1);
        race.step(0);
        const int32_t l0 = ai.bike.i32(0x28);
        auto& cars = traffic.cars_mut();
        for (auto& c : cars) c.active = false;
        cars[0].active = true;
        cars[0].position = ai.bike.i32(0x1c); cars[0].lateral = l0; cars[0].speed = 0;
        ai.bike.i32(0xec) = 0; ai.bike.i32(0x108) = 0; ai.bike.i32(0x10c) = 0;
        ai.bike.i32(0x114) = 0; ai.bike.i32(0x240) = 0;   // no throttle: no travel
        ai.brain.current_speed = 0; ai.brain.target_speed = 0; ai.brain.base_speed = 0;   // parked AI: no drive
        ai.ai = false;   // no think/step: rider is a static post; only the contact pass may move it
        ai.bike.i32(0xf0) = 0;
        const int32_t lat_before = ai.bike.i32(0x28);
        race.set_rng_state(1);
        race.step(0);
        if (ai.bike.i32(0x28) != lat_before) throw std::runtime_error("split-roadway contact fired (lateral)");
        if (ai.bike.i32(0xf0) != 0) throw std::runtime_error("split-roadway contact fired (f0)");
        if (ai.bike.i32(0x2f4) != 0) throw std::runtime_error("split-roadway contact fired (crash)");
    }
    std::cout << "car contact synthetic boundaries pass\n";
}

static void test_intersections() {
    // Plan 24 Task 1 Step 3 (0x0041eca0/0x0041f040): a type-1 zone end
    // creates one controller and four signals; phases advance at
    // 600/180/780; crossing cars spawn only in phase 0 on ticks with bit
    // 3 set; pools cap at 10 controllers and 40 signals.
    rr::VehicleLibrary library(synthetic_container(0));
    auto make_route=[]() {
        std::vector<rr::RouteSample> route(400);
        for(auto& s:route)s=lane_sample(0x300,0x300);   // 3+3 lanes
        route[100].path.zone_end=true;route[100].path.event_type=1;
        route[100].path.event_parameters[1]=6;
        return route;
    };
    auto count_crossing=[](rr::TrafficSystem& system) {
        size_t n=0;
        for(const auto& car:system.cars())
            if(car.active && (car.kind&1)) {
                ++n;
                if(car.lateral_vel!=0x78000 && car.lateral_vel!=-0x78000)
                    throw std::runtime_error("crossing lateral speed");
                if(car.kind!=1)throw std::runtime_error("default crossing kind");
            }
        return n;
    };
    // Gated spawn scenario: static camera (no window/teardown drift, no
    // regular-spawn pool pressure for 390 steps). Sample 100 sits inside
    // the initial 128-slice fill, so the placing step is step 0 and the
    // seed-780 phase-2 signals wrap to 0 exactly there — gated only by
    // the placing tick's bit 3. No further phase-0 wrap fits in 390 steps.
    auto gated=[&](uint32_t first_tick) {
        auto route=make_route();
        rr::TrafficSystem system(library,synthetic_exe());
        system.configure(6);
        uint32_t seed=1;
        auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
        const int32_t camera=0x3100;
        uint32_t tick=first_tick;
        for(int i=0;i<390;++i)
            system.step(route,camera+0x100,camera+0x5100,0,camera,0,random,tick++);
        return count_crossing(system);
    };
    if(gated(8)==0)throw std::runtime_error("no crossing spawn on gated tick");
    if(gated(16)!=0)throw std::runtime_error("crossing spawn on ungated tick");
    // Layout: one controller, four signals, durations, indices, laterals.
    {
        auto route=make_route();
        rr::TrafficSystem system(library,synthetic_exe());
        system.configure(6);
        uint32_t seed=1;
        auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
        system.step(route,0x3100+0x100,0x3100+0x5100,0,0x3100,0,random,16);
        size_t controllers=0,signals=0;
        for(const auto& c:system.controllers())if(c.active)++controllers;
        for(const auto& s:system.signals())if(s.active)++signals;
        if(controllers!=1)throw std::runtime_error("type-1 zone creates one controller");
        if(signals!=4)throw std::runtime_error("type-1 zone creates four signals");
        const auto& c0=system.controllers()[0];
        if(c0.durations[0]!=600 || c0.durations[1]!=180 || c0.durations[2]!=780)
            throw std::runtime_error("phase table 600/180/780");
        for(int k=0;k<4;++k) {
            const auto& s=system.signals()[size_t(c0.signals[k])];
            const int want_index=(k==0)?0:(k==1)?2:(k==2)?3:1;
            const int32_t want_lat=(k%2==0)?0x50:0xb0;
            if(s.index!=want_index || s.lateral_q8!=want_lat)throw std::runtime_error("signal layout");
        }
    }
    // Pool caps: controllers and signals never exceed the original pool
    // sizes (10 and 40) across many zone ends; freed behind the ring back.
    {
        auto route=make_route();
        rr::TrafficSystem big(library,synthetic_exe());
        big.configure(6);
        for(int z=0;z<15;++z){route[50+z*20].path.zone_end=true;route[50+z*20].path.event_type=1;}
        uint32_t seed=9;
        auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
        int32_t cam2=0x3100;uint32_t tick2=0;
        while(((cam2+0x5100)>>8)<50+14*20+10){cam2+=0x100;big.step(route,cam2+0x100,cam2+0x5100,0x100,cam2,0,random,tick2++);}
        size_t bc=0,bs=0;
        for(const auto& c:big.controllers())if(c.active)++bc;
        for(const auto& s:big.signals())if(s.active)++bs;
        if(bc>10 || bs>40)throw std::runtime_error("intersection pool caps");
    }
    // Repeated placement: two zone ends make two controllers.
    {
        auto route=make_route();
        route[120].path.zone_end=true;route[120].path.event_type=2;
        rr::TrafficSystem two(library,synthetic_exe());
        two.configure(6);
        uint32_t seed=7;
        auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
        two.step(route,0x3100+0x100,0x3100+0x5100,0,0x3100,0,random,16);
        size_t bc=0;
        for(const auto& c:two.controllers())if(c.active)++bc;
        if(bc!=2)throw std::runtime_error("repeated placement");
    }
}
static void test_signal_phase() {
    // 0x004415b0: nearest active signal phase within 0x801, else -1.
    rr::VehicleLibrary library(synthetic_container(0));
    rr::TrafficSystem system(library,synthetic_exe());
    system.configure(6);
    if(system.signal_phase_at(100*256)!=-1)throw std::runtime_error("phase without signals");
    std::vector<rr::RouteSample> route(400);
    for(auto& s:route)s=lane_sample(0x300,0x300);
    route[100].path.zone_end=true;route[100].path.event_type=1;
    uint32_t seed=1;
    auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
    system.step(route,0x3100+0x100,0x3100+0x5100,0,0x3100,0,random,16);
    if(system.controllers().empty() || !system.controllers()[0].active)
        throw std::runtime_error("no controller for phase query");
    // All four signals sit at sample 100; phases advance from the seed.
    const int at=system.signal_phase_at(100*256);
    if(at<0 || at>2)throw std::runtime_error("phase out of range at signal");
    if(system.signal_phase_at(100*256+0x801)!=-1)throw std::runtime_error("phase past 0x801");
    if(system.signal_phase_at(100*256-0x801)!=-1)throw std::runtime_error("phase before 0x801");
}

static void test_intersections_all_courses() {
    // Plan 24 Task 5 intersections counterpart: every course with
    // type-1..3 zone ends places controllers through a full run, and the
    // pools never exceed the original sizes (10 controllers, 40 signals).
    const std::string root=RR_SOURCE_DIR "/../cdrom/ROADRASH";
    if(!std::filesystem::exists(root+"/ROADRASH.EXE")){std::cout<<"No disc for course sweep\n";return;}
    rr::Assets assets(root);
    const auto exe=assets.read("ROADRASH.EXE");
    for(const char* name:{"CITY","HIWAY","MEDLY","NAPA","CANYN"}) {
        const std::string course_name(name);
        const rr::Course course=rr::read_course(assets.read("DATA/COURSES/"+course_name+".CRS"));
        const auto route=rr::make_route(course,false);
        rr::VehicleLibrary library(assets.read("DATA/CARS/"+course_name+".CAR"));
        rr::TrafficSystem system(library,exe);
        system.configure(6);
        uint32_t seed=1;
        auto random=[&]()->uint32_t{seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
        bool ever_placed=false;
        uint32_t tick=0;
        for(int32_t camera=0x3100;((camera+0x5100)>>8)+1<int32_t(route.size());camera+=0x60) {
            system.step(route,camera+0x100,camera+0x5100,0x60,camera,0,random,tick++);
            size_t bc=0,bs=0;
            for(const auto& c:system.controllers())if(c.active){++bc;ever_placed=true;}
            for(const auto& s:system.signals())if(s.active)++bs;
            if(bc>rr::TrafficSystem::controller_pool || bs>rr::TrafficSystem::signal_pool)
                throw std::runtime_error(std::string("pool overflow on ")+name);
        }
        if(!ever_placed)throw std::runtime_error(std::string("no intersections on ")+name);
        std::cout<<name<<": intersections occur within pool caps\n";
    }
}
int main() {
    try {
        test_configure();
        test_lane_count();
        test_library_choose();
        test_spawn_and_move();
        test_zone_events();
        test_intersections();
        test_signal_phase();
        test_intersections_all_courses();
        test_corpus();
        test_zone_corpus();
        test_car_contact_synthetic();
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"traffic: "<<e.what()<<"\n";
        return 1;
    }
}
