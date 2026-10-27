#pragma once
#include "net/channel.hpp"
#include "net/protocol.hpp"
#include "net/rollback.hpp"
#include "net/effects.hpp"
#include <deque>
#include <functional>
#include <memory>

namespace rr::net {
// StartHuman.seat is a room identity; simulation seats are compact roster order.
RaceConfig race_config(const Start&);
// Main-thread client. The factory keeps assets alive externally and constructs
// the same race from Start on every peer. Clock values are monotonic milliseconds.
class Client {
  public:
    using Factory = std::function<std::unique_ptr<RaceSetup>(const Start&)>;
    using Sampler = std::function<uint16_t(int32_t)>;
    struct Stats {
        double rtt_ms = 0, jitter_ms = 0, rate = 60;
        int lead = 1, input_delay = 0, rollback_depth = 0;
        uint64_t stalls = 0, late = 0, malformed = 0;
    };
    Client(std::unique_ptr<Channel>, Hello, Factory, int window = 16, int max_delay = 3);
    // Rollback observers capture this client; its address must stay stable.
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(Client&&) = delete;
    void attach_unreliable(std::unique_ptr<Channel>);
    void send(const Message&); // lobby/control, always reliable
    void update(uint64_t now_ms, const Sampler&, int32_t stop_before = 216000);
    std::optional<Message> poll_event();
    Rollback* rollback() { return rollback_.get(); }
    RaceInstance* race() { return setup_ ? &setup_->instance : nullptr; }
    const Stats& stats() const { return stats_; }
    size_t pending_inputs() const { return inputs_.size(); }
    size_t pending_events() const { return events_.size(); }
    bool failed() const { return failed_; }
    const Start* start() const { return start_ ? &*start_ : nullptr; }
    // RaceScreen may move the immutable assets into Game. Game must then
    // retain them until this race is replaced or the client is destroyed.
    RaceSetup* setup() { return setup_.get(); }
    std::vector<SoundEventRecord> take_sounds() {
        std::vector<SoundEventRecord> out;
        out.swap(sounds_);
        return out;
    }
    struct OutcomeEvent {
        int32_t tick;
        int slot;
        Race::HumanOutcome outcome;
    };
    std::optional<OutcomeEvent> poll_outcome();
    // Synthetic-cutoff harnesses report their own Result. Production reports
    // a common terminal tick/hash automatically, exclusively from confirmed data.
    void set_automatic_results(bool enabled) { automatic_results_ = enabled; }

  private:
    void receive(const Message&, uint64_t);
    void race_send(const Message&);
    std::unique_ptr<Channel> reliable_, unreliable_;
    Hello hello_;
    Factory factory_;
    std::unique_ptr<RaceSetup> setup_;
    std::unique_ptr<Rollback> rollback_;
    std::optional<Start> start_;
    std::deque<std::pair<int32_t, uint16_t>> inputs_;
    std::deque<Message> events_;
    std::deque<double> samples_;
    SoundHistory sound_history_;
    std::vector<SoundEventRecord> sounds_;
    std::deque<OutcomeEvent> outcomes_;
    std::array<int, 8> outcome_codes_{};
    bool automatic_results_ = true, result_sent_ = false;
    Stats stats_;
    int window_, delay_cap_;
    int32_t sampled_ = -1;
    uint64_t last_ms_ = 0, next_ping_ = 0, next_send_ = 0;
    std::optional<uint32_t> ping_;
    double clock_tick_ = 0, remote_tick_ = 0;
    uint64_t remote_ms_ = 0;
    bool welcomed_ = false, hello_sent_ = false, synced_ = false, failed_ = false, finished_ = false;
};
} // namespace rr::net
