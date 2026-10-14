# Race Shell: Input Actions, RAII Handles and the Race Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) conventions and [REVIEW-ORCHESTRATION.md](REVIEW-ORCHESTRATION.md) first. All tasks edit `src/game_main.cpp`, so they run **serially** with one worker.

**Goal:** Turn `run_race` (about 290 lines in `game_main.cpp`) into a tested, leak-free `RaceScreen`. Event-to-action decisions become a pure, unit-tested function; SDL handles are owned by RAII types; overlays move to their own file. [Plan 23](23-web-port.md) Task 3 (SDL main callbacks) then becomes a mechanical move.

**Architecture:** Four small refactors, each behaviour-preserving and verified by headless screenshot and exit-code hashes. (1) `race_action()` maps an `SDL_Event` and a context to one `RaceAction`; `run_race` executes actions in a single `switch`. (2) `sdl_handles.hpp` provides `unique_ptr` aliases with SDL deleters. (3) `FpsOverlay` and `draw_quit_prompt` move to `race_overlay.cpp`. (4) `RaceScreen` holds the loop state and exposes `event()`/`iterate()` with plan 23's `AppScreen` signatures; `run_race` becomes a 10-line driver.

**Tech Stack:** C++20, SDL3 (events can be built without a window), CTest.

**Spec:** [../agent-review/CODE-REVIEW-2026-10-13.md](../agent-review/CODE-REVIEW-2026-10-13.md), finding 7 (and finding 1, which this plan's tests pin).

## Global Constraints

- No behaviour change: the plan 23 Task 3 Step 1 baseline (BMP hashes and exit codes of `--menu-frames 120 --screenshot`, `--race --ticks 900 --autopilot --screenshot`, `--video VIDEO/INTRO.AVI --ticks 300 --screenshot`, `--first-run-smoke`) is identical after every task. Record it once in `analysis/review-28/before/` before Task 1.
- Android `main` and desktop `rr-game` compile the same sources. Every new non-pure file is added to both targets through one CMake variable (Task 1 creates `RR_GAME_SOURCES`).
- Starts after plan 26 Task 3 merges (both edit `Game`).

## Review Focus

1. **Keyboard repeat events.** Held Escape must not toggle pause on every repeat (`e.key.repeat` stays filtered; there is a test).
2. **A second gamepad's buttons.** Only the opened pad's `which` counts (test).
3. **Career Escape twice** (quit confirm, outcome `wrecked`) versus quick-race Escape (pause menu). Both paths have tests.
4. **The device-lost path throws inside the loop.** No texture or gamepad leaks (ASan `detect_leaks=1` on macOS is unreliable; assert via a counting deleter in the RAII test).
5. **Touch during the pause menu.** It resumes when the game is paused without a menu, and is ignored when the menu owns input (test).

---

### Task 1: Pure race-input actions

**Wave:** W5 · **Owns:** `src/race_actions.hpp`, `src/race_actions.cpp`, `tests/race_actions_test.cpp`, `src/game_main.cpp` (`run_race` event loop), `CMakeLists.txt` (`RR_GAME_SOURCES` + test) · **Profile:** careful-mechanical

**Interfaces:**
- Produces:
```cpp
namespace rr {
enum class RaceAction { none, quit_app, open_pause, toggle_pause, resume, request_quit, confirm_quit,
                        cancel_quit, restart, demo_exit };
struct RaceActionContext {
    bool demo = false, career = false, finished = false, paused = false, quitting = false;
    bool has_menu = false, menu_open = false;
    SDL_JoystickID pad = 0;          // 0 = no pad open
};
// One decision per event; mirrors run_race as of the plan-25 fixes.
RaceAction race_action(const SDL_Event&, const RaceActionContext&);
}
```

- [x] **Step 1: Baseline** (see Global Constraints).
- [x] **Step 2: Write the failing test** `tests/race_actions_test.cpp`:

```cpp
#include "race_actions.hpp"
#include <iostream>

static SDL_Event key(SDL_Keycode k, bool repeat = false) {
    SDL_Event e{}; e.type = SDL_EVENT_KEY_DOWN; e.key.key = k; e.key.repeat = repeat; return e;
}
static SDL_Event button(SDL_JoystickID id, Uint8 b) {
    SDL_Event e{}; e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; e.gbutton.which = id; e.gbutton.button = b; return e;
}
int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    using A = rr::RaceAction;
    rr::RaceActionContext quick; quick.has_menu = true; quick.pad = 7;
    check(rr::race_action(key(SDLK_ESCAPE), quick) == A::open_pause, "quick Esc opens pause");
    check(rr::race_action(key(SDLK_ESCAPE, true), quick) == A::none, "key repeat ignored");
    check(rr::race_action(key(SDLK_RETURN), quick) == A::none, "Enter before finish does nothing");
    rr::RaceActionContext done = quick; done.finished = true;
    check(rr::race_action(key(SDLK_RETURN), done) == A::restart, "Enter after finish restarts (finding 1)");
    check(rr::race_action(button(7, SDL_GAMEPAD_BUTTON_SOUTH), done) == A::restart, "pad South restarts");
    check(rr::race_action(button(8, SDL_GAMEPAD_BUTTON_SOUTH), done) == A::none, "other pad ignored");
    SDL_Event tap{}; tap.type = SDL_EVENT_FINGER_DOWN;
    check(rr::race_action(tap, done) == A::restart, "tap restarts");
    rr::RaceActionContext career = quick; career.career = true;
    check(rr::race_action(key(SDLK_ESCAPE), career) == A::request_quit, "career Esc asks to quit");
    career.quitting = true;
    check(rr::race_action(key(SDLK_ESCAPE), career) == A::confirm_quit, "second Esc confirms");
    check(rr::race_action(key(SDLK_A), career) == A::cancel_quit, "other key cancels");
    rr::RaceActionContext career_done = quick; career_done.career = true; career_done.finished = true;
    check(rr::race_action(key(SDLK_RETURN), career_done) == A::none, "career never restarts");
    check(rr::race_action(key(SDLK_Q), quick) == A::quit_app, "Q quits a quick race");
    rr::RaceActionContext demo; demo.demo = true;
    check(rr::race_action(key(SDLK_SPACE), demo) == A::demo_exit, "any key exits the demo");
    rr::RaceActionContext no_menu; no_menu.paused = true;
    check(rr::race_action(tap, no_menu) == A::resume, "tap resumes without a menu");
    rr::RaceActionContext menu_open = quick; menu_open.menu_open = true;
    check(rr::race_action(key(SDLK_ESCAPE), menu_open) == A::none, "menu owns input");
    if (failures == 0) std::cout << "race_actions OK\n";
    return failures ? 1 : 0;
}
```

- [x] **Step 3: Run it and confirm it fails** (header missing).
- [x] **Step 4: Implement** `race_action` by transcribing the conditions of the current `run_race` event loop (`game_main.cpp` ~1047–1110), branch by branch, keeping their order. Menu-owned events return `none`; the caller still forwards them to `pause_event`. Then rewrite the loop in `run_race` as:
```cpp
            const rr::RaceAction act = rr::race_action(e, ctx_now());
            switch (act) {
            case rr::RaceAction::none: break;
            case rr::RaceAction::quit_app: running = false; result.quit_app = true; break;
            case rr::RaceAction::demo_exit: running = false; break;
            case rr::RaceAction::open_pause: opt.menu->open_pause(capture_race_frame(renderer)); lifecycle.state.resume(); break;
            case rr::RaceAction::toggle_pause: lifecycle.state.paused() ? lifecycle.state.resume() : lifecycle.state.pause(); break;
            case rr::RaceAction::resume: lifecycle.state.resume(); break;
            case rr::RaceAction::request_quit: g.quitting = true; break;
            case rr::RaceAction::cancel_quit: g.quitting = false; break;
            case rr::RaceAction::confirm_quit: g.quitting = false; end_as_quit(); running = false; break;
            case rr::RaceAction::restart: restart = true; break;
            }
```
  `ctx_now()` is a local lambda that builds the context from `opt`, `g` and `lifecycle`; `end_as_quit()` is the existing `if (!decided) {...}` block. Non-action handling stays where it is: `SDL_EVENT_QUIT`, lifecycle forwarding, mixer, gamepad add/remove, touch movement and render reset. Put `race_actions.cpp` in `rr-resource` (it needs only SDL headers). Define `set(RR_GAME_SOURCES src/game_main.cpp src/road_draw.cpp src/import_ui.cpp)` and use it for both `main` and `rr-game`.
- [x] **Step 5: Verify:** `race_actions-test` OK; full `ctest`; the baseline hashes match; manual pass over the five Review Focus inputs (keyboard and pad).
- [x] **Step 6: Commit.** `game: pure race input actions with tests`

---

### Task 2: RAII SDL handles

**Wave:** W6 (after Task 1) · **Owns:** `src/sdl_handles.hpp`, `tests/sdl_handles_test.cpp`, `src/game_main.cpp` · **Profile:** mechanical

**Interfaces:**
- Produces:
```cpp
#pragma once
#include <SDL3/SDL.h>
#include <memory>
namespace rr {
struct SdlTextureDeleter { void operator()(SDL_Texture* t) const noexcept { SDL_DestroyTexture(t); } };
struct SdlGamepadDeleter { void operator()(SDL_Gamepad* p) const noexcept { SDL_CloseGamepad(p); } };
struct SdlSurfaceDeleter { void operator()(SDL_Surface* s) const noexcept { SDL_DestroySurface(s); } };
using TexturePtr = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;
using GamepadPtr = std::unique_ptr<SDL_Gamepad, SdlGamepadDeleter>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SdlSurfaceDeleter>;
}
```
- [ ] **Step 1: Test** `tests/sdl_handles_test.cpp`: create a surface with `SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_XRGB8888)` inside a `SurfacePtr`, throw from inside a scope, catch it, and assert there was no crash. A null `TexturePtr` must be safe to reset. No window is needed. Register it in the test list.
- [ ] **Step 2: Replace** every raw `SDL_Texture*`, `SDL_Gamepad*` and `SDL_Surface*` local or member in `game_main.cpp` (`rg -n "SDL_Texture\*|SDL_Gamepad\*|SDL_Surface\*" src/game_main.cpp`) with the aliases, and delete the manual `SDL_Destroy*`/`SDL_Close*` calls at scope ends. The `pad` parameter of `pause_event(…, SDL_Gamepad*& pad, …)` becomes `GamepadPtr&`.
- [ ] **Step 3: Verify** the baseline hashes, `ctest`, and the ASan tree. Commit: `game: RAII for SDL textures, gamepads and surfaces`.

---

### Task 3: Overlays out of the loop

**Wave:** W6 (after Task 2) · **Owns:** `src/race_overlay.hpp`, `src/race_overlay.cpp`, `src/game_main.cpp`, the `RR_GAME_SOURCES` CMake line

**Interfaces:**
- Produces:
```cpp
namespace rr {
class FpsOverlay {     // Futura Bold 24 #DEFFF0, 2-px black shadow, refreshed every 500 ms
public:
    explicit FpsOverlay(const Assets&);
    void frame(SDL_Renderer*, uint64_t now_ns, int ticks_this_frame);   // counts + redraws when due, then renders
private: /* the fps_* locals of run_race, with TexturePtr */
};
void draw_quit_prompt(SDL_Renderer*, const std::string& prompt_cp1252);   // the g.quitting block
}
```
- [ ] **Step 1:** Move the `fps_*` locals and both FPS blocks into `FpsOverlay`, verbatim except for the RAII handles. Move the quit-prompt block into `draw_quit_prompt`.
- [ ] **Step 2:** Verify the baseline hashes plus a run with `--show-fps` (the screenshot hash with FPS on is recorded in the baseline too; the FPS digits vary, so compare only that the run exits 0 and the text region is non-empty). Commit: `game: FPS overlay and quit prompt as units`.

---

### Task 4: `RaceScreen` with the plan 23 screen interface

**Wave:** W6 (after Task 3) · **Owns:** `src/race_screen.hpp`, `src/race_screen.cpp`, `src/game_main.cpp`, the CMake line · **Profile:** careful-mechanical

**Interfaces:**
- Produces (the signatures are plan 23 Task 3's `AppScreen`, so that task only adds the base class and callbacks):
```cpp
namespace rr {
class RaceScreen {
public:
    RaceScreen(Game&, SDL_Renderer*, const RaceOptions&, DisplayDriver&);
    void event(const SDL_Event&);
    bool iterate(uint64_t now_ns);   // false when the race loop would have exited
    RaceEnd result() const;
};
}
```
  `Game`, `RaceOptions`, `RaceEnd` and `DisplayDriver` move from `game_main.cpp`'s anonymous namespace into `src/game_types.hpp` (namespace `rr`), unchanged.
- [ ] **Step 1:** Move the loop's locals to members and split the loop body: event polling goes to `event`; everything after the event loop goes to `iterate`. `run_race` becomes:
```cpp
RaceEnd run_race(Game& g, SDL_Renderer* r, const RaceOptions& opt, DisplayDriver& d) {
    rr::RaceScreen screen(g, r, opt, d);
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) screen.event(e);
        if (!screen.iterate(SDL_GetTicksNS())) break;
    }
    return screen.result();
}
```
- [ ] **Step 2:** Verify the baseline hashes, `ctest`, `tools/ci.sh` (Android assemble), and a manual race plus pause/resume/restart. Commit: `game: race loop as RaceScreen (plan 23 Task 3 prework)`.
- [ ] **Step 3:** The orchestrator adds a note to plan 23 Task 3: "RaceScreen exists (plan 28 Task 4); derive it from `AppScreen`."
