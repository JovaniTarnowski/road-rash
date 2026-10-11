#include "road.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool value, const char* what) { if (!value) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    check(rr::advance_heading(0,0x120,0x150,2,3)==3*48*256,"heading within sample");
    check(rr::advance_heading(0,0x1f0,0x220,2,3)==(2*16+3*32)*256,"heading across sample");
    check(rr::advance_heading(0,0x220,0x1f0,3,2)==(-3*32-2*16)*256,"heading reverses");
    check(rr::advance_heading(17,0,257,1,1)==17 && rr::advance_heading(17,0,128,1,1,true)==17,"heading ignores jump and frozen camera");
    check(rr::horizon_scroll(-1)==0 && rr::horizon_scroll(0x400000)==-40 && rr::horizon_scroll(-0x400000)==40,"horizon signed truncation");
    rr::DepthTable table;
    for (int i=0; i<4352; ++i) table.scale.push_back(i<128 ? 65536 : (1<<23)/i); // synthetic, not original data
    auto route = [](int bend, int rise, size_t n) {
        return std::vector<rr::RouteSample>(n, rr::RouteSample{{bend,rise,500,375},0,0,0});
    };
    const rr::Camera camera;
    auto flat = rr::project_road(route(0,0,100), 0, camera, table);
    check(flat.size()==rr::view_slices, "33 slices");
    bool converging = true;
    for (size_t n=0; n<flat.size(); ++n) {
        converging &= flat[n].road[0].road_x==0 && flat[n].depth==int32_t(256*(n+1)) && flat[n].road[0].left==-500 && flat[n].road[0].right==375;
        converging &= flat[n].road[0].screen_y==150*flat[n].scale+90*65536;
        if (n) converging &= flat[n].road[0].screen_y<=flat[n-1].road[0].screen_y;
    }
    check(converging, "flat road converges to horizon");
    // Bend is curvature: dx accumulates bend, x accumulates dx.
    auto curve = rr::project_road(route(3,0,100), 0, camera, table);
    bool triangular = true;
    for (int n=0; n<rr::view_slices; ++n) triangular &= curve[n].road[0].road_x==3*n*(n+1)/2;
    check(triangular, "curvature integrates twice");
    // Rise is a height delta: y accumulates rise once.
    auto hill = rr::project_road(route(0,4,100), 0, camera, table);
    bool linear = true;
    for (int n=0; n<rr::view_slices; ++n) linear &= hill[n].road[0].screen_y==(150-4*n)*hill[n].scale+90*65536;
    check(linear, "rise integrates once");
    // Sub-slice camera fraction (0x00443d70 initial state).
    auto half = rr::project_road(route(-5,-3,100), 0x180, camera, table);
    check(half[0].sample==1 && half[0].depth==128 && half[0].road[0].road_x==0, "fractional first slice");
    check(half[0].road[0].screen_y==(150-(-(128*-3>>8)))*65536+90*65536, "fractional height uses floor shift");
    check(half[1].road[0].road_x==-5*128/256, "fractional curvature truncates");
    check(rr::project_road(route(0,0,10), 5<<8, camera, table).size()==5, "route end truncates view");
    try { rr::project_road(route(0,0,10), 10<<8, camera, table); check(false, "reject camera past route"); }
    catch (const std::runtime_error&) {}
    // Split ahead of the camera: both halves start from the current road state.
    auto split = route(0,0,60);
    for (size_t i=10; i<60; ++i) {
        auto& s = split[i];
        s.dual = true; s.half = 0; s.other = {6,2,500,375,512,384}; s.other_offset = 700; s.offset = -40;
        if (i==10) s.event = rr::RoadEvent::split;
    }
    for (size_t i=0; i<10; ++i) split[i].offset = -40;
    auto ahead = rr::project_road(split, 0, camera, table);
    check(ahead[9].count==1 && ahead[10].count==2, "second road appears at split");
    check(ahead[10].road[0].road_x==0 && ahead[10].road[1].road_x==740, "halves start together, own offsets");
    check(ahead[11].road[1].road_x-ahead[11].road[0].road_x==740+6 &&
          ahead[12].road[1].road_x-ahead[12].road[0].road_x==740+18, "other half integrates its own bend");
    check(ahead[10].road[1].screen_y==150*ahead[10].scale+90*65536 &&
          ahead[11].road[1].screen_y==(150-2)*ahead[11].scale+90*65536 &&
          ahead[12].road[1].screen_y==(150-4)*ahead[12].scale+90*65536, "other half integrates its own rise");
    // Camera already within a dual window, riding half 1: half 0 placed from stored separation.
    auto within = split;
    for (auto& s : within) { s.half = 1; s.dx_sep = 8; s.x_sep = 300; }
    auto inside = rr::project_road(within, 20<<8, camera, table);
    check(inside[0].count==2 && inside[0].road[1].road_x==0 && inside[0].road[0].road_x==-300+740, "dual camera start");
    check(inside[1].road[0].road_x==-300+740-8 && inside[2].road[0].road_x==-300+740-8-2,
          "stored slope separation, then own bend");
    // Shoulders extend beyond the road edges by the RSLD widths.
    auto sides = route(0,0,40);
    for (auto& s : sides) { s.path.shoulder_left = 224; s.path.shoulder_right = 112; }
    auto shoulder = rr::project_road(sides, 0, camera, table);
    check(shoulder[3].road[0].shoulder_left==-500-224 && shoulder[3].road[0].shoulder_right==375+112, "shoulder edges");
    for (size_t n : {0, 64, 512, 4096}) {
        try { rr::read_depth_table(std::vector<uint8_t>(n)); check(false, "reject non-PE"); }
        catch (const std::runtime_error&) {}
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
