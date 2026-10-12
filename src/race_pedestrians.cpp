#include "race.hpp"
#include "pedestrians.hpp"
#include "traffic.hpp"
#include <algorithm>

// Pedestrians, plan 24 Task 4. Each function names the routine it ports.
// Known native differences (labelled where they occur):
// - 0x004415b0 (traffic-light phase) returns -1: signals are not ported
//   (plan 24 Task 1), so pedestrians never wait on a light;
// - category-2 resources load at spawn, not at the first draw (0x00440680
//   calls 0x00440b80 lazily), so +0x1e0 is set one or more ticks early;
// - the ground height +0x24 settles on the first move instead of
//   0x00439b10 at allocation;
// - 0x0043a610 walls are skipped: the native walls() is a rider stand-in
//   that would clamp a pedestrian standing past the shoulder;
// - only the camera roadway's DZHR records spawn; the other roadway of a
//   split (second 0x00454240 call) is not walked yet;
// - scheduler order: pedestrians think after every rider, not interleaved
//   by ring position, and the contact pass runs after rider/car contacts.
namespace rr {
namespace {
int32_t sar8(int32_t x) { return (x + ((x >> 31) & 0xff)) >> 8; }
int32_t sar4(int32_t x) { return (x + ((x >> 31) & 0xf)) >> 4; }
int32_t div256(int64_t x) { return int32_t((x + ((x >> 63) & 0xff)) >> 8); }
int32_t magnitude(int32_t v) { const uint32_t s = uint32_t(v >> 31); return int32_t((uint32_t(v) ^ s) - s); }
constexpr int32_t crossing_heading[4] = {0, 0x400000, -0x800000, -0x400000};   // 0x0046df30 by +0x1ec
constexpr int32_t car_half_width = 16384, car_half_length = 64, car_half_height = 8960;   // 0x00475698 via 0x004387a0
constexpr int32_t car_mass = 1800;
}

void Race::build_pedestrian_slices() {
    // 0x004552e0 fills the ring back to front (128 slices); 0x00454120 then
    // builds the old front slice when moving forward and the new back slice
    // when moving backward. Every build walks the slice's DZHR records
    // (0x004520d0), firing each record once.
    if (!pedestrians_) return;
    auto& system = *pedestrians_;
    const auto& road = route_of(riders_[0]);
    const int32_t camera = int32_t(camera_position());
    const int32_t front = (camera + 0x5100) >> 8, back = (camera - 0x2f00) >> 8;
    auto build = [&](int32_t s) {
        if (s < 0 || size_t(s) >= road.size()) return;
        const auto& sample = road[size_t(s)];
        const auto& records = system.actors(sample);
        for (size_t i = 0; i < records.size(); ++i)
            if (system.consume(sample.section, sample.local, i)) spawn_pedestrian(sample, size_t(s), records[i]);
    };
    if (!system.filled) { system.filled = true; system.built_back = system.built_front = front - 128; }
    while (system.built_front < front) build(system.built_front++);
    while (system.built_back > back) build(--system.built_back);
    system.built_front = std::min(system.built_front, front);
    system.built_back = std::max(system.built_back, back);
}

void Race::spawn_pedestrian(const RouteSample& sample, size_t s, const DzhrActor& record) {
    // 0x004520d0: lateral (slice +0xcc + signed 12 bits) * 256, jittered by
    // the record's spread; the rand() comes first for every actor record.
    auto& system = *pedestrians_;
    const uint32_t w0 = record.word0, w1 = record.word1;
    const int32_t offset12 = (w1 & 0x800) ? int32_t(w1 | 0xfffff000u) : int32_t(w1 & 0xfff);
    const uint32_t spread = (w1 & 0xff0000u) >> 14;
    const int type = int(w0 & 15);
    const uint32_t jitter = random();
    const int32_t lateral = int32_t(uint32_t((sample.offset + offset12) * 256) - spread * 2 + jitter % (spread * 4 + 1));
    if (type >= 0xb) {
        // 0x004409e0 else-branch: two rand() draws, then the police bike
        // 0x00406b80 near the player. Cops are not live yet (plan 24 Task 2);
        // the draws keep the stream aligned.
        random(); random();
        return;
    }
    const int32_t player = riders_[0].bike.i32(0x1c);
    Pedestrian* slot = nullptr;
    for (auto& p : system.pool()) if (!p.active) { slot = &p; break; }
    if (!slot) return;   // pool full: 0x004387a0 returns null, the record is still consumed
    Pedestrian& p = *slot;
    p = Pedestrian{};
    p.active = true;
    Bike& b = p.body.bike;
    system.apply_template(b);
    b.u8(0xc0) = uint8_t(b.u8(0xc0) | 0x10);   // native guard: cross_sample's finish latch never applies to pedestrians
    const int32_t position = int32_t(uint32_t(s) * 256 + (w1 >> 24));
    b.i32(0x1c) = position; b.i32(0x20) = 0; b.i32(0x24) = 0; b.i32(0x28) = lateral;
    for (int k = 0; k < 5; ++k) b.i32(0x30 + 4 * k) = b.i32(0x1c + 4 * k);
    b.i32(0x134) = position; b.i32(0x138) = position;
    p.body.route = riders_[0].route;
    p.body.sample = p.body.previous_sample = s;
    p.anchor = position;                            // +0x1c8..+0x1d8 copy of +0x1c..+0x2c
    p.selector = uint8_t(w0 >> 8);
    p.type = type;
    p.side = lateral < 1 ? 1 : 3;
    p.trigger = player + int32_t(random() % 0x4fb1);
    p.serial = system.next_serial();
    p.last_tick = ticks_ - b.i32(0x14);            // 0x00438930: +0x10 = tick - interval, +0xc = tick
    p.next_tick = ticks_;
    p.look = system.look(sample, p.selector);
    if (p.look) {   // 0x00440b80
        set_animation(p.animation, p.look->bank, 1, uint32_t(ticks_));
        queue_animation(p.animation, p.look->bank, -1);
        advance_animation(p.animation, p.look->bank, uint32_t(ticks_));
    }
    ++system.spawned_;
}

void Race::step_pedestrians() {
    // 0x00437e60 for type 0x20: dt = tick - last run; the next interval is
    // 1 within 4 samples of the ring camera (1 - d when more than 2
    // behind), else (d + 1) / 2. Parked outside the ring window or beyond
    // lateral +-0xc8000.
    if (!pedestrians_) return;
    const int32_t ring = int32_t(camera_position()) + 0x100;
    const int32_t front = ring + 0x5000, back = ring - 0x3000;
    for (auto& p : pedestrians_->pool()) {
        if (!p.active || p.next_tick > ticks_) continue;
        Bike& b = p.body.bike;
        b.i32(0x18) = ticks_ - p.last_tick;
        p.last_tick = ticks_;
        think_pedestrian(p);
        move_pedestrian(p);
        const int32_t d = sar8(b.i32(0x1c) - ring);
        const int32_t interval = d < 4 ? (d < -2 ? 1 - d : 1) : (d + 1) / 2;
        b.i32(0x14) = interval;
        if (b.i32(0x28) < -0xc8000 || b.i32(0x28) > 0xc8000) { p.active = false; continue; }
        p.next_tick = ticks_ + interval;
        if (front - b.i32(0x1c) < 0 || b.i32(0x1c) - back < 0) p.active = false;
    }
}

int Race::pedestrian_lane(const Pedestrian& p) const {
    // 0x0044a020(obj, 0): lanes of 250 world units from the slice centre.
    const auto& s = route_of(p.body)[p.body.sample];
    const int32_t d = int32_t(uint32_t(p.body.bike.i32(0x28)) - uint32_t(s.offset * 256));
    return int8_t(int32_t(int64_t(sar8(d)) / 250) - (d < 0 ? 1 : 0));
}

bool Race::pedestrian_road_clear(const Pedestrian& p, int factor, int delta) const {
    // 0x00441500 -> 0x0044a070(obj, lane, lane < 0 ? 0 : 2, 0x14, -1): the
    // nearest bike or car in that lane, ahead for the oncoming (negative)
    // lanes and behind otherwise. Clear when none, or when the gap beats
    // its speed scaled by the crossing factor and cruise DAT_004b7e8c.
    const int8_t lane = int8_t(pedestrian_lane(p) + delta);
    const bool ahead = lane < 0;
    const int32_t pos = p.body.bike.i32(0x1c);
    bool found = false;
    int32_t best_gap = 0, other_pos = 0, other_velocity = 0;
    auto consider = [&](int32_t opos, int32_t olateral, int32_t ooffset, int32_t velocity) {
        if (ahead ? opos <= pos : opos >= pos) return;
        const int32_t d = int32_t(uint32_t(olateral) - uint32_t(ooffset * 256));
        if (int8_t(int32_t(int64_t(sar8(d)) / 250) - (d < 0 ? 1 : 0)) != lane) return;
        const int32_t gap = std::abs(opos - pos);
        if (found && gap >= best_gap) return;
        found = true; best_gap = gap; other_pos = opos; other_velocity = velocity;
    };
    for (const auto& r : riders_) {
        const auto& sample = route_of(r)[r.sample];
        if (sample.dual && r.route != p.body.route) continue;   // 0x00449ce0 roadway-half test
        consider(r.bike.i32(0x1c), r.bike.i32(0x28), sample.offset, r.bike.i32(0x108));
    }
    if (traffic_) {
        const auto& road = route_of(riders_[0]);
        for (const auto& car : traffic_->cars()) {
            if (!car.active || car.position < 0) continue;
            const size_t s = std::min(size_t(car.position) >> 8, road.size() - 1);
            consider(car.position, car.lateral, road[s].offset, car.kind == 2 ? -car.speed : car.speed);
        }
    }
    if (!found) return true;
    const uint32_t v = uint32_t(magnitude(other_velocity));
    const int32_t sum = int32_t(uint32_t(int32_t(v & 0xfffff00fu) >> 4) + uint32_t(int32_t(v & 0xfffffe01u) >> 1) + (v & 0xffffff80u) * 2u);
    const int32_t cruise = traffic_ ? traffic_->timers().cruise : 25;
    const int32_t q = sum / (cruise + 1);
    const uint32_t f = uint32_t(factor * 0x191) >> 8;
    const int32_t lhs = int32_t((uint32_t(q) * f) & 0xffffff0fu) >> 4;
    const int32_t rhs = int32_t(uint32_t(magnitude(int32_t(uint32_t(pos) - uint32_t(other_pos)))) & 0xfffffff0u);
    return lhs <= rhs;
}

bool Race::pedestrian_arrived(const Pedestrian& p) const {
    // 0x004414a0: past the middle of the far shoulder (+0xe4/+0xec right,
    // +0xe0/+0xe8 left).
    const SliceEdges e = edges(p.body, p.body.sample);
    const int32_t lateral = p.body.bike.i32(0x28);
    if (p.side == 1) return ((e.right + e.right_shoulder) / 2) * 256 < lateral;
    return lateral < ((e.left + e.left_shoulder) / 2) * 256;
}

void Race::pedestrian_voice(Pedestrian& p, int which) {
    // 0x00436830(0x16, {voice}): the 'MROF' word byte-swapped by 0x00440730;
    // 0x004351f0 plays it as the speech event.
    p.voice_tick = ticks_;
    if (sound_events_.size() < 10)
        sound_events_.push_back({SoundEvent::speech, 0, 0, 0, 0, p.look->voices[size_t(which)]});
}

void Race::think_pedestrian(Pedestrian& p) {
    // 0x00440dd0.
    Bike& b = p.body.bike;
    const int32_t player = riders_[0].bike.i32(0x1c);
    const uint32_t tick = uint32_t(ticks_);
    auto set = [&](int8_t code) { if (p.look) set_animation(p.animation, p.look->bank, code, tick); };
    auto queue = [&](int8_t code) { if (p.look) queue_animation(p.animation, p.look->bank, code); };
    auto rest = [&] { p.state = 0; set(1); queue(-1); p.pace = 0; };          // LAB_004413ea
    auto resume = [&] { p.state = 1; p.pace = 4; set(0); queue(-1); };
    const bool signal = false;   // 0x004415b0 -> -1 (no signal objects; plan 24 Task 1)
    p.pace = 0;
    if (p.state != 2) {
        p.heading = crossing_heading[p.side & 3];
        int base = -1;
        switch (p.type) {
        case 4: p.heading = int32_t(0xff800800u); break;
        case 5: base = 0xfe; break;   // 0x7f + 0x7f falling through to case 7
        case 6: base = 0x7f; break;
        case 7: base = 0; break;
        case 10:
            if (p.loaded() && p.state != 7) { p.state = 7; set(6); }
            break;
        default: break;
        }
        if (base >= 0) {
            const int factor = uint8_t(base + 1);
            switch (p.state) {
            case 0:
                p.lane = int8_t(pedestrian_lane(p));
                if ((p.has(0) || p.has(5) || !p.loaded()) && p.trigger < player && (random() & 1) != 0) {
                    if (!signal) {
                        if (!pedestrian_road_clear(p, factor, 0)) break;
                        if (!pedestrian_road_clear(p, factor, p.side == 1 ? 1 : -1)) break;
                    }
                    p.state = 1;
                    p.pace = p.has(0) ? 4 : 8;
                    if (!p.loaded()) set(0); else set(p.has(0) ? 0 : 5);
                }
                break;
            case 1:
                if (p.has(0)) { queue(0); p.pace = 4; }
                else if (p.has(5)) { queue(5); p.pace = 8; }
                else if (!p.loaded()) p.pace = 4;
                else { queue(1); p.pace = 0; }
                if (signal || int8_t(pedestrian_lane(p)) == p.lane) {
                    if ((random() & 3) == 0 || pedestrian_road_clear(p, (factor >> 2) + 0x10, 0)) {
                        if (pedestrian_arrived(p)) {
                            p.state = 0; queue(1); p.pace = 0;
                            p.trigger = player + 0xa0;
                            p.side = (p.side + 2) & 3;
                        }
                    } else {
                        // Traffic too close mid-crossing: hesitate.
                        p.state = 3;
                        if (p.has(2)) set(2);
                        if (!p.has(4) || (p.has(5) && (random() & 7) == 0)) { if (p.has(5)) queue(5); }
                        else {
                            if (uint32_t(ticks_ - p.voice_tick) > 0xb4 && p.look && p.look->voices[1] >= 0) pedestrian_voice(p, 1);
                            queue(4);
                        }
                    }
                } else {
                    p.lane = int8_t(pedestrian_lane(p));
                    if (!pedestrian_road_clear(p, factor, 0)) rest();
                }
                break;
            case 3: {
                const int8_t current = p.animation.animation;
                if (current == 2) break;
                if (current == 4 || current == 5) {
                    p.state = current;
                    queue(p.has(0) ? 0 : p.has(5) ? 5 : 1);
                } else if (!p.has(0)) { if (p.has(5)) queue(5); else rest(); }
                else resume();
                break;
            }
            case 4:
                if (p.animation.animation != 4) {
                    if (!p.has(0)) { if (p.has(5)) queue(5); else rest(); }
                    else resume();
                }
                break;
            case 5:
                p.pace = 8;
                if (pedestrian_road_clear(p, factor, 0)) {
                    if (!p.has(0)) { if (p.has(5)) queue(5); else rest(); }
                    else resume();
                }
                break;
            default: break;
            }
        }
        // 0x004498c0(obj, heading, pace*100, pace*100).
        const int32_t heading = p.heading;
        b.i32(0x128) = heading;
        int32_t speed = b.i32(0x18) * (p.pace * 100) + b.i32(0xec);
        if (p.pace * 100 < speed) speed = p.pace * 100;
        b.i32(0xec) = speed;
        b.i32(0x5c) = heading;
    }
    if (p.look) advance_animation(p.animation, p.look->bank, tick);
}

void Race::move_pedestrian(Pedestrian& p) {
    // 0x00440990: 0x0043adc0 (independent actor motion), then stay within
    // 0x1ff of the spawn position.
    Bike& b = p.body.bike;
    if (!b.i32(0x160)) { b.i32(0x114) = b.i32(0x118) = 0; b.u8(0x164) = 0; }
    integrate_speed(b, tables_, surface_class(edges(p.body, p.body.sample), b.i32(0x28)));
    advance_actor_motion(b, tables_, [&](int32_t distance) { move_along(p.body, distance); });
    vertical(p.body);
    const int32_t gap = int32_t(uint32_t(p.anchor) - uint32_t(b.i32(0x1c)));
    if (magnitude(gap) > 0x1ff) {
        move_along(p.body, gap >= 0 ? gap - 0x1ff : gap + 0x1ff, true);
        b.i32(0xec) = 0;
    }
}

void Race::hit_pedestrian(Pedestrian& p, int32_t hitter_speed, bool hitter_attacking, bool hitter_walking) {
    // 0x00440760, the pedestrian's +0x1a4 handler.
    if (!p.loaded()) return;
    Bike& b = p.body.bike;
    const uint32_t tick = uint32_t(ticks_);
    if (p.state != 2) {
        if (hitter_attacking || (magnitude(hitter_speed) > 1000 && !hitter_walking)) {
            // Knocked down: scream when 0..0x400 ahead of the view camera.
            const int32_t ahead = b.i32(0x1c) - int32_t(camera_position());
            if (ahead >= 0 && ahead < 0x401 && uint32_t(ticks_ - p.voice_tick) > 0xb4 && p.look->voices[0] >= 0) pedestrian_voice(p, 0);
            p.state = 2;
            set_animation(p.animation, p.look->bank, 3, tick);
            queue_animation(p.animation, p.look->bank, -1);
            b.i32(0x154) = 0x19;
            b.i32(0x128) = 0; b.i32(0x5c) = 0;   // 0x004498c0(obj, 0, 20000, 0)
            if (20000 < b.i32(0xec)) b.i32(0xec) = 20000;
        } else {
            // Bumped: stagger.
            p.state = 3;
            if (p.has(2)) set_animation(p.animation, p.look->bank, 2, tick);
            if (!p.has(4) || (p.has(5) && (random() & 7) == 0)) queue_animation(p.animation, p.look->bank, p.has(5) ? 5 : 1);
            else {
                queue_animation(p.animation, p.look->bank, 4);
                if (uint32_t(ticks_ - p.voice_tick) > 0xb4 && p.look->voices[1] >= 0) pedestrian_voice(p, 1);
            }
        }
    }
    if (p.state != 2) {
        const int32_t pushed = sar4(hitter_speed);
        b.i32(0xec) = pushed < 0 ? 0 : pushed;
        b.i32(0x128) = hitter_speed > 0 ? 0 : 0x7f;
    }
}

void Race::contact_pedestrian(Rider& self, Pedestrian& p) {
    // 0x004049c0 for a short object (+0x17c 0x1e00 < 0x2000): no knock-off
    // decision. Upright pedestrians (state != 2) only nudge the rider below
    // speed 2000 (500 for one already down); faster riders ride up over them.
    Bike& s = self.bike;
    const Bike& q = p.body.bike;
    const int32_t steer = s.i32(0xf0);
    if (&self == &riders_[0]) {
        const int32_t hit = magnitude(s.i32(0x184));
        if (hit < 0x1771) {
            if (hit >= 0x7d1 || magnitude(s.i32(0x188)) >= 0x3e9) {
                sound(SoundEvent((random() & 1) + 3), s);
                sound(SoundEvent(random() & 1), s);
            }
        } else {
            sound(SoundEvent((random() & 1) + 4), s);
            sound(SoundEvent((random() & 1) + 1), s);
        }
    }
    const int32_t elasticity = s.i32(0x180);
    s.i32(0x188) = div256(int64_t(s.i32(0x188)) * elasticity);
    s.i32(0x18c) = div256(int64_t(s.i32(0x18c)) * elasticity);
    s.i32(0x184) = div256(int64_t(s.i32(0x184)) * elasticity);
    const int32_t speed = s.i32(0xec);
    bool nudge;
    if (speed < 300 && s.u8(0x19c) == 0x20) { move_along(self, s.i32(0x18) * 2, false); nudge = true; }
    else if (p.state != 2) nudge = speed < 2000;
    else nudge = speed < 500;
    if (nudge) {
        const int32_t step = sar4(s.i32(0x174) + q.i32(0x174)) * s.i32(0x18);
        s.i32(0x28) += q.i32(0x28) < s.i32(0x28) ? step : -step;
    } else {
        s.i32(0x20) += q.i32(0x17c) / 2;
        int32_t t = int32_t(uint32_t(q.i32(0x17c)) * uint32_t(magnitude(s.i32(0x184))) * 2u);
        t += (t >> 31) & 0x3fff;
        s.i32(0xf4) += (((t >> 14) + ((t >> 31) & 0xf)) >> 4) * 4;
    }
    (void)steer;   // +0xf0 is unchanged on this branch, so the steer-rate tail is skipped
    if (s.i32(0xf4) > 0x600) s.i32(0xf4) = 0x600;
    s.i32(0x184) = s.i32(0x188) = s.i32(0x18c) = 0;
    s.u8(0x19c) = 0;
    if (s.i32(0xec) > 30000) s.i32(0xec) = 30000;
    if (s.i32(0xec) < -1000) s.i32(0xec) = -1000;
    s.i32(0xf4) = std::clamp(s.i32(0xf4), -10000, 10000);
}

void Race::collide_pedestrians() {
    // 0x004368f0 pairs with a pedestrian: bounds +0x178/+0x174/+0x17c,
    // separation 0x00436a40 by mass (bike +0x194 against pedestrian 100),
    // then the rider's handler and the pedestrian's 0x00440760.
    if (!pedestrians_) return;
    for (auto& p : pedestrians_->pool()) {
        if (!p.active) continue;
        Bike& q = p.body.bike;
        for (auto& r : riders_) {
            if (r.finished || r.bike.i32(0x2f4) != 0) continue;
            if (route_of(r)[r.sample].dual && r.route != p.body.route) continue;
            Bike& s = r.bike;
            const int32_t length = s.i32(0x178) + q.i32(0x178), width = s.i32(0x174) + q.i32(0x174);
            if (std::abs(s.i32(0x1c) - q.i32(0x1c)) > length) continue;
            if (std::abs(s.i32(0x28) - q.i32(0x28)) > width) continue;
            if (std::abs(s.i32(0x20) - q.i32(0x20)) > s.i32(0x17c) + q.i32(0x17c)) continue;
            const int32_t ms = s.i32(0x194), mq = q.i32(0x194), total = ms + mq;
            const bool forward = length < std::abs(s.i32(0x30) - q.i32(0x30));
            const bool lateral = width < std::abs(s.i32(0x3c) - q.i32(0x3c));
            if (forward) {
                const int32_t depth = (length - std::abs(s.i32(0x1c) - q.i32(0x1c))) + 8;
                const int32_t push = depth * mq / total + 3, back = depth * ms / total + 3;
                const bool rider_ahead = q.i32(0x1c) < s.i32(0x30);
                move_along(r, rider_ahead ? push : -push, false);
                move_along(p.body, rider_ahead ? -back : back, false);
                s.i32(0x184) = ((q.i32(0x108) - s.i32(0x108)) * mq * 2) / total;
            } else s.i32(0x184) = 0;
            if (lateral) {
                const int32_t depth = (width - std::abs(s.i32(0x28) - q.i32(0x28))) + 0x800;
                const int32_t push = depth * mq / total + 3, back = depth * ms / total + 3;
                if (q.i32(0x28) < s.i32(0x3c)) { s.i32(0x28) += push; q.i32(0x28) -= back; }
                else { s.i32(0x28) -= push; q.i32(0x28) += back; }
                s.i32(0x188) = ((q.i32(0x10c) - s.i32(0x10c)) * mq * 2) / total;
            } else s.i32(0x188) = 0;
            s.i32(0x18c) = 0;
            s.u8(0x19c) = uint8_t((forward ? (q.i32(0x1c) < s.i32(0x30) ? 1 : 2) : 0) |
                                  (lateral ? (q.i32(0x28) < s.i32(0x3c) ? 4 : 8) : 0));
            for (int k = 0; k < 5; ++k) s.i32(0x30 + 4 * k) = s.i32(0x1c + 4 * k);
            const int32_t hitter_speed = s.i32(0xec);
            contact_pedestrian(r, p);
            hit_pedestrian(p, hitter_speed, ai_attack_anim(r) != 0, false);
        }
        if (!traffic_) continue;
        for (auto& car : traffic_->cars_mut()) {
            if (!car.active) continue;
            if (std::abs(car.position - q.i32(0x1c)) > car_half_length + q.i32(0x178)) continue;
            if (std::abs(car.lateral - q.i32(0x28)) > car_half_width + q.i32(0x174)) continue;
            if (std::abs(q.i32(0x20)) > car_half_height + q.i32(0x17c)) continue;
            // Lateral separation only (the car keeps no previous-position copy).
            const int32_t depth = (car_half_width + q.i32(0x174) - std::abs(car.lateral - q.i32(0x28))) + 0x800;
            const int32_t back = depth * car_mass / (car_mass + q.i32(0x194)) + 3;
            q.i32(0x28) += car.lateral < q.i32(0x28) ? back : -back;
            hit_pedestrian(p, car.kind == 2 ? -car.speed : car.speed, false, false);
        }
    }
}
}
