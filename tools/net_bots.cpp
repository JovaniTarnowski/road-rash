// Real WebSocket clients for local/staging smoke tests. Each owns a full
// disc-backed Race and Rollback. --ticks terminates at a shared sealed cutoff
// and reports Outcome::left: this is a transport/determinism test, not a claim
// that scripted riders completed the course.
#include "assets.hpp"
#include "net/client.hpp"
#include "sha256.hpp"
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    try {
        std::string url = "ws://127.0.0.1:8080/net";
        std::string root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
        int count = 2, ticks = 600, timeout = 90;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--help") {
                std::cout << "net-bots [--url ws://host/net] [--disc ROADRASH] [--clients 2..8] [--ticks N] "
                             "[--timeout seconds]\n";
                return 0;
            }
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            std::string v = argv[++i];
            if (a == "--url")
                url = v;
            else if (a == "--disc")
                root = v;
            else if (a == "--clients")
                count = std::stoi(v);
            else if (a == "--ticks")
                ticks = std::stoi(v);
            else if (a == "--timeout")
                timeout = std::stoi(v);
            else
                throw std::runtime_error("unknown option " + a);
        }
        if (count < 2 || count > 8 || ticks < 1 || ticks > 216000 || timeout < 1)
            throw std::runtime_error("invalid limits");
        rr::Assets assets(root);
        auto exe = assets.read("ROADRASH.EXE");
        if (exe.empty()) throw std::runtime_error("disc executable not found");
        using namespace rr::net;
        auto factory = [&](const Start& s) {
            return std::make_unique<rr::RaceSetup>(rr::make_race_setup(assets, exe, race_config(s)));
        };
        // Diagnostic bots only admit peers built from the same source revision
        // and executable. The game startup compatibility gate is Task 7 work.
        Hello hello;
        hello.platform = Platform::bot;
        const auto digest = rr::sha256(exe);
        for (size_t i = 0; i < hello.data_digest.size(); ++i)
            hello.data_digest[i] = uint8_t(std::stoul(digest.substr(i * 2, 2), nullptr, 16));
        std::string revision = RR_BOT_SOURCE_HASH;
        hello.sim_fingerprint = std::stoull(revision.substr(0, 16), nullptr, 16);
        std::vector<std::unique_ptr<Client>> clients;
        for (int k = 0; k < count; ++k) {
            hello.alias = "Bot" + std::to_string(k + 1);
            clients.push_back(std::make_unique<Client>(ws_connect(url), hello, factory));
        }
        for (auto& client : clients) client->set_automatic_results(false);
        std::string code;
        bool created = false;
        std::vector<bool> joined(size_t(count), false), ready(size_t(count), false),
            reported(size_t(count), false), official(size_t(count), false);
        std::optional<uint64_t> final_hash;
        const uint64_t begin = steady_ms();
        while (steady_ms() - begin < uint64_t(timeout) * 1000) {
            uint64_t now = steady_ms();
            for (int k = 0; k < count; ++k) {
                auto& c = *clients[size_t(k)];
                c.update(
                    now,
                    [k](int32_t t) {
                        return uint16_t(rr::control_accelerate |
                                        ((t / (43 + 7 * k)) % 3 == 1 ? rr::control_left : 0));
                    },
                    ticks);
                while (auto m = c.poll_event()) {
                    if (std::holds_alternative<Welcome>(*m) && k == 0 && !created) {
                        c.send(RoomCreate{RoomOptions{0, 2, true, false}});
                        created = true;
                    }
                    if (auto s = std::get_if<RoomState>(&*m)) {
                        if (k == 0) code = s->code;
                        const int occupied =
                            int(std::count_if(s->seats.begin(), s->seats.end(),
                                              [](const SeatInfo& seat) { return seat.occupied; }));
                        // Wait for the complete roster before readying any seat.
                        if (occupied == count && !ready[size_t(k)]) {
                            c.send(PickCharacter{uint8_t(k + 1)});
                            c.send(VoteCourse{4});
                            c.send(Ready{});
                            ready[size_t(k)] = true;
                        }
                    }
                    if (auto o = std::get_if<Official>(&*m)) {
                        if (!o->agreed || o->rows.size() != size_t(count))
                            throw std::runtime_error("unofficial or incomplete result");
                        official[size_t(k)] = true;
                    }
                }
                if (c.failed()) throw std::runtime_error("server rejection, desync or client queue overflow");
                if (k && !code.empty() && !joined[size_t(k)]) {
                    c.send(RoomJoin{code, false});
                    joined[size_t(k)] = true;
                }
                if (auto r = c.rollback(); r && r->confirmed_tick() >= ticks - 1 && !reported[size_t(k)]) {
                    const uint64_t h = rr::state_hash(*c.race());
                    if (final_hash && *final_hash != h) throw std::runtime_error("final hash mismatch");
                    final_hash = h;
                    c.send(Result{ticks - 1, Outcome::left, 0, h});
                    reported[size_t(k)] = true;
                    std::cout << "bot " << k << " confirmed " << ticks << " hash " << h << " stalls "
                              << c.stats().stalls << "\n";
                }
            }
            if (std::all_of(official.begin(), official.end(), [](bool b) { return b; })) {
                std::cout << "net-bots OK: official cutoff results in room " << code << "\n";
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        throw std::runtime_error("timed out waiting for official cutoff results");
    } catch (const std::exception& e) {
        std::cerr << "net-bots: " << e.what() << "\n";
        return 1;
    }
}
