#include "animation.hpp"
#include <cstring>
#include <stdexcept>
namespace rr {
Animation read_animation(const std::vector<uint8_t>& bytes,uint32_t offset) {
    auto range=[&](size_t p,size_t n){if(p>bytes.size() || n>bytes.size()-p)throw std::runtime_error("Truncated animation resource at "+std::to_string(p)+" + "+std::to_string(n)+" / "+std::to_string(bytes.size()));};
    auto word=[&](size_t p){range(p,4);return uint32_t(bytes[p])|uint32_t(bytes[p+1])<<8|uint32_t(bytes[p+2])<<16|uint32_t(bytes[p+3])<<24;};
    auto signed_word=[&](size_t p){uint32_t v=word(p);int32_t result;std::memcpy(&result,&v,4);return result;};
    auto pointer=[&](size_t p,size_t n){const uint64_t target=uint64_t(offset)+word(p);if(target>UINT32_MAX)throw std::runtime_error("Animation offset overflow");range(size_t(target),n);return size_t(target);};
    range(offset,28);
    if(word(offset)!=0x5252414e || word(offset+4)!=20 || word(offset+20)!=0x52524644)throw std::runtime_error("Invalid RRAN/RRFD header");
    const auto info=pointer(offset+8,20);
    if(word(info)!=0x414e494d || word(info+4)<20)throw std::runtime_error("Invalid animation info");
    range(info,word(info+4));
    const uint32_t count=word(info+16);
    if(!count || count>(bytes.size()-offset-28)/28 || uint64_t(count)*28+8!=word(offset+24))throw std::runtime_error("Invalid animation frame count");
    const auto ccb=pointer(offset+12,80);
    if(word(ccb)!=0x43434220)throw std::runtime_error("Invalid animation CCB template");
    Animation result;
    for(uint32_t i=0;i<count;++i) {
        const size_t frame=size_t(offset)+28+size_t(i)*28;AnimationFrame value;
        if(word(frame))pointer(frame,68);
        if(word(frame+8)){const size_t spot=pointer(frame+8,8);value.hotspot={signed_word(spot),signed_word(spot+4)};}
        value.flags=word(frame+16);value.width=uint16_t(word(frame+20));value.height=uint16_t(word(frame+24));
        value.collision_reference=word(frame+12);
        // Some original files retain a stale absolute collision address in
        // frames whose collision flag is zero. The drawing path never uses it.
        if(value.flags && value.collision_reference){const size_t collision=pointer(frame+12,16);for(int j=0;j<4;++j)value.collision[j]=signed_word(collision+j*4);}
        if(word(frame+4)) {
            const size_t body=pointer(frame+4,12)+8;
            if(word(body)==0xbc614e)value.texture=std::make_shared<CelTexture>(read_indexed_texture(bytes.data()+body,bytes.size()-body));
            else value.legacy_pixel_word=word(body);
        }
        result.frames.push_back(std::move(value));
    }
    if(count==3 && !result.frames[0].texture) {
        result.frames[0].texture=result.frames[1].texture=result.frames[2].texture;
        result.frames[0].legacy_pixel_word=result.frames[1].legacy_pixel_word=result.frames[2].legacy_pixel_word;
    } else if(result.frames[0].texture && (result.frames[0].texture->flags&1)) {
        for(size_t i=0;i+2<result.frames.size();i+=3) {
            result.frames[i+1].texture=result.frames[i+2].texture=result.frames[i].texture;
            result.frames[i+1].legacy_pixel_word=result.frames[i+2].legacy_pixel_word=result.frames[i].legacy_pixel_word;
        }
    }
    return result;
}
}
