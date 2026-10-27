#pragma once
#include "sound_event.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <span>
#include <utility>

namespace rr::net {
// Playback history is outside snapshots. A sound that survives correction is
// never played twice; one removed by correction is allowed to finish. Stable
// emitter IDs distinguish two riders producing the same event on the same tick.
class SoundHistory {
  public:
    explicit SoundHistory(int window = 16) : window_(std::max(0, window)) {}
    template <class Play> void deliver(int32_t tick, std::span<const SoundEventRecord> sounds, Play play) {
        newest_ = std::max(newest_, tick);
        while (!played_.empty() && played_.begin()->first < newest_ - window_) played_.erase(played_.begin());
        if (tick < newest_ - window_) return;
        auto& keys = played_[tick];
        for (const auto& sound : sounds) {
            const auto key = std::make_pair(sound.source, sound.code);
            if (keys.size() < 64 && keys.insert(key).second) play(sound);
        }
    }
    size_t ticks_held() const { return played_.size(); }

  private:
    int window_;
    int32_t newest_ = -1;
    std::map<int32_t, std::set<std::pair<int32_t, SoundEvent>>> played_;
};
} // namespace rr::net
