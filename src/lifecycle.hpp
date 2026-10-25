// Plan 28 Task 4: SDL lifecycle helpers + display constants (moved from game_main.cpp).
#pragma once
#include <SDL3/SDL.h>
#include <stdexcept>
#include "mixer.hpp"
#include "race.hpp"
#include "session.hpp"

namespace rr {
#ifdef __ANDROID__
constexpr bool kMobileDisplay = true;
#else
constexpr bool kMobileDisplay = false;
#endif
#if SDL_VERSION_ATLEAST(3, 4, 0)
constexpr SDL_ScaleMode crisp_scale = SDL_SCALEMODE_PIXELART;
#else
constexpr SDL_ScaleMode crisp_scale = SDL_SCALEMODE_NEAREST;
#endif
constexpr int countdown_ticks = 3 * ticks_per_second;

inline bool lifecycle_event(uint32_t type) {
    return type == SDL_EVENT_WILL_ENTER_BACKGROUND || type == SDL_EVENT_DID_ENTER_BACKGROUND ||
           type == SDL_EVENT_WILL_ENTER_FOREGROUND || type == SDL_EVENT_DID_ENTER_FOREGROUND;
}

struct LifecycleWatch {
    SessionState state;
    Mixer* mixer = nullptr;
    static bool SDLCALL watch(void* data, SDL_Event* event) {
        if (lifecycle_event(event->type)) {
            auto& self = *static_cast<LifecycleWatch*>(data);
            self.state.handle(*event);
            if (self.mixer) self.mixer->pause(self.state.paused());
        }
        return true;
    }
    explicit LifecycleWatch(Mixer* audio = nullptr) : mixer(audio) {
        if (!SDL_AddEventWatch(watch, this)) throw std::runtime_error(SDL_GetError());
    }
    ~LifecycleWatch() { SDL_RemoveEventWatch(watch, this); }
};
} // namespace rr
