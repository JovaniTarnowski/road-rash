#pragma once
#include "animation.hpp"
#include "course.hpp"
#include "road.hpp"
#include "world_renderer.hpp"
#include <functional>
namespace rr {
struct SceneryObject {uint32_t sample=0;int32_t lateral=0;uint8_t selector=0,size=8,type=0;bool mirror=false,static_family=false;};
std::vector<SceneryObject> roadside_objects(const CourseSection&,std::function<uint32_t()> random);
std::vector<SceneryObject> placed_scenery(const CourseSection&,int level,std::function<uint32_t()> random);
// Billboard construction from 0x0044b630 / 0x00450960. Caller supplies the
// interpolated road centre/elevation and terrain ground height at the object.
DrawNode scenery_node(const AnimationFrame&,const SceneryObject&,int32_t road_x,
    int32_t road_y,int32_t ground_y,int32_t depth,const Camera&,const DepthTable&);
}
