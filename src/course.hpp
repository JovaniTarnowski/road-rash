#pragma once
#include "resource.hpp"
#include "terrain.hpp"
#include <array>
#include <map>

namespace rr {
// Per-sample path bytes and road half-widths in world units (250 per lane),
// truncated as in 0x00454240.
struct PathSample {
    int bend, rise;
    int32_t left, right, left_q8, right_q8;
    // RSLD sides (0x00452b00), world units: shoulder width beyond each road edge
    // (0x00454240) and shoulder height (0x00444630). flags: record bytes +16/+17.
    int32_t shoulder_left=0, shoulder_right=0, height_left=0, height_right=0;
    uint8_t side_flags[2]={0,0};
    uint8_t marking_flags=0,event_type=0;
    std::array<uint8_t,3> event_parameters{};
};
struct FamilyEvent {
    int32_t sample=0;
    uint8_t type=0,slot=0;
    uint16_t family=0;
};
struct CourseSection {
    std::vector<PathSample> samples;
    TerrainDefinition terrain;
    std::vector<FamilyEvent> family_events;
    int32_t family_forward_bias=0;
    std::vector<uint32_t> roadside_profiles; // BORR packed words, in original order
    std::vector<std::array<uint32_t,2>> scenery_events; // DZHR packed records, before random placement
};
struct CourseNode {
    uint32_t offset, kind, next=0, previous=0, link=0, length=0;
};
struct Course {
    uint32_t root;
    std::vector<CourseSection> sections;
    std::map<uint32_t,CourseNode> nodes;
};
// Split/merge event recorded in traversal byte +0xb (0x00453030).
enum class RoadEvent : uint8_t { none=0, merged=1, split=2, merge_window=6 };
struct RouteSample {
    PathSample path;
    uint32_t node, section, local;
    int32_t offset=0;       // lateral centre of this road ([0xd])
    // Second roadway alongside this sample (ring slice kind 2). Halves are
    // numbered by split/merge node fields +8 (0) and +12 (1).
    bool dual=false;
    uint8_t half=0;         // half this route follows
    RoadEvent event=RoadEvent::none;
    PathSample other{};
    uint32_t other_node=0,other_section=0,other_local=0;
    int32_t other_offset=0;
    int32_t dx_sep=0, x_sep=0; // half 1 minus half 0: traversal [5], [6]
};
Course read_course(const std::vector<uint8_t>& bytes);
// At each split choose its first or second outgoing edge. Merge follows link.
// Annotates split/merge windows as the original lookahead traversal would,
// assuming the rider has already committed to the chosen branch (0x00476cd4).
std::vector<RouteSample> make_route(const Course& course, bool second_branch);
}
