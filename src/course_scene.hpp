#pragma once
#include "assets.hpp"
#include "course.hpp"
#include "family.hpp"
#include "road_materials.hpp"
#include "terrain_panels.hpp"
#include "scenery.hpp"
#include "building_panels.hpp"

namespace rr {
// Owned imported resources and generated course terrain. Construction never
// uses oracle snapshots or host pointers from the original executable.
class CourseScene {
public:
    CourseScene(const Assets&,const std::vector<uint8_t>& exe,const Course&,
                const std::string& course_name,uint32_t terrain_seed=1,int level=0);
    const TerrainSlice& terrain(uint32_t section,uint32_t local) const;
    const CelTexture* material(uint32_t section,uint32_t local,uint8_t selector,
        uint8_t category,std::optional<uint32_t> frame,int32_t width,int32_t height) const;
    // Emits imported road, shoulder, hill, building and scenery geometry.
    // Marked transitions and split stitching remain integration work.
    std::vector<DrawNode> draw_nodes(const std::vector<RouteSample>& route,
        const std::vector<RoadSlice>& slices,const Camera&,const DepthTable&) const;
    void draw_horizon(Framebuffer&,int32_t scroll) const;
    const std::vector<SceneryObject>& objects(uint32_t section,uint32_t local) const;
    const AnimationFrame* object_frame(uint32_t section,uint32_t local,const SceneryObject&) const;
private:
    std::vector<std::vector<TerrainSlice>> terrain_;
    std::vector<std::vector<std::array<uint16_t,4>>> family_slots_;
    std::map<uint32_t,Family> families_;
    RoadMaterials roads_;
    std::vector<uint8_t> logarithms_,horizon_;
    std::array<RasterPoint,16> joins_{};
    std::vector<std::vector<std::vector<SceneryObject>>> objects_;
    mutable std::map<std::pair<uint32_t,uint32_t>,Animation> animations_;
};
}
