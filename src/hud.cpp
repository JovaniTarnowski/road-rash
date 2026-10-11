#include "hud.hpp"
#include "exe_tables.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace rr {
namespace {
constexpr uint32_t transparent=0x01000000;
int32_t high_product(int32_t a,int32_t b){return int32_t((int64_t(a)*b)>>32);}
void line(RgbImage& out,int x,int y,int end_x,int end_y,uint32_t color) {
    const int dx=std::abs(end_x-x),sx=x<end_x?1:-1,dy=-std::abs(end_y-y),sy=y<end_y?1:-1;
    int error=dx+dy;
    for(int guard=0;guard<2048;++guard) {
        for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx)if(out.contains(xx,yy))out.at(xx,yy)=color;
        if(x==end_x && y==end_y)return;
        const int twice=2*error;if(twice>=dy){error+=dy;x+=sx;}if(twice<=dx){error+=dx;y+=sy;}
    }
}
}
Hud::Hud(const Assets& assets,const std::vector<uint8_t>& exe,int bike,bool metric)
    :metric_(metric),palette_(Palette::from_raw(assets.read("DATA/PALETTE.RAW"))),text_(assets) {
    if(bike<0 || bike>=15)throw std::runtime_error("Invalid dashboard bike");
    const std::string prefix=bike<5?"RAT":bike<10?"SUPER":"SPORT";
    dashboard_=assets.read("IMAGES/BIKES/"+prefix+"/"+(metric?"KHIDASH.BOB":"MHIDASH.BOB"));
    meter_=assets.read("IMAGES/BIGMETER.BOB");
    // 0x0044c5f0 returns bytes-read / width; 122 is a maximum,
    // while the RAT artwork on this disc contains 119 complete rows.
    if(dashboard_.empty() || dashboard_.size()%640 || dashboard_.size()>640*122 || meter_.size()!=190*64)throw std::runtime_error("Invalid dashboard artwork");
    const auto data=read_exe_data(exe,0x00474f50+2*0xdc,0xdc);
    for(size_t i=0;i<layout_.size();++i){uint32_t v=0;for(int j=0;j<4;++j)v|=uint32_t(data[i*4+j])<<(8*j);std::memcpy(&layout_[i],&v,4);}
}
int Hud::meter_level(int current,int base) {
    // 0x00446b00 / 0x00446b40. The original byte value is 0..31.
    if(current<=0)return 0;
    if(base<=1)return 31;
    return std::clamp(int(((int64_t(current)*2-2)*15)/(base-1)+1),0,31);
}
HudState Hud::state(const Race& race,const std::string& player,const std::string& target) const {
    HudState s;s.player=player;s.target=target;s.place=race.place(0);
    const auto& b=race.bike();const uint32_t velocity=uint32_t(b.i32(0xec));
    const uint32_t sum=uint32_t(int32_t(velocity&0xfffff001u)>>1)+(velocity&0xffffff80u)*16+(velocity&0xfffffe00u)*4;
    int32_t signed_sum;std::memcpy(&signed_sum,&sum,4);
    s.speed_angle=signed_sum/200-1024;if(s.speed_angle<0)s.speed_angle+=4096;
    s.rpm_angle=b.i32(0x2dc)/44-1024;if(s.rpm_angle<0)s.rpm_angle+=4096;
    s.distance=metric_?((b.i32(0x44)>>5)-400)/165:((b.i32(0x44)>>8)-50)/33;
    s.distance=int(uint8_t(s.distance));
    s.stamina=meter_level(race.riders()[0].stamina.current,race.riders()[0].stamina.base);
    s.condition=meter_level(b.i32(0x2f0),b.i32(0x2ec));
    if(race.last_target()>0){const auto& rider=race.riders().at(size_t(race.last_target()));s.target_stamina=meter_level(rider.stamina.current,rider.stamina.base);}
    return s;
}
RgbImage Hud::compose(const HudState& state,const PhysicsTables& physics) const {
    RgbImage image(640,480,transparent);
    auto blit=[&](const std::vector<uint8_t>& source,int stride,int left,int top,int right,int bottom,int x,int y) {
        for(int row=top;row<bottom;++row)for(int col=left;col<right;++col) {
            if(row<0 || col<0 || col>=stride || size_t(row)*stride+col>=source.size() || !image.contains(x+col-left,y+row-top))continue;
            const uint8_t index=source[size_t(row)*stride+col];if(!index)continue;
            const auto* rgb=&palette_.rgbx[4*index];image.at(x+col-left,y+row-top)=uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2];
        }
    };
    auto bar=[&](int value,int offset) {
        const int type=layout_[offset],x=layout_[offset+1],y=layout_[offset+2];value=std::clamp(value,0,31);
        if(!type){blit(meter_,190,62,16,126,34,x,y);blit(meter_,190,62,0,62+value*2,16,x+1,y+1);}
        else {blit(meter_,190,11,32,62,63-value,x,y);blit(meter_,190,11,0,62,31,x,y);}
    };
    bar(state.stamina,40);bar(state.condition,45);if(!state.target.empty())bar(state.target_stamina,50);
    const int dashboard_height=int(dashboard_.size()/640);
    blit(dashboard_,640,0,0,640,dashboard_height,0,480-dashboard_height);
    auto needle=[&](int angle,int offset) {
        const int32_t sx=sine(physics,uint32_t(angle)),cy=sine(physics,uint32_t(angle+1024));
        const int x=layout_[offset],y=layout_[offset+1];
        line(image,x,y,x+high_product(sx,layout_[offset+2]),y-high_product(cy,layout_[offset+3]),0x721108);
    };
    needle(state.rpm_angle,16);needle(state.speed_angle,21);
    auto label=[&](const std::string& value,int offset,uint32_t color) {
        TextRect rect{layout_[offset+1],layout_[offset+2],layout_[offset+3],layout_[offset+4]};
        auto shadow=rect;shadow.left+=2;shadow.top+=2;shadow.right+=2;shadow.bottom+=2;
        text_.draw_box(&image,Face::badloc,32,shadow,0,value,uint32_t(layout_[offset]));
        text_.draw_box(&image,Face::badloc,32,rect,color,value,uint32_t(layout_[offset]));
    };
    label(state.player,0,0xffff00);label(std::to_string(state.place),25,0xffffff);
    char distance[32];std::snprintf(distance,sizeof distance,"%02d.%d",state.distance/10,state.distance%10);label(distance,30,0xffffff);
    if(!state.target.empty())label(state.target,5,0xffffff);
    return image;
}
}
