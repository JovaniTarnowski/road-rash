#include "mixer.hpp"
#include <algorithm>
#include <climits>
#include <limits>
#include <stdexcept>
namespace rr {
Mixer::Mixer(int count) {
    if (count<1 || count>256) throw std::runtime_error("Invalid mixer voice limit");
    voices_.resize(size_t(count));
}
Mixer::~Mixer() { close_device(); }
bool Mixer::push(const Command& command) {
    const unsigned head=head_.load(std::memory_order_relaxed), next=(head+1)%queue_.size();
    if (next==tail_.load(std::memory_order_acquire)) return false;
    queue_[head]=command;
    head_.store(next,std::memory_order_release);
    return true;
}
int Mixer::play(const PcmSound* sound,int volume,int pan,int pitch,bool loop) {
    if (!sound || sound->rate<1 || sound->rate>192000 || sound->channels<1 || sound->channels>2 ||
        sound->samples.empty() || sound->samples.size()%size_t(sound->channels) ||
        sound->samples.size()/size_t(sound->channels)>UINT32_MAX || next_id_==INT_MAX) return -1;
    Command command{1,next_id_,std::clamp(volume,0,127),std::clamp(pan,-64,63),std::clamp(pitch,1,4096),sound,loop};
    if (!push(command)) return -1;
    return next_id_++;
}
bool Mixer::set(int id,int volume,int pan,int pitch) {
    return push({2,id,std::clamp(volume,0,127),std::clamp(pan,-64,63),std::clamp(pitch,1,4096),nullptr,false});
}
bool Mixer::stop(int id) { return push({3,id,0,0,256,nullptr,false}); }
bool Mixer::cursor(int id,uint64_t& frame) const {
    if (id<0) return false;
    for (size_t i=0;i<voices_.size();++i) {
        if (cursors_[i].id.load(std::memory_order_acquire)!=id) continue;
        const uint64_t value=cursors_[i].frame.load(std::memory_order_acquire);
        if (cursors_[i].id.load(std::memory_order_acquire)!=id) return false;
        frame=value;return true;
    }
    return false;
}
int Mixer::queued_frames() const {
    if (!stream_) return 0;
    return std::max(0,SDL_GetAudioStreamQueued(stream_))/4;
}
void Mixer::retire(const PcmSound* sound) noexcept {
    if (stream_ && !SDL_LockAudioStream(stream_)) close_device();
    consume();
    for (auto& voice:voices_) if (voice.sound==sound) voice.sound=nullptr;
    if (stream_) SDL_UnlockAudioStream(stream_);
}
void Mixer::consume() {
    unsigned tail=tail_.load(std::memory_order_relaxed);
    while (tail!=head_.load(std::memory_order_acquire)) {
        const Command command=queue_[tail];
        if (command.kind==1) {
            auto slot=std::find_if(voices_.begin(),voices_.end(),[](const Voice& v) { return !v.sound; });
            if (slot==voices_.end()) slot=std::min_element(voices_.begin(),voices_.end(),[](const Voice& a,const Voice& b) { return a.id<b.id; });
            auto& cursor=cursors_[size_t(slot-voices_.begin())];
            cursor.id.store(-1,std::memory_order_release);
            cursor.frame.store(0,std::memory_order_release);
            *slot={command.sound,0,command.id,command.volume,command.pan,command.pitch,command.loop};
            cursor.id.store(command.id,std::memory_order_release);
        } else {
            const auto slot=std::find_if(voices_.begin(),voices_.end(),[&](const Voice& v) { return v.id==command.id; });
            if (slot!=voices_.end()) {
                if (command.kind==3) slot->sound=nullptr;
                else { slot->volume=command.volume;slot->pan=command.pan;slot->pitch=command.pitch; }
            }
        }
        tail=(tail+1)%queue_.size();
        tail_.store(tail,std::memory_order_release);
    }
}
void Mixer::render(int16_t* out,int frames) {
    if (frames<0 || (!out && frames)) throw std::runtime_error("Invalid mixer output");
    consume();
    for (int frame=0;frame<frames;++frame) {
        int64_t mixed[2]{};
        if (!paused_.load(std::memory_order_relaxed)) for (auto& voice:voices_) {
            if (!voice.sound) continue;
            const auto& sound=*voice.sound;
            const uint64_t count=sound.samples.size()/size_t(sound.channels);
            const uint64_t period=count<<32;
            if (voice.position>=period) {
                if (voice.loop) voice.position%=period;
                else { voice.sound=nullptr;continue; }
            }
            const size_t index=size_t(voice.position>>32);
            const size_t next=index+1<count ? index+1 : voice.loop ? 0 : index;
            const uint64_t fraction=voice.position&0xffffffffull;
            for (size_t channel=0;channel<2;++channel) {
                const size_t input_channel=sound.channels==1 ? 0 : channel;
                const int64_t a=sound.samples[index*sound.channels+input_channel], b=sound.samples[next*sound.channels+input_channel];
                const int64_t sample=(a*int64_t(0x100000000ull-fraction)+b*int64_t(fraction))/int64_t(0x100000000ull);
                int gain=127;
                if (channel==0 && voice.pan>0) gain=127*(63-voice.pan)/63;
                if (channel==1 && voice.pan<0) gain=127*(64+voice.pan)/64;
                mixed[channel]+=sample*voice.volume*gain/(127*127);
            }
            const uint64_t step=(uint64_t(sound.rate)*uint64_t(voice.pitch)<<24)/rate;
            if (voice.loop) {
                const uint64_t advance=step%period;
                if (voice.position>=period-advance) voice.position-=period-advance;
                else voice.position+=advance;
            } else if (step>=period-voice.position) voice.position=period;
            else voice.position+=step;
        }
        out[2*frame]=int16_t(std::clamp<int64_t>(mixed[0],-32768,32767));
        out[2*frame+1]=int16_t(std::clamp<int64_t>(mixed[1],-32768,32767));
    }
    for (size_t i=0;i<voices_.size();++i) cursors_[i].frame.store(voices_[i].position>>32,std::memory_order_release);
}
void SDLCALL Mixer::callback(void* data,SDL_AudioStream* stream,int additional,int) {
    auto& mixer=*static_cast<Mixer*>(data);
    std::array<int16_t,8192> samples;
    int remaining=additional>0 ? additional/4+(additional%4!=0) : 0;
    while (remaining>0) {
        const int frames=std::min(remaining,4096);
        mixer.render(samples.data(),frames);
        if (!SDL_PutAudioStreamData(stream,samples.data(),frames*4)) break;
        remaining-=frames;
    }
}
bool Mixer::open_device() {
    if (stream_) return true;
    const SDL_AudioSpec spec{SDL_AUDIO_S16,2,rate};
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,callback,this);
    if (!stream_) return false;
    if (!SDL_ResumeAudioStreamDevice(stream_)) { close_device();return false; }
    return true;
}
void Mixer::close_device() { if (stream_) SDL_DestroyAudioStream(stream_); stream_=nullptr; }
bool Mixer::handle(const SDL_Event& event) {
    if (event.type!=SDL_EVENT_AUDIO_DEVICE_REMOVED || event.adevice.recording || !stream_) return true;
    close_device();
    return open_device();
}
}
