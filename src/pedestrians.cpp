#include "pedestrians.hpp"
#include "actor_render.hpp"
#include "exe_tables.hpp"
#include <stdexcept>

namespace rr {
PedestrianSystem::PedestrianSystem(ActorSource actors,LookSource looks,std::vector<uint8_t> exe)
    :actors_(std::move(actors)),looks_(std::move(looks)),pool_(pool_size) {
    const auto bytes=read_exe_data(exe,0x0046dde0,100);
    for(size_t i=0;i<template_.size();++i)
        template_[i]=int32_t(uint32_t(bytes[i*4])|uint32_t(bytes[i*4+1])<<8|uint32_t(bytes[i*4+2])<<16|uint32_t(bytes[i*4+3])<<24);
    if(template_[1]!=0x20)throw std::runtime_error("Unrecognised pedestrian template");
}
void PedestrianSystem::reset() {
    for(auto& p:pool_)p=Pedestrian{};
    consumed_.clear();
    filled=false;built_back=built_front=0;spawned_=0;serial_=0;
}
bool PedestrianSystem::consume(uint32_t section,uint32_t local,size_t index) {
    return consumed_.emplace(section,local,index).second;
}
void PedestrianSystem::apply_template(Bike& b) const {
    // 0x00438930: +0xc0 byte, +0xc4 type, +0x14 first interval; 0x004387a0:
    // template words +0x1c.. to the object words listed below.
    b.u8(0xc0)=uint8_t((template_[0]&~8)|4); // 0x00437bb0 inserts the scheduler node
    b.i32(0xc4)=template_[1];
    b.i32(0x14)=template_[2];
    static constexpr std::array<std::pair<int,int>,18> words{{
        {0x1c,0x148},{0x20,0x14c},{0x24,0x150},{0x28,0x154},{0x2c,0x160},{0x30,0x168},{0x34,0x16c},
        {0x38,0x1b4},{0x3c,0x1b8},{0x40,0x170},{0x44,0x174},{0x48,0x178},{0x4c,0x17c},{0x50,0x180},
        {0x54,0x194},{0x58,0x1a4},{0x5c,0x1a8},{0x60,0x1ac}}};
    for(const auto& [from,to]:words)b.i32(to)=template_[size_t(from/4)];
    b.i32(0x11c)=-0x25;
}

std::optional<DrawNode> pedestrian_node(const Pedestrian& p,const RoadSlice& near,const RoadSlice& far,int road,
    const RouteSample& sample,const Camera& camera,const DepthTable& depths,int32_t ceiling,int32_t logical_width) {
    if(!p.active || !p.look || !p.animation.selected)return std::nullopt;
    const auto selected=*p.animation.selected;
    if(selected[0]<0 || size_t(selected[0])>=p.look->bank.size() || !p.look->bank[size_t(selected[0])])return std::nullopt;
    const auto& sequence=*p.look->bank[size_t(selected[0])];
    if(selected[1]<0 || size_t(selected[1])>=sequence.size())return std::nullopt;
    const Bike& b=p.body.bike;
    const auto& a=near.road[road];const auto& f=far.road[road];
    ActorOrientation orientation;orientation.depth=near.depth;orientation.position=b.i32(0x1c);orientation.road_x=a.road_x+camera.x;
    orientation.lateral_q8=b.i32(0x28)-sample.offset*256;orientation.road_heading=a.heading;orientation.heading=b.i32(0x5c);
    const auto angles=actor_angles(orientation,camera.x);
    const auto view=select_actor_view(sequence[size_t(selected[1])],p.look->views,angles.yaw,angles.roll);
    if(view.group>=p.look->views.size() || view.subframe>=p.look->views[view.group].size())return std::nullopt;
    const auto& cell=p.look->views[view.group][view.subframe];
    if(cell.frame>=p.look->graphics.frames.size())return std::nullopt;
    const auto& frame=p.look->graphics.frames[cell.frame];
    if(!frame.texture)return std::nullopt;
    ActorProjection projection;projection.road_x=orientation.road_x;projection.far_x=f.road_x+camera.x;projection.road_y=a.world_y;projection.far_y=f.world_y;
    projection.lateral=orientation.lateral_q8/256;projection.ground=(b.i32(0x20)>>8)+b.i32(0x24);projection.depth=near.depth;
    projection.fraction=uint8_t(b.i32(0x1c));projection.size=0x200;projection.flags=cell.flags;projection.occlusion=ceiling;
    return actor_node(frame,frame.texture.get(),projection,camera,depths,logical_width);
}
}
