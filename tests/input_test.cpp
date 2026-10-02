#include "input.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    rr::PlayerInput none{};
    check(rr::controls_from(none) == 0, "no input, no bits");
    rr::PlayerInput go{}; go.accelerate = true; go.left = true;
    check(rr::controls_from(go) == (rr::control_accelerate | rr::control_left), "accelerate + left");
    rr::PlayerInput stop{}; stop.brake = true; stop.right = true;
    check(rr::controls_from(stop) == (rr::control_brake | rr::control_right), "brake + right");
    // Analog stick past the threshold steers; inside the dead zone it does not.
    rr::PlayerInput stick{}; stick.steer_axis = -20000;
    check(rr::controls_from(stick) == rr::control_left, "stick left");
    stick.steer_axis = 5000;
    check(rr::controls_from(stick) == 0, "stick dead zone");
    stick.steer_axis = 20000;
    check(rr::controls_from(stick) == rr::control_right, "stick right");
    // Touch zones on a 1000-wide screen: left third steers left, right third right, bottom-middle brakes;
    // any touch accelerates unless braking.
    check(rr::touch_input(100, 500, 1000, 800).left && rr::touch_input(100, 500, 1000, 800).accelerate, "touch left");
    check(rr::touch_input(900, 500, 1000, 800).right, "touch right");
    auto b = rr::touch_input(500, 750, 1000, 800);
    check(b.brake && !b.accelerate, "touch brake");
    rr::PlayerInput fight{}; fight.punch = true; fight.kick = true; fight.backhand = true;
    check(rr::controls_from(fight) == (rr::control_punch | rr::control_kick | rr::control_backhand), "attack bits");
    check(rr::touch_input(500, 100, 1000, 800).punch, "touch top third punches");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
