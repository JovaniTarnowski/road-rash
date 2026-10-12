#pragma once
#include "avi.hpp"
#include "cinepak.hpp"
#include "mixer.hpp"
#include "assets.hpp"
#include <memory>
namespace rr {
class VideoPlayer {
public:
    VideoPlayer(const Assets&,const std::string& path,Mixer&);
    ~VideoPlayer();
    VideoPlayer(const VideoPlayer&)=delete;
    VideoPlayer& operator=(const VideoPlayer&)=delete;
    bool update(uint64_t now_ns);
    void pause(bool value);
    bool finished() const { return finished_; }
    void skip();
    // Fills the whole render output (not the 640x480 logical frame), keeping
    // the clip's aspect ratio; the logical presentation is restored afterwards.
    void draw(SDL_Renderer*);
    static SDL_FRect fit(int width,int height,int output_width,int output_height);
    void reset_texture() { SDL_DestroyTexture(texture_);texture_=nullptr; }
    size_t frame() const { return shown_; }
    const std::vector<uint8_t>& rgb24() const { return decoder_->rgb24(); }
private:
    Mixer& mixer_;
    std::vector<uint8_t> data_;
    AviFile avi_;
    std::vector<AviChunk> frames_;
    std::unique_ptr<CinepakDecoder> decoder_;
    PcmSound audio_;
    int voice_=-1,width_=0,height_=0;
    uint32_t rate_=0,scale_=0;
    uint64_t last_=0,elapsed_=0;
    size_t next_=0,shown_=0;
    bool started_=false,paused_=false,finished_=false,reset_time_=false;
    SDL_Texture* texture_=nullptr;
    SDL_Renderer* renderer_=nullptr;
};
}
