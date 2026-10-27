// Plan 34 Task 3: rollback core.
#include "net/rollback.hpp"

#include <algorithm>

namespace rr::net {

Rollback::Rollback(RaceInstance& race, int humans, int local_slot, int max_rollback, int countdown)
    : race_(race), humans_(std::clamp(humans, 1, 8)), local_(local_slot), max_(std::max(max_rollback, 0)),
      countdown_(countdown), used_(size_t(max_ + 1)), snapshots_(size_t(max_ + 1)) {
    for (auto& u : used_) u.words.assign(size_t(humans_), 0);
    scratch_.assign(size_t(humans_), 0);
}

void Rollback::local_input(int32_t tick, uint32_t word) {
    if (tick < 0) return;
    if (size_t(tick) >= local_words_.size()) local_words_.resize(size_t(tick) + 1, 0);
    local_words_[size_t(tick)] = word;
}

void Rollback::sealed(int32_t tick, std::span<const uint32_t> words, uint8_t late_mask,
                      uint8_t present_mask) {
    if (tick < int32_t(frames_.size()) || words.size() < size_t(humans_)) return; // duplicate or malformed
    Frame f;
    f.words.assign(words.begin(), words.begin() + humans_);
    f.present = uint8_t(present_mask & ((1u << humans_) - 1u));
    f.late = late_mask;
    const size_t offset = size_t(tick) - frames_.size();
    if (offset > 0) { // ahead of a gap: hold until the gap is filled
        if (early_.size() < offset) {
            early_.resize(offset);
            early_have_.resize(offset, false);
        }
        early_[offset - 1] = std::move(f);
        early_have_[offset - 1] = true;
        return;
    }
    auto accept = [&](Frame frame) {
        const int32_t t = int32_t(frames_.size());
        frames_.push_back(std::move(frame));
        if (t >= current_) return; // not simulated yet
        const Used& u = used_[size_t(t % (max_ + 1))];
        const Frame& s = frames_.back();
        if (u.words != s.words || u.present != s.present) {
            if (pending_ < 0 || t < pending_) pending_ = t;
        } else if ((t + 1) % hash_every_ == 0 && t > confirmed_hash_tick_ && pending_ < 0) {
            // The prediction was right: the hash computed then is confirmed.
            for (const auto& [pt, h] : provisional_)
                if (pt == t) {
                    confirmed_hash_ = h;
                    confirmed_hash_tick_ = t;
                    hashes_.push_back({t, h});
                }
        }
        std::erase_if(provisional_, [&](const auto& p) { return p.first <= confirmed_hash_tick_; });
    };
    accept(std::move(f));
    // Flush frames that were waiting behind the gap.
    while (!early_.empty() && early_have_.front()) {
        Frame next = std::move(early_.front());
        early_.erase(early_.begin());
        early_have_.erase(early_have_.begin());
        accept(std::move(next));
    }
    if (!early_.empty()) {
        early_.erase(early_.begin());
        early_have_.erase(early_have_.begin());
    }
}

void Rollback::inputs_for(int32_t t, std::vector<uint32_t>& words, uint8_t& present, bool& sealed) const {
    if (t < int32_t(frames_.size())) {
        const Frame& f = frames_[size_t(t)];
        words = f.words;
        present = f.present;
        sealed = true;
        return;
    }
    sealed = false;
    if (!frames_.empty()) {
        words = frames_.back().words; // repeat-last for every seat
        present = frames_.back().present;
    } else {
        std::fill(words.begin(), words.end(), 0u);
        present = uint8_t((1u << humans_) - 1u);
    }
    if (local_ >= 0 && local_ < humans_) {
        if (size_t(t) < local_words_.size())
            words[size_t(local_)] = local_words_[size_t(t)];
        else if (!local_words_.empty())
            words[size_t(local_)] = local_words_.back();
    }
}

void Rollback::simulate_one() {
    const int32_t t = current_;
    const size_t slot = size_t(t % (max_ + 1));
    save_state(race_, snapshots_[slot]);
    Used& u = used_[slot];
    inputs_for(t, u.words, u.present, u.sealed);
    Race& race = *race_.race;
    // A seat's human/AI switch is simulation input, applied on its sealed tick.
    for (int seat = 0; seat < humans_ && seat < race.humans(); ++seat) {
        const bool driven = (u.present >> seat) & 1;
        const bool now = !race.riders()[size_t(race.human_rider(seat))].ai;
        if (driven != now) race.set_human_driven(seat, driven);
    }
    if (t < countdown_)
        race.countdown_step();
    else
        race.step(u.words);
    ++current_;
    // State hash after every hash_every-th tick: confirmed when the tick ran
    // on sealed inputs; provisional ones are checked again when sealed.
    if ((t + 1) % hash_every_ == 0 && t > confirmed_hash_tick_) {
        const uint64_t h = state_hash(race_);
        if (u.sealed) {
            confirmed_hash_ = h;
            confirmed_hash_tick_ = t;
            hashes_.push_back({t, h});
        } else {
            std::erase_if(provisional_, [&](const auto& p) { return p.first == t; });
            provisional_.push_back({t, h});
        }
    }
}

int Rollback::advance_to(int32_t target) {
    int resimulated = 0;
    last_depth_ = 0;
    if (pending_ >= 0) {
        const int32_t from = pending_, end = current_;
        pending_ = -1;
        load_state(race_, snapshots_[size_t(from % (max_ + 1))]);
        current_ = from;
        // Provisional hashes of the mispredicted ticks are void (confirmed
        // hashes are only ever taken on sealed inputs and stay valid).
        std::erase_if(provisional_, [&](const auto& p) { return p.first >= from; });
        while (current_ < end) {
            simulate_one();
            ++resimulated;
        }
        last_depth_ = resimulated;
    }
    stalled_ = false;
    while (current_ < target) {
        if (current_ > sealed_tick() + max_) { // the window is full: wait for sealed frames
            stalled_ = true;
            break;
        }
        simulate_one();
    }
    return resimulated;
}

int32_t Rollback::confirmed_tick() const {
    const int32_t simulated = std::min(sealed_tick(), current_ - 1);
    return pending_ >= 0 ? std::min(simulated, pending_ - 1) : simulated;
}

std::vector<Rollback::TickHash> Rollback::take_hashes() {
    std::vector<TickHash> out;
    out.swap(hashes_);
    return out;
}

} // namespace rr::net
