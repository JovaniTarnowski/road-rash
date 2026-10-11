#include "actor_animation.hpp"
#include <stdexcept>
namespace rr {
namespace {
bool available(const ActorAnimationBank& bank,int8_t code) {
    if(code==-1)return true;
    if(code<0 || size_t(code)>=bank.size())return false;
    const auto& sequence=bank[size_t(code)];
    return sequence && !sequence->empty() && sequence->front().drawable;
}
const ActorAnimationFrame* frame_at(const ActorAnimationBank& bank,int8_t code,int8_t index) {
    const int sequence=code==-1?0:code;
    if(sequence<0 || size_t(sequence)>=bank.size())throw std::runtime_error("Actor animation sequence out of range");
    if(!bank[size_t(sequence)]) {
        if(index==0)return nullptr;
        throw std::runtime_error("Actor animation frame in absent sequence");
    }
    const auto& frames=*bank[size_t(sequence)];
    if(index<0 || size_t(index)>=frames.size())throw std::runtime_error("Actor animation frame out of range");
    return &frames[size_t(index)];
}
int8_t signed_byte(uint8_t value){return value<128?int8_t(value):int8_t(int(value)-256);}
}
bool set_animation(ActorAnimationState& state,const ActorAnimationBank& bank,int8_t code,uint32_t tick) {
    if(!available(bank,code))return false;
    state.animation=code;state.frame=0;state.loops=0;
    state.deadline=code==-1?tick+3600u:UINT32_MAX;
    return true;
}
bool queue_animation(ActorAnimationState& state,const ActorAnimationBank& bank,int8_t code) {
    if(!available(bank,code))return false;
    state.queued=code;return true;
}
bool set_animation_with_fallback(ActorAnimationState& state,const ActorAnimationBank& bank,int8_t code,uint32_t tick,const std::vector<int8_t>& fallback) {
    if(set_animation(state,bank,code,tick))return true;
    if(code<0 || size_t(code)>=fallback.size())return false;
    return set_animation(state,bank,fallback[size_t(code)],tick);
}
bool queue_animation_with_fallback(ActorAnimationState& state,const ActorAnimationBank& bank,int8_t code,const std::vector<int8_t>& fallback) {
    if(queue_animation(state,bank,code))return true;
    if(code<0 || size_t(code)>=fallback.size())return false;
    return queue_animation(state,bank,fallback[size_t(code)]);
}
const ActorAnimationFrame* advance_riding_animation(ActorAnimationState& state,const ActorAnimationBank& bank,const std::vector<int8_t>& fallback,const RidingAnimationInput& input,uint32_t tick) {
    auto set=[&](int8_t code){return set_animation_with_fallback(state,bank,code,tick,fallback);};
    const int code=state.animation;
    if(code==2 || code==-1 || code==8 || code==3 || code==4) {
        int8_t selected;
        if(input.speed<300)selected=8;
        else if(input.speed<0x708 && input.throttle>30)selected=4;
        else if(!(input.flags&0x20) || input.boost>99) {
            if(input.lateral_speed < -0x1000 || input.speed>=0x251d)selected=3;
            else selected=2;
        }else selected=2;
        set(selected);
    }
    if(input.player) {
        if(input.wrecked && input.wreck_count<4) {
            if(state.animation==5)queue_animation_with_fallback(state,bank,5,fallback);
            else set(5);
        }
        if(state.animation==0 || state.animation==1)set(2);
    } else {
        if(state.animation==6 && state.loops>2)set(2);
        if(state.animation==2 || state.animation==0 || state.animation==1)
            set(input.camera_distance<0x501?2:input.camera_distance<0xa01?0:1);
    }
    return advance_animation(state,bank,tick);
}
const ActorAnimationFrame* advance_animation(ActorAnimationState& state,const ActorAnimationBank& bank,uint32_t tick) {
    const auto* selected=frame_at(bank,state.animation,state.frame);
    if(selected)state.selected=std::array<int8_t,2>{state.animation==-1?int8_t(0):state.animation,state.frame};
    else state.selected.reset();
    if(!selected)return nullptr;
    if(state.deadline==UINT32_MAX)state.deadline=tick+uint32_t(int32_t(selected->duration));
    if(state.deadline<=tick) {
        const uint8_t action=selected->control&15;
        if(action==0)state.frame=signed_byte(uint8_t(state.frame)+1);
        else if(action==1 && state.queued==-1) {
            state.frame=int8_t(selected->control>>4);state.loops=uint8_t(state.loops+1);
        } else {
            state.frame=0;state.loops=0;state.animation=state.queued;state.queued=-1;
        }
        const auto* next=frame_at(bank,state.animation,state.frame);
        if(next)state.deadline+=uint32_t(int32_t(next->duration));
    }
    return selected;
}
}
