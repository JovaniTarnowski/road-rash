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
    const std::string dash=metric?"KHIDASH.BOB":"MHIDASH.BOB",dash_low=metric?"KLODASH.BOB":"MLODASH.BOB";
    dashboard_=assets.read("IMAGES/BIKES/"+prefix+"/"+dash);
    dashboard_low_=assets.read("IMAGES/BIKES/"+prefix+"/"+dash_low);
    meter_=assets.read("IMAGES/BIGMETER.BOB");
    meter_low_=assets.read("IMAGES/LILMETER.BOB");
    // 0x0044c5f0 returns bytes-read / width; 122 is a maximum,
    // while the RAT artwork on this disc contains 119 complete rows.
    // Low dashboards are 320-wide (RAT/SUPER 60 rows, SPORT 61); LILMETER is
    // the 95-stride half of the 190-stride BIGMETER pair (0x004457f0 low path).
    if(dashboard_.empty() || dashboard_.size()%640 || dashboard_.size()>640*122 || meter_.size()!=190*64)throw std::runtime_error("Invalid dashboard artwork");
    if(dashboard_low_.empty() || dashboard_low_.size()%320 || meter_low_.size()!=95*32)throw std::runtime_error("Invalid low-mode dashboard artwork");
    const auto read_stride=[&](int index) {
        const auto data=read_exe_data(exe,0x00474f50+uint32_t(index)*0xdc,0xdc);
        std::array<int32_t,55> out{};
        for(size_t i=0;i<out.size();++i){uint32_t v=0;for(int j=0;j<4;++j)v|=uint32_t(data[i*4+j])<<(8*j);std::memcpy(&out[i],&v,4);}
        return out;
    };
    // 0x00446c30 indexes layout (MiniDash + HiRes*2)*0xdc at 0x00474f50:
    // strides 0/1 are the low (320-wide) full/mini rows, 2/3 the high rows.
    layout_=read_stride(2);
    layout_mini_=read_stride(3);
}
int Hud::cycle_mini_dash(int mode) {
    // F10 key-up in the 0x004431d0 pump: 0->1->2->0.
    if(mode<0 || mode>2)return 0;
    return (mode+1)%3;
}
void Hud::set_low_mode(bool low) { low_=low; }
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
    // 0x00446a40 reads +0x44, the per-frame snapshot of +0x1c..+0x2c that
    // 0x00438450 copies into +0x44..+0x54 for objects in the draw window (the
    // player always is). It is the route position: distance travelled in
    // tenths of a mile (33 samples) or km, 0.0 at the grid (sample 50).
    const int32_t position=b.i32(0x1c);
    s.distance=metric_?((position>>5)-400)/165:((position>>8)-50)/33;
    s.distance=int(uint8_t(s.distance));
    s.stamina=meter_level(race.riders()[0].stamina.current,race.riders()[0].stamina.base);
    s.condition=meter_level(b.i32(0x2f0),b.i32(0x2ec));
    if(race.last_target()>0){const auto& rider=race.riders().at(size_t(race.last_target()));s.target_stamina=meter_level(rider.stamina.current,rider.stamina.base);}
    return s;
}
RgbImage Hud::compose(const HudState& state,const PhysicsTables& physics,DashMode mode) const {
    RgbImage image(640,480,transparent);
    // 0x00446c30 draws nothing at all when MiniDash == 2 (mode off).
    if(mode == DashMode::off)return image;
    const std::vector<uint8_t>& dash_art = low_ ? dashboard_low_ : dashboard_;
    const std::vector<uint8_t>& meter_art = low_ ? meter_low_ : meter_;
    const int stride = low_ ? 320 : 640, meter_stride = low_ ? 95 : 190;
    const std::array<int32_t,55>& layout = mode == DashMode::mini ? layout_mini_ : layout_;
    const float scale = low_ ? 0.5f : 1.f;
    auto blit=[&](const std::vector<uint8_t>& source,int src_stride,int left,int top,int right,int bottom,int x,int y,bool keyed=true) {
        const int dx = low_ ? x / 2 : x, dy = low_ ? y / 2 : y;
        for(int row=top;row<bottom;++row)for(int col=left;col<right;++col) {
            if(row<0 || col<0 || col>=src_stride || size_t(row)*src_stride+col>=source.size())continue;
            const uint8_t index=source[size_t(row)*src_stride+col];if(!index && keyed)continue;
            const auto* rgb=&palette_.rgbx[4*index];
            const uint32_t pixel=index?uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2]:0;
            if(!low_) { if(image.contains(dx+col-left,dy+row-top))image.at(dx+col-left,dy+row-top)=pixel; }
            else for(int yy=0;yy<2;++yy)for(int xx=0;xx<2;++xx)if(image.contains(2*(dx+col-left)+xx,2*(dy+row-top)+yy))image.at(2*(dx+col-left)+xx,2*(dy+row-top)+yy)=pixel;
        }
    };
    auto bar=[&](int value,int offset) {
        // 0x0044ca50 (flags 0x10). Vertical meters: full colour column
        // {11,0,62,31}, then the black {11,32,62,63-value} empties it from the top.
        const int type=layout[offset],x=int(layout[offset+1]*scale),y=int(layout[offset+2]*scale);value=std::clamp(value,0,31);
        if(!type){blit(meter_art,meter_stride,62,16,126,34,x,y,false);blit(meter_art,meter_stride,62,0,62+value*2,16,x+1,y+1,false);}
        else {blit(meter_art,meter_stride,11,0,62,31,x,y,false);blit(meter_art,meter_stride,11,32,62,63-value,x,y,false);}
    };
    // 0x00446880/0x004468e0 skip tach/speedo math when MiniDash is nonzero;
    // in mini mode (1) only the numeric speed readout is drawn, no needles/bars.
    if(mode == DashMode::mini) {
        auto label=[&](const std::string& value,int offset,uint32_t color) {
            TextRect rect{int(layout[offset+1]*scale),int(layout[offset+2]*scale),int(layout[offset+3]*scale),int(layout[offset+4]*scale)};
            auto shadow=rect;shadow.left+=2;shadow.top+=2;shadow.right+=2;shadow.bottom+=2;
            text_.draw_box(&image,Face::badloc,32,shadow,0,value,uint32_t(layout[offset]));
            text_.draw_box(&image,Face::badloc,32,rect,color,value,uint32_t(layout[offset]));
        };
        char speed[32];std::snprintf(speed,sizeof speed,"%d",state.distance);
        label(speed,21,0xffffff);
        return image;
    }
    // 0x00446c30 draws the third (opponent) meter whenever the full dashboard
    // is up (mode 0), empty without a target; the artwork has a hole there.
    bar(state.stamina,40);bar(state.condition,45);bar(state.target.empty()?0:state.target_stamina,50);
    const int dashboard_height=int(dash_art.size()/stride);
    blit(dash_art,stride,0,0,stride,dashboard_height,0,480-dashboard_height);
    auto needle=[&](int angle,int offset) {
        // Native 3x3-block Bresenham, colour 0x721108. The original instead
        // selects CreatePen(PS_SOLID, HiRes?3:2, 0x81172) and issues
        // MoveToEx/LineTo (0x00446c30) — documented, NOT ported (see notes).
        const int32_t sx=sine(physics,uint32_t(angle)),cy=sine(physics,uint32_t(angle+1024));
        const int x=int(layout[offset]*scale),y=int(layout[offset+1]*scale);
        line(image,x,y,x+high_product(sx,int(layout[offset+2]*scale)),y-high_product(cy,int(layout[offset+3]*scale)),0x721108);
    };
    needle(state.rpm_angle,16);needle(state.speed_angle,21);
    auto label=[&](const std::string& value,int offset,uint32_t color) {
        TextRect rect{int(layout[offset+1]*scale),int(layout[offset+2]*scale),int(layout[offset+3]*scale),int(layout[offset+4]*scale)};
        auto shadow=rect;shadow.left+=2;shadow.top+=2;shadow.right+=2;shadow.bottom+=2;
        text_.draw_box(&image,Face::badloc,32,shadow,0,value,uint32_t(layout[offset]));
        text_.draw_box(&image,Face::badloc,32,rect,color,value,uint32_t(layout[offset]));
    };
    label(state.player,0,0xffff00);label(std::to_string(state.place),25,0xffffff);
    char distance[32];std::snprintf(distance,sizeof distance,"%02d.%d",state.distance/10,state.distance%10);label(distance,30,0xffffff);
    if(!state.target.empty())label(state.target,5,0xffffff);
    return image;
}
}
