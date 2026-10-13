#include "view_state.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << "\n"; } };
    rr::ViewState a, b, out;
    a.camera_position_q8 = 1000; b.camera_position_q8 = 1256;
    a.actors = {{1, 5000, 0, 0, 0, 0, 0, true}}; b.actors = {{1, 5100, 256, 0, 0, 0, 0, true}};
    rr::blend_view(out, a, b, 0);       check(out.camera_position_q8 == 1000 && out.actors[0].position_q8 == 5000, "alpha 0 is a");
    rr::blend_view(out, a, b, 65536);   check(out.camera_position_q8 == 1256 && out.actors[0].position_q8 == 5100, "alpha 1 is b");
    rr::blend_view(out, a, b, 32768);   check(out.camera_position_q8 == 1128 && out.actors[0].lateral_q8 == 128, "halfway");
    a.camera_position_q8 = 0xffffff80u; b.camera_position_q8 = 0x80;
    rr::blend_view(out, a, b, 32768);   check(out.camera_position_q8 == 0, "wraps forward through zero");
    b.actors[0].route = 1;              // split commit
    rr::blend_view(out, a, b, 32768);   check(out.actors[0].position_q8 == 5100, "route change is not blended");
    b.actors[0].route = 0; b.actors[0].position_q8 = 5000 + (64 << 8);   // crash reset / remount jump
    rr::blend_view(out, a, b, 32768);   check(out.actors[0].position_q8 == b.actors[0].position_q8, "teleport is not blended");
    b.actors.push_back({2, 9000, 0, 0, 0, 0, 0, true});                  // new car
    rr::blend_view(out, a, b, 32768);   check(out.actors.size() == 2 && out.actors[1].position_q8 == 9000, "new actor drawn at b");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
