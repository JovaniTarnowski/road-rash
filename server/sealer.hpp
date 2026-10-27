// Plan 34 Task 5: the input sealer. For each race tick it publishes one
// authoritative frame: every seat's control word, which seats are
// human-driven, and which words were substituted because the input was late.
// Pure logic: the caller supplies the tick to seal up to; no clock, no I/O.
#pragma once
#include "net/protocol.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace rr::netd {

class Sealer {
  public:
    explicit Sealer(int humans);

    int humans() const { return humans_; }
    // Newest sealed tick, -1 before the first.
    int32_t sealed_tick() const { return int32_t(log_.size()) - 1; }
    const std::vector<net::SealedFrame>& log() const { return log_; }

    // Inputs for ticks already sealed, or more than max_ahead past the newest
    // sealed tick, are dropped and counted. A repeated tick keeps the first
    // word received (a client cannot rewrite its input).
    void input(int seat, int32_t first_tick, std::span<const uint16_t> words);
    // Seat becomes AI-driven from the next sealed tick (disconnect / leave),
    // or human-driven again (reconnected and caught up).
    void set_human(int seat, bool human);
    bool human(int seat) const { return human_[size_t(seat)]; }

    // Seals ticks up to and including `tick`. Returns how many were sealed.
    int seal_through(int32_t tick);

    // Frames [first, first+count) of the log as a Frames message.
    net::Frames frames(int32_t first, int count) const;

    struct Counters {
        uint64_t sealed = 0, late = 0, dropped_old = 0, dropped_ahead = 0, ai_ticks = 0;
    };
    const Counters& counters() const { return counters_; }

    static constexpr int32_t max_ahead = 240; // 4 s of buffered future input per seat

  private:
    int humans_;
    std::vector<net::SealedFrame> log_;
    std::vector<std::map<int32_t, uint16_t>> pending_; // per seat, future ticks
    std::vector<uint16_t> last_word_;
    std::vector<bool> human_;
    std::vector<bool> human_next_;
    Counters counters_;
};

} // namespace rr::netd
