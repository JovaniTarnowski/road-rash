#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <functional>
namespace rr {
// The runtime animation descriptors are separate from RRAN graphics frames.
// 0x0040b200 builds each 20-byte entry from CANS and an imported EXE descriptor.
struct ActorAnimationFrame {
    std::array<uint8_t, 12> render{};
    int8_t duration = 0;
    uint8_t control = 0;  // low nibble action, high nibble loop frame
    bool drawable = true; // original descriptor +16 is nonzero
    uint32_t flags = 0;   // original graphics descriptor +4 (combat bits 0x600000)
};
using ActorSequence = std::vector<ActorAnimationFrame>;
using ActorAnimationBank = std::vector<std::optional<ActorSequence>>;
struct ActorAnimationState {
    int8_t animation = -1, frame = 0, queued = -1;
    uint8_t loops = 0;
    uint32_t deadline = 0;
    // Original +0xc retains the frame selected BEFORE this call advances.
    std::optional<std::array<int8_t, 2>> selected;
};
// 0x0043ae10 / 0x0043ae60. An unavailable selection leaves state unchanged.
bool set_animation(ActorAnimationState&, const ActorAnimationBank&, int8_t code, uint32_t tick);
bool queue_animation(ActorAnimationState&, const ActorAnimationBank&, int8_t code);
// 0x0040ac70 retries the EXE fallback mapping only if direct selection fails.
bool set_animation_with_fallback(ActorAnimationState&, const ActorAnimationBank&, int8_t code, uint32_t tick,
                                 const std::vector<int8_t>& fallback);
bool queue_animation_with_fallback(ActorAnimationState&, const ActorAnimationBank&, int8_t code,
                                   const std::vector<int8_t>& fallback);
struct RidingAnimationInput {
    int32_t speed = 0, throttle = 0, lateral_speed = 0, boost = 0, camera_distance = 0;
    uint8_t flags = 0;
    int8_t wreck_count = 0;
    bool player = true, wrecked = false;
};
// Normal riding portion of 0x00407f30. Crash dispatch and attacks are separate.
const ActorAnimationFrame* advance_riding_animation(ActorAnimationState&, const ActorAnimationBank&,
                                                    const std::vector<int8_t>& fallback,
                                                    const RidingAnimationInput&, uint32_t tick);
// Crash-bike animation branch of 0x00407a90 (states 1/2 only).
const ActorAnimationFrame* advance_crash_bike_animation(ActorAnimationState&, const ActorAnimationBank&,
                                                        const std::vector<int8_t>& fallback, int32_t speed,
                                                        uint32_t tick,
                                                        const std::function<uint32_t()>& random);
struct CrashRiderAnimationInput {
    int32_t crash = 1, speed = 0, height_q8 = 0;
};
// Crash rider branch of 0x00407f30; a stopped rider moves state 1 to 2.
const ActorAnimationFrame* advance_crash_rider_animation(ActorAnimationState&, const ActorAnimationBank&,
                                                         const std::vector<int8_t>& fallback,
                                                         CrashRiderAnimationInput&, uint32_t tick,
                                                         const std::function<uint32_t()>& random);
// 0x0043ae90: at most one transition per call; deadlines accumulate from the
// previous deadline rather than restarting at the current simulation tick.
const ActorAnimationFrame* advance_animation(ActorAnimationState&, const ActorAnimationBank&, uint32_t tick);
} // namespace rr
