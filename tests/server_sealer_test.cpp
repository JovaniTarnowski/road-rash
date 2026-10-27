// Plan 34 Task 5 Step 2: sealer behaviour — inputs on time, late inputs
// sealed as repeat-last with the late flag, a vanished player handed to the
// AI at a sealed tick and back, bounded buffering, and the Frames view.
#include "check.hpp"
#include "sealer.hpp"

using rr::netd::Sealer;

int main() {
    rr::test::Checks check;
    {
        Sealer s(2);
        check.eq(s.sealed_tick(), -1, "nothing sealed yet");
        const uint16_t a[] = {1, 2, 3};
        const uint16_t b[] = {10, 20};
        s.input(0, 0, a);
        s.input(1, 0, b);
        check.eq(s.seal_through(2), 3, "three ticks sealed");
        const auto& log = s.log();
        check(log[0].words == std::vector<uint16_t>{1, 10} && log[0].present == 3 && log[0].late == 0,
              "tick 0 on time");
        check(log[1].words == std::vector<uint16_t>{2, 20} && log[1].late == 0, "tick 1 on time");
        check(log[2].words == std::vector<uint16_t>{3, 20} && log[2].late == 2,
              "tick 2: seat 1 late, repeat-last");
        check.eq(s.counters().late, uint64_t(1), "late counted");
        // Late input for a sealed tick is dropped; the sealed frame never changes.
        const uint16_t late[] = {99};
        s.input(1, 2, late);
        check(s.log()[2].words[1] == 20, "sealed tick cannot be rewritten");
        check.eq(s.counters().dropped_old, uint64_t(1), "old input counted");
        // First word for a future tick wins.
        const uint16_t first[] = {7}, second[] = {8};
        s.input(0, 3, first);
        s.input(0, 3, second);
        s.seal_through(3);
        check(s.log()[3].words[0] == 7, "first word for a tick wins");
        // Too far ahead is dropped (bounded buffering).
        const uint16_t far[] = {5};
        s.input(0, s.sealed_tick() + Sealer::max_ahead + 1, far);
        check.eq(s.counters().dropped_ahead, uint64_t(1), "input beyond the window dropped");
        // Out-of-range seats are ignored.
        s.input(2, 4, far);
        s.input(-1, 4, far);
        s.seal_through(4);
        check(s.log()[4].late == 3, "no input for tick 4 from either seat");
    }
    {
        // Vanish at tick 5: AI-driven from the next sealed tick; back later.
        Sealer s(3);
        for (int t = 0; t < 10; ++t) {
            const uint16_t w[] = {uint16_t(t)};
            for (int seat = 0; seat < 3; ++seat)
                if (!(seat == 1 && t >= 5)) s.input(seat, t, w);
        }
        s.seal_through(4);
        s.set_human(1, false);
        s.seal_through(9);
        bool ok = true;
        for (int t = 0; t < 5; ++t) ok &= (s.log()[size_t(t)].present & 2) != 0;
        for (int t = 5; t < 10; ++t)
            ok &= (s.log()[size_t(t)].present & 2) == 0 && (s.log()[size_t(t)].late & 2) == 0;
        check(ok, "seat 1 AI-driven exactly from tick 5");
        check(s.log()[7].words[1] == 4, "AI seat keeps the last human word");
        s.set_human(1, true);
        const uint16_t back[] = {42};
        s.input(1, 10, back);
        s.seal_through(10);
        check((s.log()[10].present & 2) && s.log()[10].words[1] == 42, "seat 1 human again at tick 10");
        // Determinism: the same calls give the same log.
        Sealer t(3);
        for (int k = 0; k < 10; ++k) {
            const uint16_t w[] = {uint16_t(k)};
            for (int seat = 0; seat < 3; ++seat)
                if (!(seat == 1 && k >= 5)) t.input(seat, k, w);
        }
        t.seal_through(4);
        t.set_human(1, false);
        t.seal_through(9);
        t.set_human(1, true);
        t.input(1, 10, back);
        t.seal_through(10);
        check(t.log() == s.log(), "sealing is deterministic");
        const auto f = s.frames(8, 10);
        check(f.first_tick == 8 && f.frames.size() == 3 && f.humans == 3, "Frames view clipped to the log");
    }
    return check.finish("server_sealer");
}
