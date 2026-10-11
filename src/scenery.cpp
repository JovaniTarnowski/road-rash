#include "scenery.hpp"
#include <algorithm>
#include <cstring>
namespace rr {
namespace {int32_t wrap(int64_t v){uint32_t bits=uint32_t(v);int32_t result;std::memcpy(&result,&bits,4);return result;}}
std::vector<SceneryObject> roadside_objects(const CourseSection& section,std::function<uint32_t()> random) {
    std::vector<SceneryObject> result;if(section.roadside_profiles.empty())return result;
    size_t record=0;int32_t start=0,end=0;
    auto period=[](uint32_t word){return (word&0x0f000000)?int32_t((word>>12)&15):0;};
    auto duration=[&](uint32_t word){return (period(word)+1)*int32_t((word>>24)&15);};
    end=duration(section.roadside_profiles[0]);
    for(size_t j=0;j<section.samples.size();++j) {
        if(j && int64_t(j)>end) {
            if(record+1>=section.roadside_profiles.size())break;
            ++record;start=int32_t(j);end=start+duration(section.roadside_profiles[record]);
        }
        const uint32_t word=section.roadside_profiles[record];const uint8_t selector=uint8_t(word>>16);
        const int32_t stride=period(word)+1;if((selector&63)==63 || (int32_t(j)-start)%stride)continue;
        bool left=(word&0x10000000)!=0;
        if((word&0x80000000) && ((int32_t(j)-start)&1)==0)left=!left;
        int32_t spacing;
        if(word&0x0f000000)spacing=(word&0x20000000)?int32_t(((word&15)+4)*256):int32_t((word&15)*64);
        else {const int32_t amount=int32_t(((word&0xf000)>>8)+(word&15));spacing=(word&0x20000000)?(amount+32)*32:amount*8;}
        const auto& path=section.samples[j];int32_t x=left?-path.left+224-spacing:path.right-224+spacing;
        const int count=int((word>>8)&7)+1;
        for(int object=0;object<count;++object) {
            result.push_back({uint32_t(j),x,selector,uint8_t((word>>4)&15),0,bool(word&0x40000000),false});
            const int32_t step=(word&0x800)?spacing/2+int32_t(random()%uint32_t(spacing-spacing/2+1)):spacing;
            x+=left?-step:step;
        }
    }
    return result;
}
std::vector<SceneryObject> placed_scenery(const CourseSection& section,int level,std::function<uint32_t()> random) {
    if(level<0 || level>4)throw std::runtime_error("Invalid scenery difficulty");
    struct Event {uint32_t packed,position,details;};std::vector<Event> events;int32_t base=0;
    for(const auto& pair:section.scenery_events) {
        const uint32_t word=pair[0];base+=int32_t((word>>24)&127);
        const uint32_t sample=uint32_t(base)+random()%(((word&0xf0000)>>15)+1);
        bool enabled=true;const int required=int((pair[1]>>12)&7);
        if(required && level-required!=-1)enabled=false;
        const int probability=int((word>>20)&15)+((word&0x80000000) && (word&15)>=11?level-4:level);
        if(enabled && probability<int(random()%10))enabled=false;
        events.push_back({enabled?word:word|0x3f00,sample,pair[1]});
    }
    // 0x00451fc0 uses pairwise swaps; equal positions can change order as
    // other entries move through them, so a stable sort is not equivalent.
    for(size_t i=0;i<events.size();++i)for(size_t j=i+1;j<events.size();++j)if(events[j].position<events[i].position)std::swap(events[i],events[j]);
    std::vector<SceneryObject> result;
    for(const auto& event:events) {
        const auto word=event.packed;if((uint8_t(word>>8)&63)==63 || (word&0x80000000))continue;
        if(event.position>=section.samples.size())continue;
        if(std::count_if(result.begin(),result.end(),[&](const SceneryObject& o){return o.sample==event.position;})>=2)continue;
        const auto& path=section.samples[event.position];
        const int32_t displacement=(event.details&0x800)?int32_t(event.details&0xfff)-4096:int32_t(event.details&0xfff);
        result.push_back({event.position,(path.right-path.left)/2+displacement,uint8_t(word>>8),8,uint8_t(word&15),bool(word&0x80),true});
    }
    return result;
}
DrawNode scenery_node(const AnimationFrame& frame,const SceneryObject& object,int32_t road_x,
    int32_t road_y,int32_t ground_y,int32_t depth,const Camera& camera,const DepthTable& depths) {
    if(!frame.texture)return {};
    const uint32_t index=(uint32_t(depth+128)&~1u)/2;
    if(index>=depths.scale.size())throw std::runtime_error("Scenery depth outside table");
    const int32_t projection=depths.scale[index],scale=wrap(int64_t(object.size)*projection)>>2;
    const auto& bounds=frame.texture->bounds;const int32_t width=bounds[2]-bounds[0],height=bounds[3]-bounds[1];
    const int32_t anchor_x=wrap(int64_t(road_x+object.lateral-camera.x)*projection+160*65536);
    const int32_t x=wrap(int64_t(anchor_x)+(object.mirror?1:-1)*int64_t(frame.hotspot[0])*scale);
    const int32_t y=wrap(int64_t(camera.height-road_y-ground_y)*projection-int64_t(frame.hotspot[1])*scale+90*65536);
    const int32_t right=wrap(int64_t(x)+(object.mirror?-1:1)*int64_t(width)*scale),bottom=wrap(int64_t(y)+int64_t(height+1)*scale);
    return {{{{x,y},{right,y},{right,bottom},{x,bottom}}},frame.texture.get(),std::nullopt,object.mirror,false};
}
}
