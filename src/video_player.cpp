#include "video_player.hpp"
#include "ima_adpcm.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace rr {
VideoPlayer::VideoPlayer(const Assets& assets, const std::string& path, Mixer& mixer)
    : mixer_(mixer), data_(assets.read(path)), avi_(read_avi(data_)) {
    int video = -1, audio = -1;
    for (size_t i = 0; i < avi_.streams.size(); ++i) {
        if (avi_.streams[i].type == fourcc('v', 'i', 'd', 's')) {
            if (video >= 0) throw std::runtime_error("Multiple AVI video streams");
            video = int(i);
        }
        if (avi_.streams[i].type == fourcc('a', 'u', 'd', 's')) {
            if (audio >= 0) throw std::runtime_error("Multiple AVI audio streams");
            audio = int(i);
        }
    }
    if (video < 0 || avi_.streams[video].fourcc != fourcc('c', 'v', 'i', 'd'))
        throw std::runtime_error("Unsupported AVI video codec");
    const auto& v = avi_.streams[video];
    width_ = v.width;
    height_ = v.height;
    rate_ = v.rate;
    scale_ = v.scale;
    decoder_ = std::make_unique<CinepakDecoder>(width_, height_);
    if (audio >= 0) {
        const auto& a = avi_.streams[audio];
        if (a.audio_format != 17) throw std::runtime_error("Unsupported AVI audio codec");
        audio_.rate = a.audio_rate;
        audio_.channels = a.audio_channels;
    }
    for (const auto& chunk : avi_.chunks) {
        if (chunk.stream == video)
            frames_.push_back(chunk);
        else if (chunk.stream == audio) {
            const auto& a = avi_.streams[audio];
            const auto samples =
                decode_ima_adpcm(data_.data() + chunk.offset, chunk.size, a.audio_channels, a.block_align);
            audio_.samples.insert(audio_.samples.end(), samples.begin(), samples.end());
        }
    }
    if (frames_.empty()) throw std::runtime_error("AVI has no complete video frames");
}
VideoPlayer::~VideoPlayer() {
    mixer_.retire(&audio_);
    SDL_DestroyTexture(texture_);
}
void VideoPlayer::pause(bool value) {
    if (paused_ != value) reset_time_ = true;
    paused_ = value;
    mixer_.pause(value);
}
void VideoPlayer::skip() {
    finished_ = true;
    if (voice_ >= 0) mixer_.stop(voice_);
}
bool VideoPlayer::update(uint64_t now) {
    if (finished_) return false;
    if (!started_) {
        started_ = true;
        last_ = now;
        if (!audio_.samples.empty() && mixer_.device_open())
            voice_ = mixer_.play(&audio_, 127, 0, 256, false);
    }
    const uint64_t delta = !reset_time_ && now >= last_ ? now - last_ : 0;
    last_ = now;
    reset_time_ = false;
    if (paused_) return true;
    uint64_t cursor = 0;
    const bool has_cursor = voice_ >= 0 && mixer_.cursor(voice_, cursor);
    if (voice_ >= 0 && !has_cursor) return true;
    if (has_cursor && cursor < audio_.samples.size() / size_t(audio_.channels)) {
        const uint64_t queued = uint64_t(mixer_.queued_frames()) * audio_.rate / Mixer::rate;
        const uint64_t played = cursor > queued ? cursor - queued : 0;
        elapsed_ = std::max<uint64_t>(elapsed_, played * 1000000000ull / uint64_t(audio_.rate));
    } else {
        const uint64_t limit = std::numeric_limits<uint64_t>::max();
        elapsed_ += std::min(delta, limit - elapsed_);
    }
    // Quotient/remainder avoids overflowing nanoseconds * frame rate.
    const uint64_t seconds = elapsed_ / 1000000000ull, remainder = elapsed_ % 1000000000ull;
    const uint64_t max_frame = frames_.size();
    uint64_t target = max_frame;
    if (seconds <= std::numeric_limits<uint64_t>::max() / rate_) {
        const uint64_t whole = seconds * rate_, part = remainder * rate_ / 1000000000ull;
        if (whole <= std::numeric_limits<uint64_t>::max() - part) target = (whole + part) / scale_;
    }
    if (target >= max_frame) {
        skip();
        return false;
    }
    while (next_ <= target) {
        const auto& chunk = frames_[next_];
        if (chunk.size) decoder_->decode(data_.data() + chunk.offset, chunk.size);
        shown_ = next_++;
    }
    return true;
}
SDL_FRect VideoPlayer::fit(int width, int height, int output_width, int output_height) {
    if (width <= 0 || height <= 0 || output_width <= 0 || output_height <= 0) return {0, 0, 0, 0};
    const float scale = std::min(float(output_width) / float(width), float(output_height) / float(height));
    const float w = float(width) * scale, h = float(height) * scale;
    return {(float(output_width) - w) / 2, (float(output_height) - h) / 2, w, h};
}
void VideoPlayer::draw(SDL_Renderer* renderer) {
    if (renderer_ != renderer) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
        renderer_ = renderer;
    }
    if (!texture_)
        texture_ =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width_, height_);
    if (!texture_ || !SDL_UpdateTexture(texture_, nullptr, decoder_->rgb24().data(), width_ * 3))
        throw std::runtime_error(SDL_GetError());
#if SDL_VERSION_ATLEAST(3, 4, 0)
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_PIXELART);
#else
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
#endif
    int logical_w = 0, logical_h = 0, output_w = 0, output_h = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &mode);
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    if (!SDL_GetCurrentRenderOutputSize(renderer, &output_w, &output_h))
        throw std::runtime_error(SDL_GetError());
    const SDL_FRect rect = fit(width_, height_, output_w, output_h);
    const bool drawn = SDL_RenderTexture(renderer, texture_, nullptr, &rect);
    SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, mode);
    if (!drawn) throw std::runtime_error(SDL_GetError());
}
} // namespace rr
