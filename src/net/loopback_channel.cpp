// Plan 34 Task 4: deterministic in-process network for client/server tests.
#include "net/channel.hpp"

#include <chrono>
#include <deque>
#include <map>
#include <mutex>

namespace rr::net {

uint64_t steady_ms() {
    using namespace std::chrono;
    return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

namespace {

// xorshift32: the simulated network has its own generator and never touches
// the simulation RNG.
struct Rng {
    uint32_t s;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    double unit() { return double(next() >> 8) / double(1u << 24); }
};

struct Link { // one direction
    std::multimap<uint64_t, std::vector<uint8_t>>
        in_flight; // delivery time -> payload (stable for equal keys)
    uint64_t last_delivery = 0;
};

struct Shared {
    std::mutex mu;
    NetSim sim;
    bool reliable;
    Clock clock;
    Rng rng;
    Link link[2]; // link[i] carries messages towards endpoint i
    bool closed = false;
};

class LoopbackChannel final : public Channel {
  public:
    LoopbackChannel(std::shared_ptr<Shared> s, int side) : s_(std::move(s)), side_(side) {}
    ~LoopbackChannel() override { close(); }
    bool reliable() const override { return s_->reliable; }
    void send(std::span<const uint8_t> bytes) override {
        std::lock_guard lock(s_->mu);
        if (s_->closed) return;
        Shared& s = *s_;
        Link& to = s.link[1 - side_];
        const uint64_t now = s.clock();
        auto latency = [&]() -> uint64_t {
            const int span = s.sim.max_ms - s.sim.min_ms;
            return uint64_t(s.sim.min_ms + (span > 0 ? int(s.rng.next() % uint32_t(span + 1)) : 0));
        };
        if (s.reliable) {
            const uint64_t at = std::max(now + latency(), to.last_delivery);
            to.last_delivery = at;
            to.in_flight.emplace(at, std::vector<uint8_t>(bytes.begin(), bytes.end()));
            return;
        }
        if (s.rng.unit() < s.sim.loss) return;
        to.in_flight.emplace(now + latency(), std::vector<uint8_t>(bytes.begin(), bytes.end()));
        if (s.rng.unit() < s.sim.duplicate)
            to.in_flight.emplace(now + latency(), std::vector<uint8_t>(bytes.begin(), bytes.end()));
    }
    std::optional<std::vector<uint8_t>> poll() override {
        std::lock_guard lock(s_->mu);
        Link& from = s_->link[side_];
        if (from.in_flight.empty() || from.in_flight.begin()->first > s_->clock()) return std::nullopt;
        std::vector<uint8_t> out = std::move(from.in_flight.begin()->second);
        from.in_flight.erase(from.in_flight.begin());
        return out;
    }
    ChannelStatus status() const override {
        std::lock_guard lock(s_->mu);
        return s_->closed ? ChannelStatus::closed : ChannelStatus::open;
    }
    void close() override {
        std::lock_guard lock(s_->mu);
        s_->closed = true;
    }

  private:
    std::shared_ptr<Shared> s_;
    int side_;
};

} // namespace

std::pair<std::unique_ptr<Channel>, std::unique_ptr<Channel>> loopback_pair(NetSim sim, bool reliable,
                                                                            Clock clock) {
    auto s = std::make_shared<Shared>();
    s->sim = sim;
    s->reliable = reliable;
    s->clock = clock ? std::move(clock) : Clock(steady_ms);
    s->rng.s = sim.seed ? sim.seed : 1;
    return {std::make_unique<LoopbackChannel>(s, 0), std::make_unique<LoopbackChannel>(s, 1)};
}

} // namespace rr::net
