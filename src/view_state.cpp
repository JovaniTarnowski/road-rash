#include "view_state.hpp"
#include "traffic.hpp"

namespace rr {
namespace {
// Q16 blend of int32 fields; Q8 positions wrap-aware on uint32.
int32_t blend_i32(int32_t a, int32_t b, uint32_t alpha_q16) {
    return int32_t(int64_t(a) + ((int64_t(b) - int64_t(a)) * alpha_q16 >> 16));
}
// Shortest signed modular blend for wrapped quarter/factor (position-like uint32).
uint32_t blend_u32_wrap(uint32_t a, uint32_t b, uint32_t alpha_q16) {
    const uint32_t wrapped = b - a;
    const int64_t d = wrapped <= INT32_MAX ? int64_t(wrapped) : int64_t(wrapped) - (int64_t(1) << 32);
    return uint32_t(int64_t(a) + (d * alpha_q16 >> 16));
}
// Wrapped blend for heading (period 0x1000000, as in wrap_angle).
int32_t blend_heading(int32_t a, int32_t b, uint32_t alpha_q16) {
    constexpr int32_t period = 0x1000000;
    int64_t d = int64_t(b) - int64_t(a);
    d %= period;
    if (d > period / 2)
        d -= period;
    else if (d < -period / 2)
        d += period;
    int64_t r = (int64_t(a) + (d * alpha_q16 >> 16)) % period;
    if (r > period / 2)
        r -= period;
    else if (r < -period / 2)
        r += period;
    return int32_t(r);
}
} // namespace

void capture_view(ViewState& v, const Race& race, const TrafficSystem* traffic) {
    v.camera_position_q8 = race.camera_position();
    v.camera = race.camera();
    v.camera_route = 0;
    v.tick = uint32_t(race.ticks());
    v.actors.clear();
    const auto& riders = race.riders();
    for (size_t i = 0; i < riders.size(); ++i) {
        const auto& rd = riders[i];
        ActorPose p;
        p.id = uint32_t(i + 1);
        p.position_q8 = uint32_t(rd.bike.i32(0x1c));
        p.lateral_q8 = rd.bike.i32(0x28);
        p.height_q8 = rd.bike.i32(0x24);
        p.heading = rd.bike.i32(0x5c);
        p.roll = rd.bike.i32(0x60);
        p.route = rd.route;
        p.visible = true;
        v.actors.push_back(p);
        if (rd.crash_active) {
            ActorPose c;
            c.id = uint32_t(1000 + i);
            c.position_q8 = uint32_t(rd.crash_actor.motion.i32(0x1c));
            c.lateral_q8 = rd.crash_actor.motion.i32(0x28);
            c.height_q8 = rd.crash_actor.motion.i32(0x24);
            c.heading = rd.crash_actor.motion.i32(0x5c); // walk-back facing (0x0040a600)
            c.roll = rd.crash_actor.motion.i32(0x60);
            c.route = rd.crash_actor.route;
            c.visible = true;
            v.actors.push_back(c);
        }
    }
    if (traffic) {
        const auto& cars = traffic->cars();
        for (size_t i = 0; i < cars.size(); ++i) {
            if (!cars[i].active) continue;
            ActorPose p;
            p.id = uint32_t(2000 + i);
            p.position_q8 = uint32_t(cars[i].position);
            p.lateral_q8 = cars[i].lateral;
            p.route = 0;
            p.visible = true;
            v.actors.push_back(p);
        }
    }
}

void blend_view(ViewState& out, const ViewState& a, const ViewState& b, uint32_t alpha_q16,
                uint32_t teleport_q8) {
    out.tick = b.tick;
    out.camera_route = b.camera_route;
    out.camera = b.camera;
    if (a.camera_route != b.camera_route) {
        out.camera_position_q8 = b.camera_position_q8;
    } else if (uint32_t((b.camera_position_q8 - a.camera_position_q8) & 0xffffffffu) > teleport_q8 &&
               uint32_t((a.camera_position_q8 - b.camera_position_q8) & 0xffffffffu) > teleport_q8) {
        // Ambiguous large jump either way: teleport, draw at b.
        // (Forward wrap is small in (b-a); backward wrap small in (a-b). A
        // genuine teleport is large in both directions.)
        out.camera_position_q8 = b.camera_position_q8;
    } else {
        out.camera_position_q8 = blend_u32_wrap(a.camera_position_q8, b.camera_position_q8, alpha_q16);
    }
    out.actors.clear();
    out.actors.reserve(b.actors.size());
    for (const auto& pb : b.actors) {
        const ActorPose* pa = nullptr;
        for (const auto& q : a.actors)
            if (q.id == pb.id) {
                pa = &q;
                break;
            }
        if (!pa || !pa->visible || pa->route != pb.route) {
            out.actors.push_back(pb); // new, gone-before, or route-changed: draw at b
            continue;
        }
        const uint32_t fwd = pb.position_q8 - pa->position_q8;
        const uint32_t back = pa->position_q8 - pb.position_q8;
        if (fwd > teleport_q8 && back > teleport_q8) {
            out.actors.push_back(pb); // teleport: draw at b
            continue;
        }
        ActorPose m = pb;
        m.position_q8 = blend_u32_wrap(pa->position_q8, pb.position_q8, alpha_q16);
        m.lateral_q8 = blend_i32(pa->lateral_q8, pb.lateral_q8, alpha_q16);
        m.height_q8 = blend_i32(pa->height_q8, pb.height_q8, alpha_q16);
        m.heading = blend_heading(pa->heading, pb.heading, alpha_q16);
        m.roll = blend_i32(pa->roll, pb.roll, alpha_q16);
        out.actors.push_back(m);
    }
}
} // namespace rr
