#include "actor_nodes.hpp"
#include <stdexcept>
namespace rr {
ActorNodes::ActorNodes(const Assets& assets,const std::vector<uint8_t>& exe,bool high)
    :library_(exe,assets.read("DATA/GLOBAL.RSC")),sprites_(load_rider_sprites(assets,exe,high)){}
const CelTexture* ActorNodes::texture(size_t bank,uint32_t frame,int32_t colour)const {
    const auto& graphic=library_.graphics.at(bank).frames.at(frame);
    if(graphic.texture)return graphic.texture.get();
    if(!graphic.legacy_pixel_word)return nullptr;
    const int32_t palette=bank==1?0xde:bank==2?0xe0:bank==3?0xdf:colour;
    const std::array<int32_t,3> key{int32_t(bank),int32_t(frame),palette};
    auto found=pages_.find(key);
    if(found==pages_.end())found=pages_.emplace(key,rider_texture(sprites_,palette,int32_t(frame))).first;
    return found->second?&*found->second:nullptr;
}
std::vector<DrawNode> ActorNodes::rider(const ActorAnimationFrame& descriptor,size_t bank,int32_t colour,
    const ActorOrientation& orientation,ActorProjection projection,const Camera& camera,const DepthTable& depths,int32_t vertical_q8)const {
    if(bank>=2)throw std::runtime_error("Invalid rider animation bank");
    const auto angles=actor_angles(orientation,camera.x);
    const auto view=select_actor_view(descriptor,library_.views[bank],angles.yaw,angles.roll);
    const auto& cell=library_.views[bank].at(view.group).at(view.subframe);
    std::vector<DrawNode> result;
    projection.flags=cell.flags;
    // 0x00446250: shadow at terrain ground, then body including vertical Q8
    // displacement. Bikes force rotation zero for both display nodes.
    projection.clip_ground=false;
    const auto& shadow=library_.graphics[bank+2].frames.at(cell.frame);
    if(auto node=actor_node(shadow,texture(bank+2,cell.frame,colour),projection,camera,depths))result.push_back(*node);
    projection.ground+=vertical_q8>>8;projection.clip_ground=true;
    const auto& body=library_.graphics[bank].frames.at(cell.frame);
    if(auto node=actor_node(body,texture(bank,cell.frame,colour),projection,camera,depths))result.push_back(*node);
    return result;
}
}
