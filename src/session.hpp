#pragma once
#include <SDL3/SDL_events.h>
#include <atomic>
#include <cstdint>

namespace rr {
// Lifecycle events can arrive from SDL's Android event thread. Only atomic state
// is touched there; simulation and renderer work remain on the game thread.
class SessionState {
  public:
    void handle(const SDL_Event&);
    void pause();
    void resume();
    bool paused() const { return paused_.load() || background_.load() || !focused_.load(); }
    bool background() const { return background_.load(); }
    bool take_clock_reset() { return reset_.exchange(false); }

  private:
    std::atomic<bool> paused_{false}, background_{false}, focused_{true}, reset_{false};
};

// Rational 60 Hz accumulator: no nanosecond truncation drift, and no catch-up
// burst after pause/resume. Long stalls use the simulation's existing step cap.
class TickClock {
  public:
    int advance(uint64_t elapsed_ns, bool paused);
    void reset() { remainder_ = 0; }
    uint32_t alpha_q16() const; // plan 21 Task 2: sub-tick remainder in Q16
  private:
    uint64_t remainder_ = 0;
};
} // namespace rr
