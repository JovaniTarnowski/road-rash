#include "road_panels.hpp"
#include <cstring>
#include <stdexcept>
namespace rr {
namespace {
int32_t bits(uint32_t value){int32_t result;std::memcpy(&result,&value,4);return result;}
int32_t add(int32_t a,int32_t b){return bits(uint32_t(a)+uint32_t(b));}
int32_t sub(int32_t a,int32_t b){return bits(uint32_t(a)-uint32_t(b));}
int32_t truncate_shift8(int32_t value){return value/256;}
RoadPanelMaterial material(const RoadPanelInput& in,int32_t panel) {
    if(in.surface_flags&0x20)return RoadPanelMaterial::plain;
    if(panel>0) {
        const int32_t left=truncate_shift8(add(in.left_lanes_q8,255));
        if(panel==left)panel=2;
        else if((uint32_t(in.left_lanes_q8)&255)==0 || panel>1)
            panel=(uint32_t(in.right_lanes_q8)&255)==0?1:int32_t(panel<add(truncate_shift8(in.right_lanes_q8),left));
        else panel=0;
    }
    const auto flags=in.marking_flags;
    if(flags && !(flags&0x40)) {
        if(flags&0x18)return RoadPanelMaterial(int(RoadPanelMaterial::marked0)+panel);
        if(flags&0x20)return RoadPanelMaterial::alternate;
    }
    return RoadPanelMaterial(int(RoadPanelMaterial::normal0)+panel);
}
}
std::vector<RoadPanel> road_panels(const RoadPanelInput& in,int32_t width) {
    if(width<=0 || width>32767)throw std::runtime_error("Invalid road panel view width");
    std::vector<RoadPanel> result;if(!in.visible)return result;
    int32_t first=in.first_left,second=in.second_left;
    int32_t first_step=in.first_lane_width,second_step=in.second_lane_width;
    int32_t first_y=bits(uint32_t(in.first_y)&0xffffc000u),second_y=bits(uint32_t(in.second_y)&0xffffc000u);
    if(in.marking_flags&0x10){std::swap(first,second);std::swap(first_step,second_step);std::swap(first_y,second_y);}
    int32_t first_right=add(first,first_step),second_right=add(second,second_step);
    first=sub(first,0x40000);second=sub(second,0x40000);
    const int count=int(in.left_lane_count)+int(in.right_lane_count)-1;
    const uint32_t pre0=in.terrain_type==3 && in.next_terrain_type==3?0x18u:0x1f001f00u;
    auto append=[&](int index){
        if((first<width*65536 && first_right>0)||(second<width*65536 && second_right>0)) {
            RoadPanel panel;panel.points={{{first,first_y},{first_right,first_y},{second_right,second_y},{second,second_y}}};
            panel.material=material(in,index);panel.pre0=pre0;
            panel.flip_u=first_right<first || second_right<second;panel.flip_v=second_y<first_y;
            result.push_back(panel);
        }
    };
    for(int i=0;i<count;++i){append(i);first=first_right;second=second_right;first_right=add(first_right,first_step);second_right=add(second_right,second_step);}
    if(count>=0){first_right=add(first_right,0x40000);second_right=add(second_right,0x40000);append(count);}
    return result;
}
}
