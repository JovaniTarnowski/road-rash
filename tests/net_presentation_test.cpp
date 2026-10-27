#include "assets.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "net/client.hpp"
#include "race_screen.hpp"
#include "race_state.hpp"
#include <filesystem>
#include <cstdlib>
#include <iostream>

#ifdef __EMSCRIPTEN__
int main() {
    return 77;
}
#else
namespace {
void pure(rr::test::Checks& check) {
    rr::net::SoundHistory history(16);
    rr::SoundEventRecord a{rr::SoundEvent::impact_punch, 0, 0, 0, 0, 0, 1}, b = a;
    b.source = 2;
    int played = 0;
    const std::vector<rr::SoundEventRecord> events{a, b};
    auto play = [&](const auto&) { ++played; };
    history.deliver(0, events, play);
    history.deliver(0, events, play);
    check(played == 2, "same sound from two emitters plays once per emitter");
    a.code = rr::SoundEvent::impact_kick;
    history.deliver(0, std::span(&a, 1), play);
    check(played == 3, "new correction sound is played");
    for (int t = 1; t < 10000; ++t) history.deliver(t, events, play);
    check(history.ticks_held() == 17, "sound playback history stays within the rollback window");
    const int before = played;
    history.deliver(0, events, play);
    check(played == before, "expired ticks cannot replay ancient sounds");

    rr::ViewState old, live;
    old.camera_position_q8 = 100;
    live.camera_position_q8 = 900;
    old.actors = {
        {1, 100, 0, 0, 0, 0, 0, true}, {2, 100, 0, 0, 0, 0, 0, true}, {3, 100, 0, 0, 0, 0, 0, true}};
    live.actors = old.actors;
    for (auto& actor : live.actors) actor.position_q8 = 200;
    rr::RemoteSmoothing smoothing;
    smoothing.correction(old, 1000);
    auto drawn = live;
    smoothing.apply(drawn, 2, 1050);
    check(drawn.actors[0].position_q8 == 200 && drawn.camera_position_q8 == 900,
          "local pose and camera never smoothed");
    check(drawn.actors[1].position_q8 == 150 && drawn.actors[2].position_q8 == 200,
          "only remote human pose smoothed");
    smoothing.apply(drawn, 2, 1100);
    drawn = live;
    smoothing.apply(drawn, 2, 1101);
    check(drawn.actors[1].position_q8 == 200, "correction smoothing expires after 100 ms");
    live.actors[1].route = 1;
    drawn = live;
    smoothing.correction(old, 1200);
    smoothing.apply(drawn, 2, 1250);
    check(drawn.actors[1].position_q8 == 200, "branch changes snap instead of crossing roads");
    auto settings = rr::default_settings();
    settings.network_hud = true;
    settings.smooth_remote = false;
    auto restored = rr::parse_settings(rr::format_settings(settings));
    check(restored.network_hud && !restored.smooth_remote, "network view settings persist");
    rr::NetworkOverlay overlay(nullptr);
    for (int k = 0; k < 7; ++k) overlay.push(std::to_string(k));
    check(overlay.lines().size() == 6 && overlay.lines().front() == "1",
          "six persistent chat lines drop only oldest");
}
rr::net::Start start() {
    rr::net::Start s;
    s.race_id = 1;
    s.seed = 1234;
    s.countdown = 0;
    s.your_seat = 1;
    s.options = {0, 2, true, false};
    s.humans = {{2, 1, 0, "A"}, {5, 2, 0, "B"}};
    return s;
}
std::unique_ptr<rr::RaceSetup> build(const rr::Assets& a, const std::vector<uint8_t>& exe,
                                     const rr::net::Start& s) {
    return std::make_unique<rr::RaceSetup>(rr::make_race_setup(a, exe, rr::net::race_config(s)));
}
void effects(rr::test::Checks& check, const rr::Assets& a, const std::vector<uint8_t>& exe) {
    for (bool correction : {false, true}) {
        auto setup = build(a, exe, start());
        // Controlled terminal fixture: both bikes are wrecked before tick zero.
        for (auto& rider : setup->instance.race->riders()) rider.bike.i32(0x2f0) = 0;
        rr::net::Rollback rollback(setup->instance, 2, 0, 16, 0);
        std::vector<int32_t> confirmed;
        rollback.on_confirmed = [&](const rr::net::Rollback::TickEffects& e) {
            check(e.tick == int(confirmed.size()), "confirmed effects publish once, in order");
            check(e.terminal && e.terminal_hash != 0, "terminal state has a canonical per-tick hash");
            confirmed.push_back(e.tick);
        };
        rollback.advance_to(4);
        check(confirmed.empty(), "predicted terminal states cannot commit side effects");
        const std::array<uint32_t, 2> words{0, correction ? rr::control_right : 0};
        for (int t = 0; t < 4; ++t) rollback.sealed(t, words, 0, 3);
        rollback.advance_to(4);
        check(confirmed.size() == 4, "correct predictions and corrections both publish sealed effects");
        rollback.advance_to(4);
        check(confirmed.size() == 4, "repeated advancement cannot replay confirmed effects");
    }
}
} // namespace
int main() {
    rr::test::Checks check;
    pure(check);
    const std::string root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(root + "/ROADRASH.EXE")) return rr::golden::skip;
    rr::Assets assets(root);
    const auto exe = assets.read("ROADRASH.EXE");
    effects(check, assets, exe);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return 1;
    SDL_Window* window = SDL_CreateWindow("net-presentation-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, "software") : nullptr;
    if (!renderer) return 1;
    try {
        uint64_t now = 1000;
        std::array<std::unique_ptr<rr::net::Client>, 2> clients;
        std::array<std::unique_ptr<rr::net::Channel>, 2> endpoints;
        for (size_t k = 0; k < 2; ++k) {
            auto link = rr::net::loopback_pair({}, true, [&] { return now; });
            endpoints[k] = std::move(link.second);
            clients[k] =
                std::make_unique<rr::net::Client>(std::move(link.first), rr::net::Hello{},
                                                  [&](const auto& s) { return build(assets, exe, s); });
            clients[k]->set_automatic_results(false);
            endpoints[k]->send(rr::net::encode(rr::net::Welcome{}));
            endpoints[k]->send(rr::net::encode(start()));
            clients[k]->update(now, [](int32_t) { return uint16_t(0); });
            while (clients[k]->poll_event()) {
            }
        }
        rr::Game game;
        game.depth = rr::read_depth_table(exe);
        game.palette = rr::Palette::from_raw(assets.read("DATA/PALETTE.RAW"));
        game.course_name = "NAPA";
        rr::Settings settings;
        settings.network_hud = true;
        rr::DisplayDriver display{window, renderer, &settings};
        rr::RaceOptions options;
        options.online = clients[0].get();
        options.custom_keys = true;
        options.assets = &assets;
        options.settings = &settings;
        options.chat_messages[0] = "See you at the finish!";
        rr::Mixer mixer(48);
        rr::SoundSystem sound(assets, mixer, rr::read_volume_curve(exe), 2);
        sound.start_race(0, 2);
        game.sound = &sound;
        rr::RaceScreen screen(game, renderer, options, display);
        bool chat_sent = false;
        int corrections = 0;
        for (int t = 0; t < 90; ++t) {
            now = 1000 + uint64_t(t) * 17;
            for (size_t k = 0; k < 2; ++k) {
                while (auto bytes = endpoints[k]->poll()) {
                    auto message = rr::net::decode(*bytes);
                    if (message) {
                        if (auto ping = std::get_if<rr::net::Ping>(&*message))
                            endpoints[k]->send(rr::net::encode(rr::net::Pong{ping->t, t, uint32_t(now)}));
                        if (auto chat = std::get_if<rr::net::Chat>(&*message))
                            chat_sent = chat->slot == 0 && chat->text == options.chat_messages[0];
                    }
                }
                endpoints[k]->send(rr::net::encode(
                    rr::net::Frames{t,
                                    2,
                                    {{3,
                                      0,
                                      {uint16_t(rr::control_accelerate |
                                                ((t / 7) % 2 ? rr::control_left : rr::control_right)),
                                       0}}}}));
            }
            SDL_Event e{};
            if (t == 10 || t == 20) {
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.key = t == 10 ? SDLK_ESCAPE : SDLK_P;
                screen.event(e);
            }
            if (t == 30) {
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.key = SDLK_F1;
                screen.event(e);
            }
            if (t == 40) endpoints[0]->send(rr::net::encode(rr::net::ChatRelay{2, t, "Hello B"}));
            if (t == 50) {
                e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
                screen.event(e);
            }
            if (t == 60) {
                e.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
                screen.event(e);
            }
            clients[1]->update(now, [](int32_t) { return uint16_t(0); });
            check(screen.iterate(now * 1000000), "online screen continues while race is not official");
            if (t == 80)
                if (const char* path = std::getenv("RR_NET_SCREENSHOT")) {
                    rr::SurfacePtr surface(SDL_RenderReadPixels(renderer, nullptr));
                    check(surface && SDL_SaveBMP(surface.get(), path),
                          "online presentation screenshot written");
                }
            if (clients[0]->stats().rollback_depth > 0) ++corrections;
            check(rr::state_hash(*clients[0]->race()) == rr::state_hash(*clients[1]->race()),
                  "rendering/audio/quit/focus cannot change authoritative state");
            check(game.race().bike().i32(0x1c) == clients[0]->race()->race->riders()[1].bike.i32(0x1c),
                  "presentation follows local human seat");
        }
        check(corrections > 0, "screen handles real rollback corrections while rendering");
        check(chat_sent, "F1 sends configured chat slot");
        check(!screen.network_overlay()->lines().empty() &&
                  screen.network_overlay()->lines().back() == "A: Hello B",
              "relay enters persistent race log");
        check(clients[0]->rollback()->current_tick() > 80,
              "quit prompt and focus loss never pause network simulation");
        check(!screen.result().official, "no results screen before Official");
        const rr::net::Official official{true,
                                         {{2, rr::net::Outcome::finished, 2}, {5, rr::net::Outcome::won, 1}}};
        endpoints[0]->send(rr::net::encode(official));
        bool running = screen.iterate((now += 17) * 1000000);
        const int frozen = clients[0]->rollback()->current_tick();
        for (int i = 0; i < 10 && running; ++i) running = screen.iterate((now += 150) * 1000000);
        check(!running && screen.result().official == official &&
                  screen.result().outcome == rr::RaceOutcome::won,
              "official table decides result and completes fade");
        check(clients[0]->rollback()->current_tick() == frozen,
              "official race cannot step during result fade");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        check(false, "network presentation exception");
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return check.finish("net_presentation");
}
#endif
