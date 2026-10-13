// Plan 20 Task 3: upscaled in-race texture generation + attach.
#pragma once
#include <array>
#include <map>
#include "cel.hpp"
#include "upscale_cache.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace rr {
class Assets;
class UpscaleCache;
struct CelTexture;
struct RiderSprites;
// Generate MMPX 2x/4x entries for every in-race indexed texture (course CELs,
// rider/cop/shadow sheets, horizons). Returns false if progress() returns false
// (cancel: no .tmp files left behind). progress(done, total) is called per item.
void generate_upscaled(const Assets& assets, const UpscaleCache& cache,
                       const std::function<bool(int done, int total)>& progress);
// Attach cached 2x/4x levels to one texture (no-op when absent).
void attach_upscaled(CelTexture& tex, const UpscaleCache& cache, const std::string& asset, uint32_t index);
// Content-addressed attach (plan 20 Task 3): matches by mip-0 bytes, no identity.
void attach_upscaled_content(CelTexture& tex, const UpscaleCache& cache);
// Batch attach from one index walk (same semantics, no repeated walks).
using UpscaleIndex = std::map<std::pair<std::array<uint8_t, 32>, int>, UpscaledImage>;
void attach_upscaled_indexed(CelTexture& tex, const UpscaleIndex& index);
}  // namespace rr
