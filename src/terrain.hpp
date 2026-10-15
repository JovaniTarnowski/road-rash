#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
namespace rr {
struct TerrainWallRecord {
    uint32_t start=0;
    std::array<uint8_t,8> parameters{}; // record +4..+11
    int32_t base_height=0;
};
struct TerrainHillPoint {
    uint8_t min_x,max_x,x_variation;
    int8_t min_height,max_height;
    uint8_t height_variation,min_length,max_length;
};
struct TerrainHillRecord {
    int32_t start=0,end=0;
    std::array<uint8_t,2> selectors{};
    std::array<TerrainHillPoint,8> points{};
};
struct TerrainDefinition {
    enum class Kind { none,building,wall,hill } kind=Kind::none;
    std::vector<std::array<uint8_t,2>> selectors;
    std::array<std::vector<TerrainWallRecord>,2> walls;
    std::vector<TerrainHillRecord> hills;
};
TerrainDefinition read_terrain(const std::vector<uint8_t>& bytes,uint32_t sample_count);
struct TerrainSlice {
    uint8_t type=0xff;
    std::array<int32_t,2> building_height{};
    std::array<bool,2> building_cut_roof{};
    std::array<uint8_t,2> selectors{};
    std::array<std::array<int32_t,3>,2> walls{}; // width, outer offset, height
    std::array<std::array<int32_t,2>,6> points{}; // left/right alternating: x,height
    std::array<int32_t,6> shades{};
};
// Read-only iterator snapshot for original-state fixture comparisons. These
// include the two interpolation points not emitted by TerrainSlice.
struct TerrainPointState {
    int32_t start=0,end=0,x=0,height=0,dx=0,dh=0;
};
struct TerrainIteratorState {
    int32_t position=0,last_sample=0;
    size_t record=0;
    std::array<TerrainPointState,8> points{};
};
// Random source returns a nonnegative value, as the original CRT rand wrapper.
// Caller owns definition and random-stream lifetime; no per-sample random cache.
class TerrainIterator {
public:
    TerrainIterator(const TerrainDefinition& definition,int32_t position_q8,std::function<uint32_t()> random);
    void step(int32_t delta_q8);
    TerrainSlice slice(int32_t left_edge,int32_t right_edge) const;
    TerrainIteratorState diagnostic_state() const;
private:
    const TerrainDefinition& definition_;
    std::function<uint32_t()> random_;
    int32_t position_,last_sample_;
    std::array<size_t,2> wall_record_{};
    std::array<std::array<int32_t,3>,2> wall_values_{};
    size_t hill_record_=0;
    std::array<TerrainPointState,8> points_{};
    void hill_slopes(int32_t sample,int32_t delta);
};
}
