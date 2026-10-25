#include "actor_nodes.hpp"
#include "exe_tables.hpp"
#include <stdexcept>
namespace rr {
ActorNodes::ActorNodes(const Assets& assets, const std::vector<uint8_t>& exe, bool high)
    : library_(exe, assets.read("DATA/GLOBAL.RSC")), sprites_(load_rider_sprites(assets, exe, high)) {
    // Quarter-sin table for the 0x00450ad0 body rotation (0x00475768, 1024
    // int32 words, 32768 * sin(i * 90 / 1024); imported, never embedded).
    const auto bytes = read_exe_data(exe, 0x475768, 1024 * 4);
    sin_table_.reserve(1024);
    for (size_t i = 0; i < 1024; ++i)
        sin_table_.push_back(int32_t(uint32_t(bytes[4 * i]) | uint32_t(bytes[4 * i + 1]) << 8 |
                                     uint32_t(bytes[4 * i + 2]) << 16 | uint32_t(bytes[4 * i + 3]) << 24));
}
const CelTexture* ActorNodes::texture(size_t bank, uint32_t frame, int32_t colour) const {
    const auto& graphic = library_.graphics.at(bank).frames.at(frame);
    if (graphic.texture) return graphic.texture.get();
    if (!graphic.legacy_pixel_word) return nullptr;
    const int32_t palette = bank == 1 ? 0xde : bank == 2 ? 0xe0 : bank == 3 ? 0xdf : colour;
    const std::array<int32_t, 3> key{int32_t(bank), int32_t(frame), palette};
    auto found = pages_.find(key);
    if (found == pages_.end())
        found = pages_.emplace(key, rider_texture(sprites_, palette, int32_t(frame))).first;
    if (found->second) return &*found->second;
    if (!empty_texture_) empty_texture_ = CelTexture{};
    return &*empty_texture_;
}
std::vector<DrawNode> ActorNodes::rider(const ActorAnimationFrame& descriptor, size_t bank, int32_t colour,
                                        const ActorOrientation& orientation, ActorProjection projection,
                                        const Camera& camera, const DepthTable& depths, int32_t vertical_q8,
                                        const int32_t* body_roll_override, int32_t logical_width) const {
    if (bank >= 2) throw std::runtime_error("Invalid rider animation bank");
    const auto angles = actor_angles(orientation, camera.x);
    const auto view = select_actor_view(descriptor, library_.views[bank], angles.yaw, angles.roll);
    const auto& cell = library_.views[bank].at(view.group).at(view.subframe);
    std::vector<DrawNode> result;
    projection.flags = cell.flags;
    // 0x004463e0: shadow at terrain ground with rotation forced zero
    // (+0xbc = 0), then body with the select roll (+0xbc, yaw-clamped).
    // Bodies with nonzero roll take the 0x00450ad0 rotated path.
    projection.clip_ground = false;
    const auto& shadow = library_.graphics[bank + 2].frames.at(cell.frame);
    if (auto node = actor_node(shadow, texture(bank + 2, cell.frame, colour), projection, camera, depths,
                               logical_width))
        result.push_back(*node);
    projection.ground += vertical_q8 >> 8;
    projection.clip_ground = true;
    const auto& body = library_.graphics[bank].frames.at(cell.frame);
    int32_t body_roll = body_roll_override ? *body_roll_override : view.roll;
    if (!body_roll_override) {
        if (angles.yaw > 0xe0000)
            body_roll = 0x50000;
        else if (angles.yaw < -0xe0000)
            body_roll = -0x50000;
    }
    if (body_roll) {
        if (auto node = actor_node_rotated(body, texture(bank, cell.frame, colour), projection, camera,
                                           depths, sin_table_, body_roll, logical_width))
            result.push_back(*node);
    } else if (auto node = actor_node(body, texture(bank, cell.frame, colour), projection, camera, depths,
                                      logical_width))
        result.push_back(*node);
    return result;
}
} // namespace rr
