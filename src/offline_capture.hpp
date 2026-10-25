#pragma once
// Offline export uses the same race renderer and mixer, one tick per frame.
#include "mixer.hpp"
#include "sdl_handles.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

namespace rr {
class OfflineCapture {
    using File = std::unique_ptr<FILE, decltype(&std::fclose)>;
    File video_{nullptr, &std::fclose}, audio_{nullptr, &std::fclose};
    long ticks_ = 0;
    static void write(FILE* f, const void* data, size_t size) {
        if (std::fwrite(data, 1, size, f) != size) throw std::runtime_error("Offline capture write failed");
    }
public:
    static constexpr int width = 1280, height = 960, fps = 60;
    std::string video_path, audio_path;
    long frames = 0, warmup = 360, written = 0;
    bool enabled() const { return frames > 0; }
    bool done() const { return written == frames; }
    void open() {
        video_.reset(std::fopen(video_path.c_str(), "wb"));
        audio_.reset(std::fopen(audio_path.c_str(), "wb"));
        if (!video_ || !audio_) throw std::runtime_error("Cannot open offline video/audio output");
    }
    void frame(SDL_Renderer* renderer, Mixer* mixer) {
        // 22050 / 60 = 367.5: alternate 367/368, preserving the fractional
        // sample even during warm-up. No audio device/callback runs offline.
        const int samples = int((ticks_ + 1) * Mixer::rate / fps - ticks_ * Mixer::rate / fps);
        std::array<int16_t, 736> pcm{};
        if (mixer) mixer->render(pcm.data(), samples);
        if (ticks_++ < warmup) return;
        SurfacePtr captured(SDL_RenderReadPixels(renderer, nullptr));
        SurfacePtr rgb(SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGB24));
        if (!captured || !rgb || !SDL_BlitSurfaceScaled(captured.get(), nullptr, rgb.get(), nullptr, SDL_SCALEMODE_NEAREST))
            throw std::runtime_error(SDL_GetError());
        for (int y = 0; y < height; ++y)
            write(video_.get(), static_cast<const uint8_t*>(rgb->pixels) + y * rgb->pitch, width * 3);
        // Explicit little-endian PCM, including on big-endian hosts.
        std::array<uint8_t, 1472> bytes{};
        for (int i = 0; i < samples * 2; ++i) {
            const auto sample = uint16_t(pcm[size_t(i)]);
            bytes[size_t(i) * 2] = uint8_t(sample);
            bytes[size_t(i) * 2 + 1] = uint8_t(sample >> 8);
        }
        write(audio_.get(), bytes.data(), size_t(samples) * 4);
        ++written;
    }
    void finish() {
        if (!done()) throw std::runtime_error("Offline capture interrupted before requested frame count");
        if (std::fflush(video_.get()) || std::fflush(audio_.get())) throw std::runtime_error("Offline capture flush failed");
    }
};
}
