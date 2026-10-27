// Plan 34 Task 3: rollback against a server-sealed input timeline.
//
// Race tick t consumes sealed frame t. Ticks below the networked countdown
// (0x168, 0x00442dd0) run Race::countdown_step; later ticks step the race
// with one word per seat. Before each simulated tick the state is saved
// into a ring of max_rollback + 1 snapshots. Unsealed ticks are predicted:
// the local seat uses its own sampled input, other seats repeat their last
// sealed word. When a sealed frame differs from what was predicted for an
// already-simulated tick (words, or which seats are human-driven), the next
// advance restores that tick's snapshot and re-simulates to the present.
// The simulation never runs more than max_rollback ticks past the newest
// contiguous sealed tick; beyond that the client stalls.
#pragma once
#include "race_state.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace rr::net {

class Rollback {
  public:
    Rollback(RaceInstance&, int humans, int local_slot, int max_rollback = 16, int countdown = 0x168);

    // The local seat's word for a tick (sampled once per simulated tick).
    void local_input(int32_t tick, uint32_t word);
    // One sealed frame: a word per seat (seat order), which seats are
    // human-driven (others are AI-driven: a departed human) and which words
    // the server substituted.
    void sealed(int32_t tick, std::span<const uint32_t> words, uint8_t late_mask,
                uint8_t present_mask = 0xff);
    // Simulates up to (not including) race tick `tick`, after any pending
    // correction. Returns the ticks re-simulated by a correction.
    int advance_to(int32_t tick);

    int32_t current_tick() const { return current_; }                   // next tick to simulate
    int32_t sealed_tick() const { return int32_t(frames_.size()) - 1; } // newest contiguous sealed tick
    int32_t confirmed_tick() const; // newest simulated tick with sealed inputs
    uint64_t confirmed_hash() const { return confirmed_hash_; }
    int32_t confirmed_hash_tick() const { return confirmed_hash_tick_; }
    bool stalled() const { return stalled_; }
    int last_rollback_depth() const { return last_depth_; }
    int max_rollback() const { return max_; }
    // Hash checks: every `hash_every` sealed ticks (tick + 1 multiple), the
    // state hash after that tick. Read them with take_hashes().
    struct TickHash {
        int32_t tick;
        uint64_t hash;
    };
    std::vector<TickHash> take_hashes();
    void set_hash_every(int n) { hash_every_ = n; }
    // The full sealed timeline so far (for results and reconnect logs).
    struct Frame {
        std::vector<uint32_t> words;
        uint8_t present = 0, late = 0;
    };
    const std::vector<Frame>& frames() const { return frames_; }

  private:
    struct Used { // what a simulated tick actually consumed
        std::vector<uint32_t> words;
        uint8_t present = 0;
        bool sealed = false;
    };
    void simulate_one();
    void inputs_for(int32_t tick, std::vector<uint32_t>& words, uint8_t& present, bool& sealed) const;

    RaceInstance& race_;
    int humans_, local_, max_, countdown_;
    int32_t current_ = 0;
    int32_t pending_ = -1; // earliest mispredicted simulated tick, -1 none
    bool stalled_ = false;
    int last_depth_ = 0;
    std::vector<Frame> frames_; // sealed, contiguous from tick 0
    std::vector<Frame> early_;  // sealed frames that arrived ahead of a gap
    std::vector<bool> early_have_;
    std::vector<uint32_t> local_words_; // local input by tick
    std::vector<Used> used_;            // per simulated tick (ring of max+1)
    std::vector<RaceState> snapshots_;  // ring of max+1: state before tick t at t % (max+1)
    std::vector<uint8_t> present_now_;  // seats currently human-driven in the race
    std::vector<uint32_t> scratch_;
    int hash_every_ = 60;
    std::vector<TickHash> hashes_;
    std::vector<std::pair<int32_t, uint64_t>> provisional_; // hash ticks run on predictions
    uint64_t confirmed_hash_ = 0;
    int32_t confirmed_hash_tick_ = -1;
};

} // namespace rr::net
