#include "session.hpp"
#include "race.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) { ++failures; std::cerr << "FAIL " << message << '\n'; }
    };
    rr::SessionState state;
    auto event = [&](Uint32 type) { SDL_Event e{}; e.type = type; state.handle(e); };
    check(!state.paused(), "race starts active");
    event(SDL_EVENT_WILL_ENTER_BACKGROUND);
    check(state.paused() && state.background(), "background pauses immediately");
    state.resume();
    check(state.paused(), "cannot resume while backgrounded");
    event(SDL_EVENT_DID_ENTER_BACKGROUND);
    event(SDL_EVENT_WILL_ENTER_FOREGROUND);
    event(SDL_EVENT_DID_ENTER_FOREGROUND);
    check(state.paused() && !state.background(), "foreground requires explicit resume");
    check(state.take_clock_reset() && !state.take_clock_reset(), "clock reset consumed once");
    state.resume();
    check(!state.paused() && state.take_clock_reset(), "explicit resume resets clock");
    event(SDL_EVENT_WINDOW_FOCUS_LOST);
    state.resume();
    check(state.paused(), "cannot resume without focus");
    event(SDL_EVENT_WINDOW_FOCUS_GAINED);
    check(state.paused(), "focus gain does not resume");
    state.resume();
    check(!state.paused(), "focused window resumes");

    // Exact 10-second sequences at common presentation rates.
    for (uint64_t hz : {30, 60, 120, 144}) {
        rr::TickClock clock;
        int steps = 0;
        uint64_t previous = 0;
        for (uint64_t frame = 1; frame <= hz * 10; ++frame) {
            const uint64_t now = frame * 1000000000ull / hz;
            steps += clock.advance(now - previous, false);
            previous = now;
        }
        check(steps == 10 * rr::ticks_per_second, "fixed rate independent of presentation rate");
    }
    rr::TickClock clock;
    check(clock.advance(10000000, false) == 0, "partial tick retained");
    check(clock.advance(60000000000ull, true) == 0, "background time never advances simulation");
    check(clock.advance(10000000, false) == 0, "resume discards pre-pause partial tick");
    check(clock.advance(10000000, false) == 1, "normal ticks after resume");
    check(clock.advance(UINT64_MAX, false) == rr::max_steps_per_frame, "stall clamped before multiplication");
    check(clock.advance(0, false) == 0, "stall leaves no catch-up debt");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
