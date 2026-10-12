#include "traffic.hpp"
#include "actor_render.hpp"
#include "exe_tables.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace rr {
namespace {
int32_t bits(uint32_t v){int32_t r;std::memcpy(&r,&v,4);return r;}
int32_t wrap(int64_t v){return bits(uint32_t(v));}
// Original division-by-256 idiom: (x + (x < 0 ? 0xff : 0)) >> 8 with an
// arithmetic shift, i.e. x86 sar semantics (rounding toward zero).
int32_t div256(int32_t x){return (x + ((x >> 31) & 0xff)) >> 8;}
uint32_t word(const std::vector<uint8_t>& b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
// 0x0040b5f0 with index 0: skip the SNAC header unless word +8 is 1, then
// follow the embedded sub-container size to the cel block; the mask is the
// CCB flags word there (0x0040b620 -> 0x004b8fe0[i]).
uint32_t snac_mask(const std::vector<uint8_t>& payload) {
    auto range=[&](size_t p,size_t n){if(p>payload.size() || n>payload.size()-p)throw std::runtime_error("Truncated SNAC wrapper");};
    auto w=[&](size_t p){range(p,4);return word(payload,p);};
    range(0,12);
    if(w(0)!=0x43414e53)throw std::runtime_error("Invalid SNAC wrapper");
    size_t base=8;
    if(w(8)!=1)base=w(4);                    // header skip via the size word
    range(base,8);
    const size_t cel=base+w(base+4)+0xc;     // 0x0040b5f0 chain
    range(cel,8);
    return w(cel+4);                         // CCB flags
}
}

VehicleLibrary::VehicleLibrary(const std::vector<uint8_t>& container,const std::vector<uint8_t>& exe) {
    const auto entries=read_resources(container);
    std::vector<const Resource*> anims,cans;
    for(const auto& e:entries) {
        if(e.tag=="ANIM")anims.push_back(&e);
        else if(e.tag=="CANS")cans.push_back(&e);
    }
    if(anims.empty() && cans.empty())throw std::runtime_error("No vehicle resources in CARS container");
    // 0x004465f0: vehicle i uses 'ANIM' i+1; the loop stops at the first
    // missing animation, and that index becomes the vehicle count DAT_00474d88.
    std::sort(anims.begin(),anims.end(),[](const Resource* a,const Resource* b){return a->id<b->id;});
    std::sort(cans.begin(),cans.end(),[](const Resource* a,const Resource* b){return a->id<b->id;});
    std::vector<uint8_t> inherited_cans;
    for(size_t i=0;i<anims.size();++i) {
        if(anims[i]->id!=i+1)break;    // FUN_004153a0('MINA', i+1) miss ends the loop
        Vehicle v;
        v.animation=read_animation(std::vector<uint8_t>(container.begin()+anims[i]->offset,
            container.begin()+anims[i]->offset+anims[i]->size));
        // 'CANS' entries pair sequentially (the original counts its own hits).
        if(!cans.empty() && cans.front()->id==vehicles_.size()+1) {
            inherited_cans={container.begin()+cans.front()->offset,container.begin()+cans.front()->offset+cans.front()->size};
            v.mask=snac_mask(inherited_cans);
            cans.erase(cans.begin());
        }
        // 0x004465f0 falls back to the previous pair's CANS pointer.
        if(!inherited_cans.empty())v.mask=snac_mask(inherited_cans);
        if(!exe.empty()) {
            if(inherited_cans.empty())throw std::runtime_error("Missing initial vehicle CANS");
            v.views=read_actor_graphics(inherited_cans);
            for(int code=0;code<3;++code) {
                const auto sequence=read_actor_sequence(inherited_cans,read_exe_data(exe,0x474da0+code*8,8));
                if(!sequence){v.playback.push_back(std::nullopt);continue;}
                ActorSequence frames;for(const auto& descriptor:*sequence)frames.push_back(descriptor.playback);
                v.playback.push_back(std::move(frames));
            }
            for(const auto& group:v.views)for(const auto& cell:group)if(cell.frame>=v.animation.frames.size())throw std::runtime_error("Vehicle CANS reference outside RRAN");
        }
        vehicles_.push_back(std::move(v));
        if(vehicles_.size()>=15)break;    // original stops after index 14
    }
    if(vehicles_.empty())throw std::runtime_error("No vehicle animations in CARS container");
}

int VehicleLibrary::choose(uint8_t kind,uint32_t random) const {
    // 0x0044df60: the speed-class filter; the loop excludes the last vehicle
    // (cVar4 < DAT_00474d88 - 1).
    const uint32_t filter=(kind&1)?0x40000u:(kind==2?0x80000u:0x100000u);
    std::vector<uint8_t> accepted;
    for(size_t v=0;v+1<vehicles_.size();++v)
        if((vehicles_[v].mask&0x1c0000u)==0 || (vehicles_[v].mask&filter)!=0)accepted.push_back(uint8_t(v));
    if(accepted.empty())return -1;
    return int(accepted[random%accepted.size()]);
}

void traffic_set_density(TrafficTimers& t,int density) {
    // 0x0044dd00 (single player; multiplayer quadruples the density).
    if(density<0) {t.cruise=-density;t.rate=-density*100;return;}
    if(density==0)density=6;
    t.density=density;
    t.oncoming_reload=div256(t.rate*density*0x3c);
    t.oncoming=t.oncoming_reload;
    t.other_reload=div256(t.rate*density*0x3c);
}
void traffic_configure(TrafficTimers& t,int density) {
    if(density<0) {traffic_set_density(t,density);return;}
    traffic_set_density(t,-25);         // 0x0044db80: 0x0044dd00(-0x19)
    traffic_set_density(t,density);     // then 0x0044dd00(6)
    t.behind=0;
    t.ahead=-t.other_reload;            // 0x0044db80 tail
}

int traffic_lane_count(int32_t lanes_q8) {
    return int8_t(div256(lanes_q8));    // (char)(value + sign fix) >> 8
}

TrafficSystem::TrafficSystem(const VehicleLibrary& library,std::vector<uint8_t> exe)
    :library_(library) {
    const auto drift=read_exe_data(exe,0x00475694,16);
    for(int k=0;k<4;++k)drift_rates_[k]=int8_t(word(drift,size_t(k)*4))*800;    // 0x0044e710: +0x1e0
    const auto look=read_exe_data(exe,0x00468f00,16);
    for(int k=0;k<4;++k)lookahead_[k]=int32_t(word(look,size_t(k)*4))*256;      // 0x00449e60 window
    const auto car_template=read_exe_data(exe,0x00475698,0x64);
    sprite_size_=car_template[0x28];    // car template size class (+0x160 copy)
}

bool TrafficSystem::place(uint8_t kind,int8_t lane,const RouteSample& at,int32_t spawn_q8,const std::function<uint32_t()>& random) {
    const int vehicle=library_.choose(kind,random());    // _rand() in 0x0044df60
    if(vehicle<0)return false;
    for(auto& car:cars_) {
        if(car.active)continue;
        // 0x0044df60 (kind bit 0 clear): lateral from the road centre.
        car.lateral=wrap(int64_t(at.offset+lane*250+125)*256);
        car.position=spawn_q8;
        car.kind=kind;car.vehicle=uint8_t(vehicle);car.lane=lane;car.speed=0;
        car.phase=(random()&0x7f)+0xc0;    // +0x71: full int slot, 192..319
        car.jitter_a=int8_t(random());              // +0x72
        car.jitter_b=int8_t(random());              // +0x1c9
        car.jitter_c=int8_t(random());              // +0x1ca
        car.serial=++serial_;                       // DAT_004b7e98
        car.active=true;
        return true;
    }
    return false;    // pool exhausted: FUN_00438a80 returns null
}

void TrafficSystem::move(TrafficCar& car,const std::vector<RouteSample>& route,int32_t front_q8,
                        int32_t back_q8,int32_t player_q8,int32_t player_lateral,const std::function<uint32_t()>& random) {
    // Despawn outside the ring window [back, front): 0x00437e60 parks the
    // object, which returns it to the free pool.
    if(car.position>=front_q8 || car.position<back_q8) {car.active=false;return;}
    // 0x0044e710 no-obstacle branch: the speed target is the phase-scaled
    // cruise value (DAT_004b7e8c = 25 times +0x71 >> 8, times 100 through
    // 0x00449880), reached at the phase-scaled rate (+0x71 * 0x12 >> 8).
    // Obstacle braking and lane changes (0x0044a070/0x00449e60) are not ported.
    const int32_t target=div256(timers_.cruise*car.phase)*100;
    const int32_t rate=div256(car.phase*0x12);
    car.speed+=rate;
    if(target<car.speed)car.speed=target;
    // Position advance from the movement form ((v*2 - a*dt)*dt) >> 9 with the
    // car's surface drag neglected; oncoming cars (kind 2) drive backward.
    car.position=wrap(int64_t(car.position)+int64_t(car.speed>>8)*(car.kind==2?-1:1));
    // Lateral steering toward the lane centre of the car's current sample
    // (0x0044e710 +0x1dc target, drift rate from 0x00475694[kind] * 800).
    const size_t sample=size_t(std::clamp(car.position,int32_t(0),int32_t((route.size()-1)*256))>>8);
    const RouteSample& at=route.at(sample);
    const int32_t lane_target=wrap(int64_t(at.offset+car.lane*250+125)*256);
    const int32_t error=wrap(int64_t(lane_target)-car.lateral);
    const int32_t drift=drift_rates_[car.kind&3];
    car.lateral=wrap(int64_t(car.lateral)+std::clamp(error,-drift,drift));
    // 0x0044efa0 horn: the player is the car's nearest object ahead.
    const int32_t ahead=(car.kind==2)?wrap(int64_t(car.position)-player_q8):wrap(int64_t(player_q8)-car.position);
    if(ahead>0 && ahead<lookahead_[car.kind&3] && std::abs(player_lateral-car.lateral)<600*256) {
        if(random()%10==1)
            events_.push_back({SoundEvent::horn,car.lateral>>8,0,car.position>>8,car.speed,0});
    }
}

void TrafficSystem::build_slice(const std::vector<RouteSample>& route,int32_t sample) {
    // 0x00454c80 -> 0x00454240 with the forward traversal: each roadway of
    // the slice (own half first) whose event ends here calls 0x0041ec70.
    if(sample<0 || size_t(sample)>=route.size())return;
    const auto& at=route[size_t(sample)];
    auto zone=[&](const PathSample& path) {
        if(!path.zone_end)return;
        traffic_set_density(timers_,int8_t(path.event_parameters[1]));
        ++zone_events_;
    };
    zone(at.path);
    if(at.dual)zone(at.other);
}

void TrafficSystem::step(const std::vector<RouteSample>& route,int32_t camera_q8,int32_t front_q8,
                        int32_t distance_moved,int32_t player_q8,int32_t player_lateral,const std::function<uint32_t()>& random) {
    const int32_t back_q8=front_q8-0x8000;    // ring span 0x8000 (0x004552e0)
    // Ring slices built this step (0x00454120 runs before 0x0044dd90).
    const int32_t front_sample=front_q8>>8;
    if(!filled_) {
        filled_=true;
        built_end_=(front_q8-distance_moved)>>8;
        for(int32_t s=built_end_-128;s<built_end_;++s)build_slice(route,s);
    }
    for(;built_end_<front_sample;++built_end_)build_slice(route,built_end_);
    if(front_sample<built_end_)built_end_=front_sample;   // backward: rebuilt later
    const auto sample_at=[&](int32_t q8)->const RouteSample& {
        const size_t index=size_t(std::clamp(q8,int32_t(0),int32_t((route.size()-1)*256))>>8);
        return route.at(index);
    };
    // Oncoming timer 0x004b7e90: decays by rate/256 + distance even when the
    // rider stops; spawns kind 2 one sample behind the front cursor with a
    // negative lane on the left side (sub-slice +0x30).
    timers_.oncoming-=div256(timers_.rate)+distance_moved;
    if(timers_.oncoming<0) {
        const int32_t spawn=front_q8-0x100;
        const int count=traffic_lane_count(sample_at(spawn).path.left_q8);
        if(count>0)place(2,int8_t(-1-int32_t(random()%uint32_t(count))),sample_at(spawn),spawn,random);
        timers_.oncoming+=timers_.oncoming_reload;
    }
    // Behind timer 0x004b7e94: decays only while the rider moves.
    timers_.behind-=distance_moved;
    timers_.behind+=div256(timers_.rate);
    if(timers_.behind<0) {
        const int32_t spawn=front_q8-0x100;
        const int count=traffic_lane_count(sample_at(spawn).path.right_q8);    // sub-slice +0x2c
        if(count>0)place(0,int8_t(random()%uint32_t(count)),sample_at(spawn),spawn,random);
        timers_.behind+=timers_.other_reload;
    } else if(timers_.other_reload*2<timers_.behind)timers_.behind=timers_.other_reload*2;
    // Ahead timer 0x004b7e80: grows while the rider is slow; spawns one sample
    // past the back cursor.
    timers_.ahead-=distance_moved;
    timers_.ahead+=div256(timers_.rate);
    if(0<timers_.ahead) {
        const int32_t spawn=back_q8+0x100;
        const int count=traffic_lane_count(sample_at(spawn).path.right_q8);
        if(count>0)place(0,int8_t(random()%uint32_t(count)),sample_at(spawn),spawn,random);
        timers_.ahead-=timers_.other_reload;
    } else {
        const int32_t cap=-timers_.other_reload*2;
        if(cap-timers_.ahead!=0 && timers_.ahead<=cap)timers_.ahead=cap;
    }
    for(auto& car:cars_)if(car.active)move(car,route,front_q8,back_q8,player_q8,player_lateral,random);
}

void TrafficSystem::append_snapshot(std::vector<FieldValue>& out) const {
    for (size_t i = 0; i < cars_.size(); ++i) {
        const auto& car = cars_[i];
        const std::string p = "car." + std::to_string(i) + ".";
        out.push_back({p + "active", car.active ? 1 : 0});
        out.push_back({p + "kind", car.kind});
        out.push_back({p + "vehicle", car.vehicle});
        out.push_back({p + "lane", car.lane});
        out.push_back({p + "position", car.position});
        out.push_back({p + "lateral", car.lateral});
        out.push_back({p + "speed", car.speed});
        out.push_back({p + "phase", car.phase});
    }
}

std::optional<DrawNode> traffic_car_node(const Vehicle& vehicle,const TrafficCar& car,
    const RoadSlice& near,const RoadSlice& far,int road,const RouteSample& sample,
    const Camera& camera,const DepthTable& depths,int32_t camera_q8,int32_t ceiling) {
    if(vehicle.playback.size()!=3)return std::nullopt;
    const int64_t distance=std::abs(int64_t(car.position)-camera_q8);
    const size_t code=distance>0xa00 && vehicle.playback[2]?2:distance>0x500 && vehicle.playback[1]?1:0;
    const auto& sequence=vehicle.playback[code];if(!sequence || sequence->empty())return std::nullopt;
    const auto& a=near.road[road];const auto& b=far.road[road];
    ActorOrientation orientation;orientation.depth=near.depth;orientation.position=car.position;orientation.road_x=a.road_x+camera.x;orientation.lateral_q8=car.lateral-sample.offset*256;orientation.road_heading=a.heading;orientation.heading=car.kind==2?-0x800000:0;
    const auto angles=actor_angles(orientation,camera.x);
    const auto selected=select_actor_view(sequence->front(),vehicle.views,angles.yaw,angles.roll);
    const auto& cell=vehicle.views.at(selected.group).at(selected.subframe);const auto& frame=vehicle.animation.frames.at(cell.frame);
    ActorProjection projection;projection.road_x=orientation.road_x;projection.far_x=b.road_x+camera.x;projection.road_y=a.world_y;projection.far_y=b.world_y;projection.lateral=orientation.lateral_q8/256;projection.depth=near.depth;projection.fraction=uint8_t(car.position);projection.size=0x500;projection.flags=cell.flags;projection.occlusion=ceiling;
    return actor_node(frame,frame.texture.get(),projection,camera,depths);
}
std::vector<DrawNode> traffic_draw_nodes(const TrafficSystem& system,
    const std::vector<RouteSample>& route,const std::vector<RoadSlice>& slices,
    const Camera& camera,const DepthTable& depths,int32_t camera_q8,uint32_t tick) {
    std::vector<DrawNode> result;
    if(slices.empty())return result;
    struct Visible {const TrafficCar* car;int32_t depth;int32_t road_x;int32_t world_y;};
    std::vector<Visible> visible;
    for(const auto& car:system.cars()) {
        if(!car.active)continue;
        // Depth in the projector's Q8 sample units, relative to the camera.
        const int32_t depth=wrap(int64_t(car.position)-camera_q8);
        if(depth<=0 || depth>=0x2200)continue;    // outside the projected window
        // Bracketing projected slices around the car's position on the
        // route's own roadway.
        size_t n=0;
        while(n+1<slices.size() && slices[n+1].sample*256<uint32_t(car.position))++n;
        const RoadSlice& a=slices[n];
        const RoadSlice& b=slices[std::min(n+1,slices.size()-1)];
        const int own_a=a.count==1?0:a.own,own_b=b.count==1?0:b.own;
        const int32_t span=int32_t(b.sample-a.sample)*256;
        const int32_t fraction=span>0?std::clamp(wrap(int64_t(car.position)-int64_t(a.sample)*256)*256/span,0,255):0;
        const int32_t road_x=wrap(int64_t(a.road[own_a].road_x)+wrap(int64_t(wrap(int64_t(b.road[own_b].road_x)-a.road[own_a].road_x)*fraction)>>8));
        const int32_t world_y=wrap(int64_t(a.road[own_a].world_y)+wrap(int64_t(wrap(int64_t(b.road[own_b].world_y)-a.road[own_a].world_y)*fraction)>>8));
        visible.push_back({&car,depth,road_x,world_y});
    }
    std::sort(visible.begin(),visible.end(),[](const Visible& x,const Visible& y){return x.depth>y.depth;});
    for(const auto& v:visible) {
        const auto& vehicle=system.library().vehicles().at(v.car->vehicle);
        if(vehicle.animation.frames.empty())continue;
        // Frame cycling is a native approximation of the RRAN advancement
        // (0x0043ae10/0x0043ae90), which is not ported.
        const auto& frame=vehicle.animation.frames[(tick+v.car->phase)%vehicle.animation.frames.size()];
        if(!frame.texture)continue;
        const uint32_t index=(uint32_t(v.depth+128)&~1u)/2;
        if(index>=depths.scale.size())continue;
        const int32_t projection=depths.scale[index];
        const int32_t scale=wrap(int64_t(system.sprite_size())*projection)>>2;
        const auto& bounds=frame.texture->bounds;
        const int32_t width=bounds[2]-bounds[0],height=bounds[3]-bounds[1];
        // Same projection as the verified scenery path (scenery_node). The
        // interpolated road centre is already relative to the camera.
        const int32_t anchor_x=wrap(int64_t(v.road_x+(v.car->lateral>>8))*projection+160*65536);
        const int32_t x=wrap(int64_t(anchor_x)-int64_t(frame.hotspot[0])*scale);
        const int32_t y=wrap(int64_t(camera.height-v.world_y)*projection-int64_t(frame.hotspot[1])*scale+90*65536);
        const int32_t right=wrap(int64_t(x)+int64_t(width)*scale),bottom=wrap(int64_t(y)+int64_t(height+1)*scale);
        result.push_back({{{{x,y},{right,y},{right,bottom},{x,bottom}}},frame.texture.get(),std::nullopt,false,false});
    }
    (void)route;
    return result;
}

}