#pragma once
#include "actor_descriptors.hpp"
#include "actor_render.hpp"
#include "exe_tables.hpp"
#include "rider_sprites.hpp"
#include <map>
#include <vector>
namespace rr {
// Owns imported graphics and stable remapped Bob pages for display-list nodes.
// Animation stepping belongs to the simulation, never to frame presentation.
class ActorNodes {
public:
    ActorNodes(const Assets&,const std::vector<uint8_t>& exe,bool high_resolution);
    const ActorAnimationLibrary& library()const{return library_;}
    std::vector<DrawNode> rider(const ActorAnimationFrame&,size_t bank,int32_t colour,
        const ActorOrientation&,ActorProjection,const Camera&,const DepthTable&,int32_t vertical_q8=0,
        const int32_t* body_roll_override=nullptr,int32_t logical_width=320)const;
    const CelTexture* texture(size_t graphics_bank,uint32_t frame,int32_t colour)const;
private:
    ActorAnimationLibrary library_;
    RiderSprites sprites_;
    mutable std::map<std::array<int32_t,3>,std::optional<CelTexture>> pages_;
    // 0x004436a0 leaves a null pixel pointer for zero-size sheet frames
    // (~130 of the 326 shadow frames on disc); 0x0044ba40 still emits the
    // node and the rasterizer draws nothing. A mip-less texture marks that.
    mutable std::optional<CelTexture> empty_texture_;
    std::vector<int32_t> sin_table_;   // quarter-sin for body rotation (0x00475768)
};
}
