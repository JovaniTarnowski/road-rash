#include "assets.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "net/client.hpp"
#include "room.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>

namespace {
std::unique_ptr<rr::RaceSetup> make(const rr::Assets& assets, const std::vector<uint8_t>& exe,
                                    const rr::net::Start& s) {
    return std::make_unique<rr::RaceSetup>(rr::make_race_setup(assets, exe, rr::net::race_config(s)));
}
void scenario(rr::test::Checks& check, const rr::Assets& assets, const std::vector<uint8_t>& exe, int humans,
              int rtt, double loss, bool terminal = false) {
    using namespace rr::net;
    uint64_t now = 1000;
    rr::netd::ServerConfig cfg;
    cfg.seed = 42;
    cfg.load_ms = 500;
    cfg.min_humans = humans;
    rr::netd::Server server(cfg);
    server.on_race_finished = [&](const rr::netd::RaceRecord& record) {
        check(record.hashes.size() == size_t(terminal ? 0 : 8),
              "all eight periodic hash checks reached server");
        for (const auto& [tick, seats] : record.hashes) {
            (void)tick;
            check(seats.size() == size_t(humans), "every seat supplied every hash check");
            for (const auto& [seat, hash] : seats) {
                (void)seat;
                check(hash == seats.begin()->second, "all sealed tick hashes equal at every check");
            }
        }
    };
    std::vector<std::unique_ptr<Client>> clients;
    for (int k = 0; k < humans; ++k) {
        NetSim sim{std::max(0, rtt / 2 - 30), rtt / 2 + 30, loss, .1, uint32_t(k + 99)};
        auto reliable = loopback_pair(sim, true, [&] { return now; });
        auto id = server.add_connection(std::move(reliable.second), now);
        auto c = std::make_unique<Client>(
            std::move(reliable.first),
            Hello{protocol_version, 123, {}, Platform::bot, "P" + std::to_string(k), {}},
            [&](const Start& s) {
                auto setup = make(assets, exe, s);
                if (terminal)
                    for (auto& rider : setup->instance.race->riders()) rider.bike.i32(0x2f0) = 0;
                return setup;
            });
        c->set_automatic_results(terminal);
        auto unreliable = loopback_pair(sim, false, [&] { return now; });
        server.attach_unreliable(id, std::move(unreliable.second));
        c->attach_unreliable(std::move(unreliable.first));
        clients.push_back(std::move(c));
    }
    const int end = terminal ? 1 : 480;
    std::string code;
    std::vector<bool> joined(size_t(humans), false), ready(size_t(humans), false);
    std::vector<std::map<int32_t, uint64_t>> hashes(size_t(humans), std::map<int32_t, uint64_t>{});
    std::vector<bool> reported(size_t(humans), false), official(size_t(humans), false);
    int deepest = 0;
    uint64_t stalls = 0;
    bool done = false, created = false;
    std::optional<Official> result;
    for (; now < 35000; now += 8) {
        server.step(now);
        for (int k = 0; k < humans; ++k) {
            auto& c = *clients[size_t(k)];
            c.update(
                now + 100000 + uint64_t(k) * 1000 + now * uint64_t(k) / 10000,
                [k](int32_t t) {
                    return uint16_t(rr::control_accelerate |
                                    ((t / (25 + k)) % 2 ? rr::control_left : rr::control_right));
                },
                end);
            while (auto m = c.poll_event()) {
                if (auto e = std::get_if<Reject>(&*m))
                    std::cerr << "REJECT " << k << " " << int(e->reason) << " at " << now << "\n";
                if (auto e = std::get_if<Desync>(&*m)) std::cerr << "DESYNC " << k << " " << e->tick << "\n";
                if (std::holds_alternative<Welcome>(*m) && k == 0 && !created) {
                    c.send(RoomCreate{RoomOptions{0, 0, false, true}});
                    created = true;
                }
                if (auto rs = std::get_if<RoomState>(&*m)) {
                    if (k == 0) code = rs->code;
                    if (!ready[size_t(k)]) {
                        c.send(PickCharacter{uint8_t(k + 1)});
                        c.send(VoteCourse{4});
                        c.send(Ready{});
                        ready[size_t(k)] = true;
                    }
                }
                if (auto o = std::get_if<Official>(&*m)) {
                    check(o->agreed, "official hashes agreed");
                    if (terminal)
                        for (const auto& row : o->rows)
                            check(row.outcome == Outcome::wrecked,
                                  "automatic confirmed results preserve outcomes");
                    if (result) check(*result == *o, "all final result tables equal");
                    result = *o;
                    official[size_t(k)] = true;
                }
            }
            if (k && !code.empty() && !joined[size_t(k)]) {
                c.send(RoomJoin{code, false});
                joined[size_t(k)] = true;
            }
            if (auto r = c.rollback()) {
                // Inspect a confirmed state at a common cutoff; predicted ticks
                // beyond it are forbidden by update's stop argument.
                deepest = std::max(deepest, c.stats().rollback_depth);
                check(c.stats().rate >= 59 && c.stats().rate <= 61, "time dilation stays within 59..61 Hz");
                check(c.stats().input_delay >= 0 && c.stats().input_delay <= 3,
                      "adaptive delay stays capped");
                check(c.pending_inputs() <= size_t(r->max_rollback() + 1), "bounded resend queue");
                if (r->confirmed_tick() >= end - 1 && !reported[size_t(k)]) {
                    hashes[size_t(k)][end - 1] = rr::state_hash(*c.race());
                    if (!terminal) c.send(Result{end - 1, Outcome::left, 0, hashes[size_t(k)][end - 1]});
                    reported[size_t(k)] = true;
                }
            }
        }
        done = std::all_of(official.begin(), official.end(), [](bool b) { return b; });
        if (done) break;
    }
    check(done, "all clients receive official synthetic-cutoff results");
    for (int k = 0; k < humans; ++k) {
        auto& c = *clients[size_t(k)];
        check(!c.failed(), "client has no protocol/desync failure");
        check(hashes[size_t(k)] == hashes[0] && !hashes[0].empty(), "equal final confirmed state");
        check(c.pending_events() <= 128, "bounded event queue");
        stalls += c.stats().stalls;
    }
    check(deepest <= 16, "rollback window respected");
    check(server.counters().desyncs == 0, "every server hash comparison agrees");
    std::cout << humans << " clients RTT " << rtt << " loss " << loss << " depth " << deepest << " stalls "
              << stalls << std::endl;
}
} // namespace
int main() {
    const std::string root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(root + "/ROADRASH.EXE")) return rr::golden::skip;
    rr::Assets assets(root);
    auto exe = assets.read("ROADRASH.EXE");
    rr::test::Checks check;
    // Hostile/flooded channel input cannot grow the event queue indefinitely.
    {
        auto link = rr::net::loopback_pair({}, true);
        rr::net::Client client(std::move(link.first), {},
                               [](const rr::net::Start&) { return std::unique_ptr<rr::RaceSetup>{}; });
        link.second->send(std::vector<uint8_t>{0xff});
        for (int i = 0; i < 200; ++i) link.second->send(rr::net::encode(rr::net::Welcome{}));
        client.update(1000, [](int32_t) { return uint16_t(0); });
        check(client.failed() && client.pending_events() == 128, "flood fails with bounded event queue");
        check(client.stats().malformed == 1, "malformed wire message counted");
    }
    {
        auto link = rr::net::loopback_pair({}, true);
        rr::net::Client client(std::move(link.first), {},
                               [](const rr::net::Start&) { return std::unique_ptr<rr::RaceSetup>{}; });
        link.second->close();
        client.update(1000, [](int32_t) { return uint16_t(0); });
        check(client.failed(), "closed control channel is surfaced");
    }
    for (int n : {2, 3, 4, 5, 6, 7, 8})
        for (int rtt : {0, 50, 150, 300})
            for (double loss : {0., .05, .20}) scenario(check, assets, exe, n, rtt, loss);
    for (int n : {2, 8}) scenario(check, assets, exe, n, 150, .20, true);
    return check.finish("net_client");
}
