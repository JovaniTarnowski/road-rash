#pragma once
#include "actor_animation.hpp"
#include "animation.hpp"
namespace rr {
struct ActorGraphicsDescriptor {
    ActorAnimationFrame playback;
    uint32_t source_frame = 0;
};
// 0x0040b200 / 0x0040b6a0. source has 8 bytes per EXE descriptor.
std::optional<std::vector<ActorGraphicsDescriptor>> read_actor_sequence(const std::vector<uint8_t>& cans,
                                                                        const std::vector<uint8_t>& source);
struct ActorGraphicsCell {
    uint32_t frame = 0, flags = 0; // zero-based RRAN frame and CANS view flags
};
using ActorGraphicsGroups = std::vector<std::vector<ActorGraphicsCell>>;
// Owned counterpart of 0x0040b730 / 0x0040b620 / 0x0040b6f0.
ActorGraphicsGroups read_actor_graphics(const std::vector<uint8_t>& cans);
struct ActorViewSelection {
    uint32_t group = 0, subframe = 0;
    int32_t roll = 0;
};
// View indices from 0x0043af60 after 0x00413ae0 supplies yaw/roll.
ActorViewSelection select_actor_view(const ActorAnimationFrame& frame, const ActorGraphicsGroups& views,
                                     int32_t yaw, int32_t roll);
struct ActorAnimationLibrary {
    std::vector<int8_t> fallback;
    std::array<ActorAnimationBank, 2> playback;
    std::array<std::vector<std::optional<std::vector<ActorGraphicsDescriptor>>>, 2> descriptors;
    std::array<ActorGraphicsGroups, 2> views;
    std::array<Animation, 4> graphics; // rider, cop, rider shadow, cop shadow
    ActorAnimationLibrary(const std::vector<uint8_t>& exe, const std::vector<uint8_t>& global);
};
} // namespace rr
