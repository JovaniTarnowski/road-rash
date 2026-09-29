#pragma once
#include "course.hpp"

namespace rr {
// Original 3D view space: 320x240, horizon row 90 (0x00444630).
constexpr int view_width=320, view_height=240, view_horizon=90;
// 0x00443a00 projects 0x21 slices ahead of the camera.
constexpr int view_slices=33;

// Q16 depth scale, indexed by depth/2. Static data at 0x0046f098 in ROADRASH.EXE.
struct DepthTable { std::vector<int32_t> scale; };
DepthTable read_depth_table(const std::vector<uint8_t>& exe);

// Camera position relative to the road surface of the current slice.
// Race start (0x0044b250) and the camera update (0x00437d80) use height >= 150.
struct Camera { int32_t x=0, height=150; };

// One projected roadway at a slice.
struct RoadSurface {
    int32_t road_x;    // road centre minus camera x, world units
    int32_t screen_y;  // Q16 row of the road surface
    int32_t left, right;
    // Outer shoulder edges: sub-slice [0x3a]/[0x3b] = road edges -/+ RSLD widths (0x00454240).
    int32_t shoulder_left, shoulder_right;
};

struct RoadSlice {
    uint32_t sample;   // route index
    int32_t depth;     // Q8 slices ahead of camera
    int32_t scale;     // Q16 screen pixels per world unit
    // Single road: road[0]. Split/merge: road[0] and road[1] are halves 0 and 1.
    uint8_t count;
    uint8_t own;       // roadway the route follows
    RoadSurface road[2];
    // Q16 screen column of a world offset from a roadway's centre.
    int64_t screen_x(int r, int32_t offset) const {
        return int64_t(road[r].road_x+offset)*scale+int64_t(view_width/2)*65536;
    }
};

// Integer port of 0x00443d70 + 0x00444630, including split/merge roadways.
// position is Q8 samples along route; nearest slice first. Camera x is
// relative to the centre of the route's own road at the camera sample.
std::vector<RoadSlice> project_road(const std::vector<RouteSample>& route, uint32_t position,
                                    const Camera& camera, const DepthTable& table);
}
