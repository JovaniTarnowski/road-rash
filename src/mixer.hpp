#pragma once
#include "wav.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <vector>
namespace rr {
// One game-thread producer, one audio-thread consumer. Sounds must remain alive
// until the mixer closes. No allocation or locks occur in render().
class Mixer {
public:
    static constexpr int rate=22050;
    explicit Mixer(int max_voices);
    ~Mixer();
    Mixer(const Mixer&)=delete;
    Mixer& operator=(const Mixer&)=delete;
    int play(const PcmSound*,int volume,int pan,int pitch_q8,bool loop);
    bool set(int voice,int volume,int pan,int pitch_q8);
    bool stop(int voice);
    bool cursor(int voice,uint64_t& source_frame) const;
    // True while a started voice is queued or still sounding (DirectSound GetStatus PLAYING).
    bool active(int voice) const;
    int queued_frames() const;
    // Game-thread lifetime boundary: drain commands under SDL's stream lock
    // before releasing a sound. The callback never takes a user mutex.
    void retire(const PcmSound*) noexcept;
    void render(int16_t* out,int frames);
    void pause(bool value) { paused_=value; }
    bool open_device();
    void close_device();
    bool handle(const SDL_Event&);
    bool device_open() const { return stream_!=nullptr; }
private:
    struct Command { int kind=0,id=0,volume=0,pan=0,pitch=256; const PcmSound* sound=nullptr; bool loop=false; };
    struct Voice { const PcmSound* sound=nullptr; uint64_t position=0; int id=-1,volume=0,pan=0,pitch=256; bool loop=false; };
    std::array<Command,256> queue_{};
    std::atomic<unsigned> head_{0},tail_{0};
    std::atomic<bool> paused_{false};
    std::vector<Voice> voices_;
    struct PlaybackCursor { std::atomic<int> id{-1}; std::atomic<uint64_t> frame{0}; std::atomic<bool> active{false}; };
    std::atomic<int> consumed_id_{-1};
    std::array<PlaybackCursor,256> cursors_{};
    int next_id_=0;
    SDL_AudioStream* stream_=nullptr;
    bool push(const Command&);
    void consume();
    static void SDLCALL callback(void*,SDL_AudioStream*,int,int);
};
}
