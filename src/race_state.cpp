// Plan 34 Task 3: race snapshots and the state hash.
#include "race_state.hpp"

namespace rr {
namespace {

template <class T> void copy_into(std::optional<T>& slot, const T& from) {
    if (slot)
        *slot = from; // copy-assignment reuses the slot's capacity
    else
        slot.emplace(from);
}

struct Fnv {
    uint64_t h = 0xcbf29ce484222325ull;
    void byte(uint8_t b) {
        h ^= b;
        h *= 0x100000001b3ull;
    }
    void i64(int64_t v) {
        for (int i = 0; i < 8; ++i) byte(uint8_t(uint64_t(v) >> (8 * i)));
    }
    template <size_t N> void bytes(const std::array<uint8_t, N>& a) {
        for (uint8_t b : a) byte(b);
    }
};

} // namespace

void save_state(const RaceInstance& in, RaceState& out) {
    copy_into(out.race, *in.race);
    if (in.traffic) copy_into(out.traffic, *in.traffic);
    if (in.pedestrians) copy_into(out.pedestrians, *in.pedestrians);
    if (in.ring) copy_into(out.ring, *in.ring);
    out.seat_rings.resize(in.seat_rings.size());
    for (size_t i = 0; i < in.seat_rings.size(); ++i) copy_into(out.seat_rings[i], *in.seat_rings[i]);
}

void load_state(RaceInstance& in, const RaceState& st) {
    *in.race = *st.race;
    if (in.traffic && st.traffic) *in.traffic = *st.traffic;
    if (in.pedestrians && st.pedestrians) *in.pedestrians = *st.pedestrians;
    if (in.ring && st.ring) *in.ring = *st.ring;
    for (size_t i = 0; i < in.seat_rings.size() && i < st.seat_rings.size(); ++i)
        if (st.seat_rings[i]) *in.seat_rings[i] = *st.seat_rings[i];
}

uint64_t state_hash(const RaceInstance& in) {
    Fnv f;
    const Race& race = *in.race;
    const RaceSnapshot snap = race.snapshot(); // rider, AI and car fields of the plan 10 oracle
    f.i64(snap.tick);
    f.i64(snap.rng);
    for (const auto& field : snap.fields) f.i64(field.value);
    for (const Rider& r : race.riders()) {
        f.bytes(r.bike.m);
        f.bytes(r.half_object.m);
        f.i64(int64_t(r.sample));
        f.i64(r.route);
        f.i64(r.ai);
        f.i64(r.finished);
        f.i64(r.finish_tick);
        f.i64(r.place);
        f.i64(r.parked);
        f.i64(r.live);
    }
    if (in.pedestrians)
        for (const Pedestrian& p : in.pedestrians->pool()) {
            f.i64(p.active);
            if (!p.active) continue;
            f.bytes(p.body.bike.m);
            f.i64(p.state);
            f.i64(p.type);
            f.i64(p.side);
            f.i64(p.heading);
            f.i64(p.pace);
        }
    if (in.ring) {
        f.i64(in.ring->front_q8());
        f.i64(in.ring->back_q8());
    }
    for (const auto& ring : in.seat_rings) {
        f.i64(ring->front_q8());
        f.i64(ring->back_q8());
    }
    return f.h;
}

} // namespace rr
