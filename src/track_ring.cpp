#include "track_ring.hpp"
#include <algorithm>
#include <stdexcept>

namespace rr {
namespace {
const CourseNode& node_of(const Course& course, uint32_t offset) {
    const auto it = course.nodes.find(offset);
    if (it == course.nodes.end()) throw std::runtime_error("Track ring: missing course node");
    return it->second;
}
}

TrackRing::TrackRing(const Course& course, int level)
    : course_(course), level_(level), events_(course.sections.size()),
      forward_([this] { return (*random_)(); }) {
    if (level < 0 || level > 4) throw std::runtime_error("Track ring: invalid level");
    for (size_t s = 0; s < course.sections.size(); ++s) {
        events_[s].section = uint32_t(s);
        events_[s].records = course.sections[s].scenery_events;
    }
}

// 0x00453e60: depth-first over the course graph. A road node jitters its
// section's DZHR records (0x00451ee0) and sorts them (0x00451fc0); a split
// visits +8 then +12, a merge +16. Each node is visited once.
void TrackRing::jitter_events(uint32_t offset, std::vector<uint32_t>& visited, const Random& random) {
    for (;;) {
        if (std::find(visited.begin(), visited.end(), offset) != visited.end()) return;
        const auto& node = node_of(course_, offset);
        if (node.kind > 2) return;
        visited.push_back(offset);
        if (node.kind == 1) {
            jitter_events(node.next, visited, random);
            offset = node.previous;
            continue;
        }
        if (node.kind == 2) { offset = node.link; continue; }
        auto& records = events_.at(node.link).records;
        int32_t base = 0;
        for (auto& r : records) {   // 0x00451ee0
            const uint32_t word = r[0];
            base += int32_t((word & 0x7f000000u) >> 24);
            const int32_t draw = int32_t(random());
            const int bonus = (!(word & 0x80000000u) || (word & 15) < 0xb) ? level_ : level_ - 4;
            const uint32_t sample = uint32_t(draw % int32_t(((word & 0xf0000u) >> 15) + 1) + base);
            r[0] = ((sample * 0x10000u ^ word) & 0x7fff0000u) ^ word;
            const uint32_t required = (r[1] & 0x7000u) >> 12;
            bool enabled = !(required != 0 && level_ - int(required) != -1);
            if (enabled && int(((word & 0xf00000u) >> 20) + uint32_t(bonus)) < int32_t(random()) % 10) enabled = false;
            if (!enabled) r[0] |= 0x3f00u;
        }
        for (size_t i = 0; i + 1 < records.size(); ++i)   // 0x00451fc0 pairwise swaps
            for (size_t j = i + 1; j < records.size(); ++j)
                if ((records[j][0] & 0x7fff0000u) < (records[i][0] & 0x7fff0000u)) std::swap(records[i], records[j]);
        offset = node.next;
    }
}

void TrackRing::Borr::load(int32_t index) {
    const uint32_t word = (*words).at(size_t(index));
    const uint8_t top = uint8_t(word >> 24);
    alternate = top >> 7;
    left = (top >> 4) & 1;
    selector = uint8_t(word >> 16);
    random = (word >> 11) & 1;
    count = (word >> 8) & 7;
    units = top & 15;
    if ((word & 0x0f000000u) == 0) {
        const int32_t amount = int32_t(((word & 0xf000u) >> 8) + (word & 15u));
        spacing = (word & 0x20000000u) ? (amount + 0x20) * 0x20 : amount * 8;
        period = 0;
    } else {
        spacing = (word & 0x20000000u) ? int32_t((word & 15u) + 4) * 0x100 : int32_t(word & 15u) << 6;
        period = (word >> 12) & 15;
    }
}

void TrackRing::Borr::init(const std::vector<uint32_t>* data) {
    words = data && !data->empty() ? data : nullptr;
    if (!words) return;
    record = 0; position = 0;
    load(0);
    start = 0; current = 0;
    end = (period + 1) * units + (position >> 8);
}

void TrackRing::Borr::step(int32_t delta) {
    if (!words) return;
    const int32_t old = position;
    position += delta;
    const int32_t sample = position >> 8;
    if (sample == old >> 8) return;
    current = sample;
    if (delta < 1) return;   // backward walks are not ported yet
    if (end < sample) {
        if (int32_t(words->size()) - 1 <= record) { selector = 0x3f; return; }
        load(++record);
        start = position >> 8;
        end = (period + 1) * units + (position >> 8);
    }
}

void TrackRing::Borr::slots(const PathSample&, const Random& draw) const {
    if (!words) return;
    if (period != 0 && (current - start) % (period + 1) != 0) return;
    if ((selector & 0x3f) == 0x3f) return;
    if (!random) return;
    for (int i = 0; i <= count; ++i) draw();   // slot spacing spacing/2 + rand % (spacing - spacing/2 + 1)
}

void TrackRing::enter(Walker& w, uint32_t offset) {
    // 0x00452cb0 with param_3 = 1: position 0 in the node's section.
    const auto& node = node_of(course_, offset);
    const auto& section = course_.sections.at(node.link);
    w.node = offset;
    w.position = 0;
    w.borr.init(&section.roadside_profiles);
    w.terrain.reset();
    if (section.terrain.kind != TerrainDefinition::Kind::none) w.terrain.emplace(section.terrain, 0, forward_);
    w.dzhr = {size_t(node.link), 0, 0};
}

void TrackRing::step(Walker& w, int32_t delta) {
    // 0x00453030 -> 0x00452e10, single road, forward.
    if (w.stopped) return;
    const auto& node = node_of(course_, w.node);
    int32_t next = w.position + delta;
    if (next < 0) { w.stopped = unsupported_ = true; return; }
    if (next >= int32_t(node.length) * 0x100) {
        const auto& following = node_of(course_, node.next);
        if (following.kind != 0) { w.stopped = unsupported_ = true; return; }
        next -= int32_t(node.length) * 0x100;
        enter(w, node.next);
        delta = next;
    }
    w.position = next;
    w.borr.step(delta);                          // 0x00451e20
    if (w.terrain) w.terrain->step(delta);       // 0x004516e0
    // 0x00452230: skip records before the walker's sample.
    w.dzhr.position += delta;
    const auto& records = events_.at(w.dzhr.node).records;
    if (delta > 0) {
        const uint32_t sample = uint32_t(w.dzhr.position >> 8);
        // The index may end one past the last record (loop test after ++).
        while (w.dzhr.index < int32_t(records.size())) {
            if (sample <= (records[size_t(w.dzhr.index)][0] & 0x7fff0000u) >> 16) break;
            ++w.dzhr.index;
        }
    }
}

void TrackRing::build_slice(Walker& w, int32_t ring_q8, const ActorSink& sink) {
    // 0x00454240: two 0x00451cb0 walks, then 0x004520d0.
    if (w.stopped) return;
    const auto& node = node_of(course_, w.node);
    const auto& section = course_.sections.at(node.link);
    const uint32_t local = uint32_t(w.position >> 8);
    const auto& path = section.samples.at(local);
    w.borr.slots(path, *random_);
    w.borr.slots(path, *random_);
    auto& records = events_.at(w.dzhr.node).records;
    for (size_t i = size_t(std::max(0, w.dzhr.index)); i < records.size(); ++i) {
        auto& r = records[i];
        if ((r[0] & 0x7fff0000u) >> 16 != uint32_t(w.dzhr.position >> 8)) break;
        if (((r[0] >> 8) & 0x3f) == 0x3f) continue;
        if (!(r[0] & 0x80000000u)) continue;   // billboard slot: no draw
        const uint32_t jitter = (*random_)();
        const RingActor actor{ring_q8 >> 8, local * 0x100u + (r[1] >> 24), r[0], r[1], node.link, local};
        if (sink) sink(actor, jitter);
        r[0] |= 0x3f00u;
    }
}

void TrackRing::setup(int32_t camera, const Random& random, const ActorSink& sink) {
    random_ = &random;
    std::vector<uint32_t> visited;
    jitter_events(course_.root, visited, random);
    enter(front_walker_, course_.root);    // 0x00452f50 front, then back
    enter(back_walker_, course_.root);
    front_ = camera + 0x5000;
    back_ = camera - 0x3000;
    auto walk = [&](Walker& w, int32_t to) {   // 0x00453ae0 forward
        int32_t d = to - w.position;
        if (d <= 0) { if (d < 0) w.stopped = unsupported_ = true; return; }
        for (int32_t n = d >> 8; n > 0; --n) step(w, 0x100);
        step(w, d & 0xff);
    };
    walk(front_walker_, back_);
    walk(back_walker_, back_);
    for (int32_t k = 0; k < 0x8000; k += 0x100) {
        build_slice(front_walker_, back_ + k, sink);
        step(front_walker_, 0x100);
    }
    random_ = nullptr;
}

void TrackRing::advance(int32_t delta, const Random& random, const ActorSink& sink) {
    if (delta == 0) return;
    random_ = &random;
    const int32_t old_back = back_;
    back_ += delta;
    front_ += delta;
    if (((old_back ^ back_) & ~0xff) != 0) {
        if (delta < 1) {
            back_walker_.stopped = front_walker_.stopped = unsupported_ = true;
        } else {
            step(back_walker_, 0x100);
            build_slice(front_walker_, front_ - delta, sink);
            step(front_walker_, 0x100);
        }
    }
    random_ = nullptr;
}

TrackRingState TrackRing::diagnostic_state() const {
    auto state=[](const Walker& walker) {
        TrackWalkerState out;
        out.position=walker.position;
        if(walker.terrain)out.terrain=walker.terrain->diagnostic_state();
        return out;
    };
    return {state(front_walker_),state(back_walker_)};
}

TerrainSlice TrackRing::front_terrain(int32_t left, int32_t right) const {
    return front_walker_.terrain ? front_walker_.terrain->slice(left, right) : TerrainSlice{};
}
}
