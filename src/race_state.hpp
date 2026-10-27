// Plan 34 Task 3: race snapshots for rollback. A RaceState holds copies of
// every mutable part of a RaceInstance (Race, traffic, pedestrians, every
// ring window). Immutable data (routes, tables, course, assets) is shared or
// referenced, never copied; the large mutable tables (ring statics, ring
// terrain, ring DZHR lists) are copy-on-write, so a save copies a few tens of
// KB and, once its slots have grown, allocates nothing.
//
// Restoring copies the state back into the same live objects, so the
// pointers between them (Race -> traffic/pedestrians/rings, the ring sinks
// that capture the live Race) stay valid without rebinding. A RaceState is
// only ever loaded into the instance it was saved from.
#pragma once
#include "race_setup.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace rr {

struct RaceState {
    std::optional<Race> race;
    std::optional<TrafficSystem> traffic;
    std::optional<PedestrianSystem> pedestrians;
    std::optional<TrackRing> ring;
    std::vector<std::optional<TrackRing>> seat_rings;
};

void save_state(const RaceInstance&, RaceState&);
void load_state(RaceInstance&, const RaceState&);
// 64-bit FNV-1a over the live simulation state: tick, RNG, every rider's
// object images and standings, cars, pedestrians and ring cursors. Uses only
// fixed-width integers in a fixed order, so equal states hash equally on
// every platform.
uint64_t state_hash(const RaceInstance&);

} // namespace rr
