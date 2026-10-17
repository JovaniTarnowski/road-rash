#pragma once
#include "assets.hpp"
#include "course.hpp"
#include "family.hpp"
#include "road_materials.hpp"
#include "terrain_panels.hpp"
#include "scenery.hpp"
#include "building_panels.hpp"
#include "actor_descriptors.hpp"
#include <functional>

namespace rr {
class UpscaleCache;
// A pedestrian family's FAMILIES.RSC category-2 resources, as 0x00440b80
// loads them: resource 0 RRAN graphics, resource 1 CANS views, and the
// animation codes 0..6 built from the EXE descriptor tables 0x0046de48,
// 0x0046de78, 0x0046de80, 0x0046de90, 0x0046dec0, 0x0046ded0 and 0x0046df00
// (0x0040b6a0). Resources 2/3 are 'MROF' voices: big-endian word +8 is the
// speech argument 0x00440730 byte-swaps for sound event 0x16 (-1 if absent).
struct PedestrianLook {
    Animation graphics;
    ActorGraphicsGroups views;
    ActorAnimationBank bank;
    std::array<int32_t,2> voices{-1,-1};
};
// Owned imported resources and generated course terrain. Construction never
// uses oracle snapshots or host pointers from the original executable.
class CourseScene {
public:
    CourseScene(const Assets&,const std::vector<uint8_t>& exe,const Course&,
                const std::string& course_name,uint32_t terrain_seed=1,int level=0);
    const TerrainSlice& terrain(uint32_t section,uint32_t local) const;
    // Oracle fixtures: replace one sample's terrain values (e.g. those an
    // original scene dump holds, drawn from the live race random stream).
    void set_terrain(uint32_t section,uint32_t local,const TerrainSlice& slice) {terrain_.at(section).at(local)=slice;}
    // Plan 20 Task 3: family textures for generation (read-only walk).
    const std::map<uint32_t, Family>& families() const { return families_; }
    // Plan 20 Task 3: attach cached MMPX levels to every family texture.
    void attach_upscaled(const class UpscaleCache& cache);
    void clear_upscaled();
    const CelTexture* material(uint32_t section,uint32_t local,uint8_t selector,
        uint8_t category,std::optional<uint32_t> frame,int32_t width,int32_t height) const;
    // Emits imported road, shoulder, hill, building and scenery geometry.
    // Marked transitions and split stitching remain integration work.
    using ActorEmitter=std::function<std::vector<DrawNode>(const RoadSlice&,const RoadSlice&,int,int32_t,bool)>;
    std::vector<DrawNode> draw_nodes(const std::vector<RouteSample>& route,
        const std::vector<RoadSlice>& slices,const Camera&,const DepthTable&,ActorEmitter actors={},int32_t logical_width=320) const;
    void draw_horizon(Framebuffer&,int32_t scroll,int32_t logical_width=320) const;
    const std::vector<SceneryObject>& objects(uint32_t section,uint32_t local) const;
    const AnimationFrame* object_frame(uint32_t section,uint32_t local,const SceneryObject&) const;
    // DZHR actor records placed on this sample (place_dzhr), in original order.
    const std::vector<DzhrActor>& dzhr_actors(uint32_t section,uint32_t local) const;
    // 0x0044acd0(selector, 2, n) against the families resident at this
    // sample; nullptr when resource 0 or 1 is missing (0x00440680 then
    // never loads or draws the pedestrian).
    const PedestrianLook* pedestrian_look(uint32_t section,uint32_t local,uint8_t selector) const;
    // Marking flag 0x40 on a mountain sample: drawn as a tunnel (type 3).
    bool tunnel(uint32_t section,uint32_t local) const;
private:
    const CelTexture* tunnel_material(uint32_t section,uint32_t local,uint8_t selector,uint32_t lit,uint8_t kind,int32_t width,int32_t height) const;
    int32_t sine(int32_t angle) const;
    const CelGroup* rail_group(uint32_t section,uint32_t local,uint8_t selector) const;
    const CelGroup* building_group(uint32_t section,uint32_t local,uint8_t selector) const;
    static int32_t approximate_angle(int32_t x,int32_t y);
    std::vector<std::vector<bool>> tunnels_;
    uint8_t side_street_run_=2;                  // 0x00465a40[0]
    std::array<int32_t,8> arch_{};               // 0x00465a54 tunnel heights / fractions
    std::array<int32_t,257> sine_words_{};       // 0x004766f0
    std::array<uint8_t,128> sine_bytes_{};       // 0x00476870
    std::vector<std::vector<TerrainSlice>> terrain_;
    std::vector<std::vector<std::array<uint16_t,4>>> family_slots_;
    std::map<uint32_t,Family> families_;
    RoadMaterials roads_;
    Cel water_;  // GLOBAL.RSC CEL 0x10, cliff-foot water (0x0040d030)
    std::vector<uint8_t> logarithms_,horizon_;
    std::array<RasterPoint,16> joins_{};
    std::vector<std::vector<std::vector<SceneryObject>>> objects_;
    std::vector<std::vector<std::vector<DzhrActor>>> actors_;
    std::array<std::vector<uint8_t>,7> pedestrian_tables_;
    mutable std::map<std::pair<uint32_t,uint32_t>,std::optional<PedestrianLook>> pedestrians_;
    mutable std::map<std::pair<uint32_t,uint32_t>,Animation> animations_;
};
}
