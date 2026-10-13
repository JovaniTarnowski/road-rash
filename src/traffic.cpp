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
        car=TrafficCar{};
        car.lateral=wrap(int64_t(at.offset+lane*250+125)*256);
        car.position=spawn_q8;
        car.kind=kind;car.vehicle=uint8_t(vehicle);car.lane=lane;car.speed=0;
        car.heading=kind==2?-0x800000:0;
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

namespace {
// Speed in 0x0044e710 target units: (v >> 9) + (v >> 12) + (v >> 7), about v / 100.
int32_t speed_units(int32_t v){return (v>>9)+(v>>12)+(v>>7);}
// 0x0044a020 with an even kind: 250-unit lanes from the slice centre.
int lane_of(int32_t lateral,int32_t offset) {
    const int32_t d=wrap(int64_t(lateral)-int64_t(offset)*256);
    return int8_t(int32_t(int64_t(div256(d))/250)-(d<0?1:0));
}
// 0x00449a80: approach speed toward a reference p5 (position, or lateral for
// odd kinds), clamped to [floor, cruise] and to +-step of the current speed.
int32_t follow_speed(int kind,int32_t self_position,int32_t self_lateral,int32_t current,int32_t cruise,int32_t floor,
                     int32_t other_position,int32_t other_lateral,int32_t other_speed,int32_t step,int32_t window,int32_t bias) {
    int32_t d;
    if(kind==0)d=(other_position-self_position)>>4;
    else if(kind==2)d=(self_position-other_position)>>4;
    else d=(other_lateral-self_lateral)>>12;
    d+=bias;
    int32_t v=cruise;
    if(d<=window) {
        v=floor;
        if(-window==d || -d<window) {
            if(d<0)d=(d+((d>>31)&7))>>3;
            v=d+other_speed;
            if(cruise<v)v=cruise;
            else if(v<floor)v=floor;
        }
    }
    if(step<v-current)return current+step;
    if(step<current-v)return current-step;
    return v;
}
constexpr int32_t crossing_half_width=0xa000;   // 0x00475700 + kind*0x18 for kinds 1/3 (0x4000 for 0/2)
constexpr int32_t car_half_length=0x40;         // +0x178 from the same table
}

int32_t TrafficSystem::car_half_width(const TrafficCar& car) { return (car.kind&1)?crossing_half_width:0x4000; }

std::pair<int32_t,int32_t> TrafficSystem::plan_speed(TrafficCar& car,const std::vector<RouteSample>& route) {
    // 0x0044e710 with dt 1 (the port steps cars every tick).
    const int kind=car.kind&3;
    const int32_t cruise=(timers_.cruise*car.phase)>>8;   // iVar8
    const int32_t current=speed_units(car.speed);           // iVar9
    const int32_t near=(current*0x300)>>8;                  // iVar10
    const int32_t gap_unit=near<0x15?0x14:near;             // iVar7
    int32_t window=gap_unit*2;                              // iVar15
    int32_t gap=1000,target=cruise;                         // local_14, local_1c
    bool following=false,hurry=false;                       // bVar3, bVar4
    const size_t sample=size_t(std::clamp(car.position,int32_t(0),int32_t((route.size()-1)*256))>>8);
    const RouteSample& at=route[sample];
    // Signal latch (kinds 0/2): the nearest signal of type 0x00468f00[kind]
    // (index 0 for kind 0, 2 for kind 2) ahead in the driving direction
    // within (iVar15 + 0x80) * 16.
    if(!(kind&1) && car.signal<0) {
        const int want=kind==0?0:2;const int32_t limit=(window+0x80)*16;int32_t best=limit;
        for(size_t i=0;i<signals_.size();++i) {
            const auto& s=signals_[i];if(!s.active || s.index!=want)continue;
            const int32_t ahead=kind==0?s.position_q8-car.position:car.position-s.position_q8;
            if(ahead<0 || ahead>=best)continue;
            best=ahead;car.signal=int(i);
        }
    }
    // Obstacle in the lane (0x0044a070, mask 0x3f): even kinds scan the
    // driving direction within iVar7 << 5 by lane; odd kinds look across
    // the same sample toward their target side.
    struct Other {int32_t position,lateral,velocity,lateral_velocity;};
    std::optional<Other> obstacle;int32_t best=INT32_MAX;
    auto consider=[&](const Other& o) {
        int32_t distance;
        if(!(kind&1)) {
            distance=kind==0?o.position-car.position:car.position-o.position;
            if(distance<=0 || distance>=(gap_unit<<5))return;
            const size_t os=size_t(std::clamp(o.position,int32_t(0),int32_t((route.size()-1)*256))>>8);
            if(lane_of(o.lateral,route[os].offset)!=car.lane)return;
        } else {
            if(((o.position^car.position)&~0xff)!=0)return;
            distance=kind==1?o.lateral-car.lateral:car.lateral-o.lateral;
            if(distance<=0)return;
        }
        if(distance<best){best=distance;obstacle=o;}
    };
    for(const auto& other:cars_) {
        if(!other.active || &other==&car)continue;
        const int32_t forward=other.kind==2?-other.speed:(other.kind&1)?0:other.speed;
        const int32_t across=other.kind==1?other.speed:other.kind==3?-other.speed:0;
        consider({other.position,other.lateral,forward,across});
    }
    for(const auto& o:obstacles_)consider({o.position,o.lateral,o.velocity,o.lateral_velocity});
    if(obstacle) {
        gap=kind==0?(obstacle->position-car.position)>>4:kind==2?(car.position-obstacle->position)>>4:
            kind==1?(obstacle->lateral-car.lateral)>>12:(car.lateral-obstacle->lateral)>>12;
        // Lane changes past a slower obstacle (iVar7 * 3 / 2) are not ported.
        if(gap<window) {
            const int32_t speed=kind==0?obstacle->velocity:kind==2?-obstacle->velocity:
                                kind==1?obstacle->lateral_velocity:-obstacle->lateral_velocity;
            target=follow_speed(kind,car.position,car.lateral,current,cruise,0,obstacle->position,obstacle->lateral,
                                speed_units(speed),300,window,-gap_unit);
            following=true;
        }
    }
    // Signal: stop line and phase.
    if(car.signal>=0 && target>=1) {
        const auto& s=signals_[size_t(car.signal)];
        if(!s.active || s.controller<0 || !controllers_[size_t(s.controller)].active)car.signal=-1;
        else {
            const auto& c=controllers_[size_t(s.controller)];
            int32_t line,offset;   // iVar11 (distance to the stop line), local_10
            if(kind==0) {
                const int32_t stop=c.start_q8+0xa0;
                offset=(s.position_q8-stop)+car_half_length;line=((stop-car.position)-car_half_length)>>4;
            } else if(kind==2) {
                const int32_t stop=c.end_q8-0xa0;
                offset=(stop-s.position_q8)+car_half_length;line=((car.position-stop)-car_half_length)>>4;
            } else if(kind==1) {
                const int32_t edge=at.offset-at.path.left-0x60;
                offset=(crossing_half_width>>8)+(s.lateral_q8>>8)-edge;
                line=(edge*256-car.lateral-crossing_half_width)>>12;
            } else {
                const int32_t edge=at.offset+at.path.right+0x60;
                offset=(crossing_half_width>>8)-(s.lateral_q8>>8)+edge;
                line=(car.lateral-edge*256-crossing_half_width)>>12;
            }
            if(!(kind&1) && line*0x1000<target*200) {
                const size_t ss=size_t(std::clamp(s.position_q8,int32_t(0),int32_t((route.size()-1)*256))>>8);
                if(traffic_lane_count(kind==0?route[ss].path.right_q8:route[ss].path.left_q8)<1)target=0;
            }
            if(target>0 && line<=gap) {
                offset>>=4;
                if(line<-6 && line< -offset)car.signal=-1;   // past the light
                else if(s.phase==1 || s.phase==2) {
                    int32_t yellow=near;if(s.phase==2)yellow=-6;
                    if(window<0x1a)window=0x19;
                    if(yellow<line) {
                        if(line<window) {
                            target=0;
                            if(line>0) {
                                target=follow_speed(kind,car.position,car.lateral,current,cruise,0,s.position_q8,s.lateral_q8,0,300,window,-offset);
                                while(line*0x1000<target*100)target/=2;
                            }
                        }
                    } else if(!following)hurry=true;
                }
            }
        }
    }
    if(target<0)target=0;
    if(hurry)target*=2;
    const int32_t rate=(car.phase*((current<target/2 || hurry)?0x24:0x12))>>8;
    return {target*100,rate};
}

void TrafficSystem::move(TrafficCar& car,const std::vector<RouteSample>& route,int32_t camera_q8,int32_t front_q8,
                        int32_t back_q8,int32_t player_q8,int32_t player_lateral,const std::function<uint32_t()>& random) {
    // Despawn outside the ring window [back, front): 0x00437e60 parks the
    // object, which returns it to the free pool.
    if(car.position>=front_q8 || car.position<back_q8) {car.active=false;return;}
    const size_t sample=size_t(std::clamp(car.position,int32_t(0),int32_t((route.size()-1)*256))>>8);
    const RouteSample& at=route.at(sample);
    if(car.kind&1) {
        // 0x0044dc10: a crossing car lives only on intersection slices
        // (marking 0x20..0x22) with its zone data; the split-roadway parks
        // past the other roadway's edge are not ported.
        if((at.path.marking_flags&0x20)==0 || car.zone_end==0 || car.zone_lanes==0) {car.active=false;return;}
        // 0x0044e710: collisions only within [-0x400, 0x2400] of the ring camera.
        car.collides=uint32_t(car.position-camera_q8+0x400)<0x2801;
        const auto [target,rate]=plan_speed(car,route);
        // 0x00449880 toward (+-0x32000, 0): heading +-0x400000, speed by
        // 0x004498c0. The car moves straight across (drag neglected as for
        // the other kinds), lateral += speed per tick at 90 degrees.
        car.heading=car.kind==1?0x400000:-0x400000;
        car.speed+=rate;if(target<car.speed)car.speed=target;
        car.lateral=wrap(int64_t(car.lateral)+(car.kind==1?car.speed:-car.speed));
        if(car.lateral< -0xc8000 || car.lateral>0xc8000)car.active=false;   // 0x00437e60 lateral bound
        return;
    }
    // 0x0044e710 for kinds 0/2: the speed target follows obstacles in the
    // lane and stops at red signals; reached at the phase-scaled rate.
    const auto [target,rate]=plan_speed(car,route);
    car.speed+=rate;
    if(target<car.speed)car.speed=target;
    // Position advance from the movement form ((v*2 - a*dt)*dt) >> 9 with the
    // car's surface drag neglected; oncoming cars (kind 2) drive backward.
    car.position=wrap(int64_t(car.position)+int64_t(car.speed>>8)*(car.kind==2?-1:1));
    // Lateral steering toward the lane centre of the car's current sample
    // (0x0044e710 +0x1dc target, drift rate from 0x00475694[kind] * 800).
    const size_t moved=size_t(std::clamp(car.position,int32_t(0),int32_t((route.size()-1)*256))>>8);
    const RouteSample& now=route.at(moved);
    const int32_t lane_target=wrap(int64_t(now.offset+car.lane*250+125)*256);
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

void TrafficSystem::build_slice(const std::vector<RouteSample>& route,int32_t sample,int32_t back_q8,int32_t front_q8,uint32_t tick) {
    // 0x00454c80 -> 0x00454240 with the forward traversal: each roadway of
    // the slice (own half first) whose event ends here calls 0x0041ec70:
    // 0x0041eca0 (types 1..3), then the density 0x0044dd00.
    if(sample<0 || size_t(sample)>=route.size())return;
    const auto& at=route[size_t(sample)];
    auto zone=[&](const PathSample& path) {
        if(!path.zone_end)return;
        if(path.event_type>=1 && path.event_type<=3)place_intersection(route,sample,path,back_q8,front_q8,tick);
        traffic_set_density(timers_,int8_t(path.event_parameters[1]));
        ++zone_events_;
    };
    zone(at.path);
    if(at.dual)zone(at.other);
}

int TrafficSystem::place_signal(const std::vector<RouteSample>& route,int controller,int index,int side,
                                int32_t fraction,int32_t slice,int lane) {
    // 0x0041ef60: type 0x40 << index (template word written to 0x00468f34),
    // at the corner slice + fraction, laterally in the middle of the side's
    // shoulder interpolated toward the next slice by the same fraction
    // (the original follows the sub-slice link +8/+0xc by side; the port
    // always uses the next route sample).
    int si=-1;
    for(size_t i=0;i<signals_.size();++i)if(!signals_[i].active){si=int(i);break;}
    if(si<0)return -1;
    const size_t s=size_t(std::clamp(slice,int32_t(0),int32_t(route.size()-1)));
    const size_t n=std::min(s+1,route.size()-1);
    auto middle=[&](const RouteSample& r) {
        return side ? (2*r.offset+2*r.path.right+r.path.shoulder_right)/2 : (2*r.offset-2*r.path.left-r.path.shoulder_left)/2;
    };
    const int32_t here=middle(route[s]),next=middle(route[n]);
    auto& g=signals_[size_t(si)];
    g=IntersectionSignal{};
    g.active=true;g.index=index;g.side=side;g.controller=controller;g.lane=lane;
    g.phase=(~index&1)*2;g.accum=0x30c;
    g.position_q8=int32_t(s)*256+fraction;
    g.lateral_q8=div256((next*256-here*256)*fraction)+here*256;
    return si;
}

void TrafficSystem::place_intersection(const std::vector<RouteSample>& route,int32_t s,const PathSample& path,
                                       int32_t back_q8,int32_t front_q8,uint32_t tick) {
    // 0x0041eca0. s is the zone-end sample; the event runs from
    // event_start to event_end in section-local samples.
    const int variant=path.event_type;
    const int32_t span=path.event_end-path.event_start;
    const int32_t start=std::max<int32_t>(0,s-span),last=std::max<int32_t>(0,s-1);
    const auto& first=route[size_t(start)];const auto& before=route[size_t(last)];
    const bool split_hills=route[size_t(s)].dual && terrain_type_ && terrain_type_(route[size_t(s)])==4;   // bVar1
    const bool quiet=path.event_parameters[2]==0;                                                        // bVar13
    const int rear=int8_t(span-2);                                                                         // cVar2
    const int across=int8_t(traffic_lane_count(first.path.right_q8)+traffic_lane_count(first.path.left_q8));   // cVar3
    int ci=-1;
    for(size_t i=0;i<controllers_.size();++i)if(!controllers_[i].active){ci=int(i);break;}
    if(ci<0)return;   // 0x00438930 returned null
    auto& c=controllers_[size_t(ci)];
    c=IntersectionController{};
    c.active=true;
    int32_t position=s*256;
    if(front_q8<position)position=front_q8; else if(position<back_q8)position=back_q8;
    c.position_q8=position&~0xff;
    c.start_q8=start*256;c.end_q8=s*256;
    c.variant=variant;c.lanes=int8_t(path.event_parameters[0]);
    if(split_hills && quiet){c.durations[0]=0x4b;c.durations[1]=0;c.durations[2]=0xf0;}
    c.last_tick=int32_t(tick)-0x1e;c.next_tick=int32_t(tick);   // 0x00438930: +0x10 = tick - interval, +0xc = tick
    // Creation order and the null-goto: a failed signal skips the rest.
    if(traffic_lane_count(first.path.right_q8)>0 && (c.signals[0]=place_signal(route,ci,0,1,0x50,last,rear))<0)return;
    if(traffic_lane_count(before.path.left_q8)>0 && (c.signals[2]=place_signal(route,ci,2,0,0xb0,start,rear))<0)return;
    if(split_hills)return;
    if((variant==1 || variant==2) && (c.signals[3]=place_signal(route,ci,3,0,0x50,last,across))<0)return;
    if(variant==1 || variant==3)c.signals[1]=place_signal(route,ci,1,1,0xb0,start,across);
}

int TrafficSystem::pedestrian_signal(uint8_t marking_flags,int32_t position_q8,int lane) const {
    if((marking_flags&0x18)==0)return -1;
    const int want=lane>=0?0:2;
    const IntersectionSignal* ahead=nullptr;const IntersectionSignal* behind=nullptr;
    for(const auto& s:signals_) {
        if(!s.active || s.index!=want)continue;
        const int32_t d=s.position_q8-position_q8;
        if(d>0 && d<0x1e0 && (!ahead || d<ahead->position_q8-position_q8))ahead=&s;
        if(d<=0 && -d<0x1e0 && (!behind || -d<position_q8-behind->position_q8))behind=&s;
    }
    const IntersectionSignal* chosen=ahead;
    if(behind && (!ahead || position_q8-behind->position_q8<=ahead->position_q8-position_q8))chosen=behind;
    if(!chosen || std::abs(chosen->position_q8-position_q8)>=0x801)return -1;
    return chosen->phase;
}

void TrafficSystem::think_intersections(int32_t back_q8,int32_t front_q8,uint32_t tick,
                                        const std::vector<RouteSample>& route,
                                        const std::function<uint32_t()>& random) {
    // Controllers in pool order (the original walks them in ring order).
    for(size_t ci=0;ci<controllers_.size();++ci) {
        auto& c=controllers_[ci];
        if(!c.active || c.next_tick>int32_t(tick))continue;
        const int32_t dt=int32_t(tick)-c.last_tick;   // +0x18
        c.last_tick=int32_t(tick);c.next_tick=int32_t(tick)+0x1e;
        for(int slot=0;slot<4;++slot) {
            const int si=c.signals[slot];if(si<0)continue;
            auto& s=signals_[size_t(si)];
            s.accum+=dt;
            if(!(back_q8+0x100<=s.position_q8 && s.position_q8<=front_q8-0x200 && c.durations[s.phase]<s.accum))continue;
            do {
                s.accum-=c.durations[s.phase];
                if(++s.phase>2)s.phase=0;
                if((s.index&1) && s.phase==0 && (tick&8)) {
                    int lane=0,kind,range;
                    if(c.variant==1) {range=int8_t(c.lanes)/2;kind=s.index;if(s.index==3)lane=range;}
                    else {kind=c.variant==2?3:1;range=c.lanes;}
                    if(range>0) {   // a zero range divides by zero in the original
                        lane=int8_t(lane+int32_t(random()%uint32_t(range)));
                        place_crossing(c,si,kind,lane,route,random);
                    }
                }
            } while(c.durations[s.phase]<s.accum);
        }
        // 0x00437e60 window test after the think: 0x0041f1f0 parks the
        // signals and the controller outside [back, front].
        if(front_q8-c.position_q8<0 || c.position_q8-back_q8<0) {
            for(int k=0;k<4;++k)if(c.signals[k]>=0)signals_[size_t(c.signals[k])]=IntersectionSignal{};
            for(auto& car:cars_)if(car.signal>=0 && signals_[size_t(car.signal)].active==false)car.signal=-1;
            c=IntersectionController{};
        }
    }
}

void TrafficSystem::place_crossing(const IntersectionController& c,int signal,int kind,int lane,
                                   const std::vector<RouteSample>& route,const std::function<uint32_t()>& random) {
    // 0x0044df60 with an odd kind on the slice (lane + 1) samples into the
    // zone: lateral +-0x78000 plus the slice centre (world units, added
    // raw as the original does), fraction 0x80; the cross-road vehicle
    // class (mask 0x40000) and the shared 16-car pool.
    const int32_t s=std::clamp((lane+1)*0x100+c.start_q8,int32_t(0),int32_t((route.size()-1)*256))>>8;
    const auto& at=route[size_t(s)];
    const int vehicle=library_.choose(uint8_t(kind),random());
    if(vehicle<0)return;
    for(auto& car:cars_) {
        if(car.active)continue;
        car=TrafficCar{};
        car.lateral=(kind==3?0x78000:-0x78000)+at.offset;
        car.position=s*256+0x80;
        car.kind=uint8_t(kind);car.vehicle=uint8_t(vehicle);car.lane=int8_t(lane);
        car.phase=(random()&0x7f)+0xc0;
        car.jitter_a=int8_t(random());
        car.jitter_b=int8_t(random());
        car.jitter_c=int8_t(random());
        car.serial=++serial_;
        car.signal=signal;car.zone_start=c.start_q8;car.zone_end=c.end_q8;car.zone_lanes=c.lanes;
        car.heading=kind==1?0x400000:-0x400000;
        car.active=true;
        return;
    }
}

void TrafficSystem::step(const std::vector<RouteSample>& route,int32_t camera_q8,int32_t front_q8,
                        int32_t distance_moved,int32_t player_q8,int32_t player_lateral,const std::function<uint32_t()>& random,
                        uint32_t tick) {
    const int32_t back_q8=front_q8-0x8000;    // ring span 0x8000 (0x004552e0)
    // Ring slices built this step (0x00454120 runs before 0x0044dd90).
    const int32_t front_sample=front_q8>>8;
    if(!filled_) {
        filled_=true;
        built_end_=(front_q8-distance_moved)>>8;
        for(int32_t s=built_end_-128;s<built_end_;++s)build_slice(route,s,back_q8,front_q8,tick);
    }
    for(;built_end_<front_sample;++built_end_)build_slice(route,built_end_,back_q8,front_q8,tick);
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
    // 0x0041f040 intersection think runs in the 0x00438170 scheduler pass,
    // after the 0x0044dd90 spawns and before the 0x0044e710 car moves.
    think_intersections(back_q8,front_q8,tick,route,random);
    for(auto& car:cars_)if(car.active)move(car,route,camera_q8,front_q8,back_q8,player_q8,player_lateral,random);
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
    const Camera& camera,const DepthTable& depths,int32_t camera_q8,int32_t ceiling,int32_t logical_width) {
    if(vehicle.playback.size()!=3)return std::nullopt;
    const int64_t distance=std::abs(int64_t(car.position)-camera_q8);
    const size_t code=distance>0xa00 && vehicle.playback[2]?2:distance>0x500 && vehicle.playback[1]?1:0;
    const auto& sequence=vehicle.playback[code];if(!sequence || sequence->empty())return std::nullopt;
    const auto& a=near.road[road];const auto& b=far.road[road];
    ActorOrientation orientation;orientation.depth=near.depth;orientation.position=car.position;orientation.road_x=a.road_x+camera.x;orientation.lateral_q8=car.lateral-sample.offset*256;orientation.road_heading=a.heading;orientation.heading=car.heading;
    const auto angles=actor_angles(orientation,camera.x);
    const auto selected=select_actor_view(sequence->front(),vehicle.views,angles.yaw,angles.roll);
    const auto& cell=vehicle.views.at(selected.group).at(selected.subframe);const auto& frame=vehicle.animation.frames.at(cell.frame);
    ActorProjection projection;projection.road_x=orientation.road_x;projection.far_x=b.road_x+camera.x;projection.road_y=a.world_y;projection.far_y=b.world_y;projection.lateral=orientation.lateral_q8/256;projection.depth=near.depth;projection.fraction=uint8_t(car.position);projection.size=0x500;projection.flags=cell.flags;projection.occlusion=ceiling;
    return actor_node(frame,frame.texture.get(),projection,camera,depths,logical_width);
}
std::vector<DrawNode> traffic_draw_nodes(const TrafficSystem& system,
    const std::vector<RouteSample>& route,const std::vector<RoadSlice>& slices,
    const Camera& camera,const DepthTable& depths,int32_t camera_q8,uint32_t tick,int32_t logical_width) {
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
        const int32_t anchor_x=wrap(int64_t(v.road_x+(v.car->lateral>>8))*projection+(logical_width&~1)*32768);
        const int32_t x=wrap(int64_t(anchor_x)-int64_t(frame.hotspot[0])*scale);
        const int32_t y=wrap(int64_t(camera.height-v.world_y)*projection-int64_t(frame.hotspot[1])*scale+90*65536);
        const int32_t right=wrap(int64_t(x)+int64_t(width)*scale),bottom=wrap(int64_t(y)+int64_t(height+1)*scale);
        result.push_back({{{{x,y},{right,y},{right,bottom},{x,bottom}}},frame.texture.get(),std::nullopt,false,false});
    }
    (void)route;
    return result;
}

}