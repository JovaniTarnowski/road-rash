// Plan 20 Task 3: generate + attach MMPX levels for in-race indexed art.
#include "upscale_import.hpp"
#include "assets.hpp"
#include "cel.hpp"
#include "course.hpp"
#include "course_scene.hpp"
#include "mmpx.hpp"
#include "framebuffer.hpp"
#include "resource.hpp"
#include "rider_sprites.hpp"
#include "road_materials.hpp"
#include "upscale_cache.hpp"
#include <array>

namespace rr {
namespace {
// Source bytes for a cache entry: the mip-0 index bytes (hash identity).
std::vector<uint8_t> mip_source(const CelMip& m) {
    const size_t stride = size_t(1) << m.stride_shift;
    std::vector<uint8_t> out;
    out.reserve(size_t(m.width) * m.height);
    for (int y = 0; y < m.height; ++y)
        out.insert(out.end(), m.pixels.begin() + ptrdiff_t(y) * ptrdiff_t(stride),
                   m.pixels.begin() + ptrdiff_t(y) * ptrdiff_t(stride) + m.width);
    return out;
}
UpscaledImage to_entry(const Image8& img) {
    UpscaledImage e;
    e.width = img.width;
    e.height = img.height;
    e.indexed = true;
    e.indices = img.pixels;
    return e;
}
// Generate (or hit the cache for) one texture's 2x/4x levels. Returns false on cancel.
bool generate_one(const CelMip& mip, const UpscaleCache& cache, const std::string& asset, uint32_t index,
                  const std::array<uint32_t, 256>& luma, const std::function<bool(int, int)>& progress,
                  int& done, int total) {
    if (mip.width <= 0 || mip.height <= 0) return true;
    const auto src = mip_source(mip);
    Image8 im{mip.width, mip.height, src};
    for (int scale : {2, 4}) {
        if (cache.find(asset, index, scale, src, {UpscaleMethod::mmpx})) continue;
        const Image8 up = scale == 2 ? mmpx2x(im, luma, -1) : mmpx4x(im, luma, -1);
        if (!cache.store(asset, index, scale, src, UpscaleMethod::mmpx, to_entry(up))) return false;
        if (!progress(done++, total)) return false;
    }
    return true;
}
} // namespace

void attach_upscaled(CelTexture& tex, const UpscaleCache& cache, const std::string& asset, uint32_t index) {
    tex.upscaled.clear();
    if (tex.mips.empty()) return;
    const auto& m0 = tex.mips[0];
    const auto src = mip_source(m0);
    for (int scale : {2, 4}) {
        const auto found = cache.find(asset, index, scale, src, {UpscaleMethod::mmpx, UpscaleMethod::ai});
        if (!found || !found->indexed) continue;
        CelMip up;
        up.width = found->width;
        up.height = found->height;
        unsigned shift = 0;
        while ((1u << shift) < size_t(up.width)) ++shift;
        up.stride_shift = shift;
        up.pixels = found->indices;
        tex.upscaled.push_back(std::move(up));
    }
}
void attach_upscaled_indexed(CelTexture& tex, const UpscaleIndex& index) {
    tex.upscaled.clear();
    if (tex.mips.empty()) return;
    const auto& m0 = tex.mips[0];
    const auto src = mip_source(m0);
    const auto h = hash_source32(src);
    for (int scale : {2, 4}) {
        const auto it = index.find(std::make_pair(h, scale));
        if (it == index.end() || !it->second.indexed) continue;
        const auto& found = it->second;
        CelMip up;
        up.width = found.width;
        up.height = found.height;
        unsigned shift = 0;
        while ((1u << shift) < size_t(up.width)) ++shift;
        up.stride_shift = shift;
        up.pixels = found.indices;
        tex.upscaled.push_back(std::move(up));
    }
}
void attach_upscaled_content(CelTexture& tex, const UpscaleCache& cache) {
    tex.upscaled.clear();
    if (tex.mips.empty()) return;
    const auto& m0 = tex.mips[0];
    const auto src = mip_source(m0);
    for (int scale : {2, 4}) {
        const auto found = cache.find_by_content(src, scale, {UpscaleMethod::mmpx, UpscaleMethod::ai});
        if (!found || !found->indexed) continue;
        CelMip up;
        up.width = found->width;
        up.height = found->height;
        unsigned shift = 0;
        while ((1u << shift) < size_t(up.width)) ++shift;
        up.stride_shift = shift;
        up.pixels = found->indices;
        tex.upscaled.push_back(std::move(up));
    }
}

void generate_upscaled(const Assets& assets, const UpscaleCache& cache,
                       const std::function<bool(int done, int total)>& progress) {
    const auto palette = Palette::from_raw(assets.read("DATA/PALETTE.RAW"));
    const auto luma = mmpx_luma(palette.rgbx);
    const auto exe = assets.read("ROADRASH.EXE");
    struct Job {
        std::string asset;
        uint32_t index;
        CelMip mip;
    };
    std::vector<Job> jobs;
    auto fnv = [](uint32_t a, uint32_t b, uint32_t c) {
        uint32_t h = 2166136261u;
        for (uint32_t v : {a, b, c}) {
            for (int i = 0; i < 4; ++i) {
                h ^= uint8_t(v >> (i * 8));
                h *= 16777619u;
            }
        }
        return h;
    };
    // Family cells per course scene (same granularity the scene attaches).
    for (const char* course : {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"}) {
        const std::string asset = std::string("DATA/COURSES/") + course + ".CRS";
        if (!assets.exists(asset)) continue;
        Course course_data = read_course(assets.read(asset));
        CourseScene scene(assets, exe, course_data, course, 1, 0);
        for (const auto& [famid, family] : scene.families())
            for (const auto& [groupoff, group] : family.groups)
                for (size_t ci = 0; ci < group.cells.size(); ++ci) {
                    const auto& tex = group.cells[ci].texture;
                    if (tex.mips.empty()) continue;
                    jobs.push_back({"DATA/FAMILIES.RSC", fnv(famid, groupoff, uint32_t(ci)), tex.mips[0]});
                }
    }
    // Road + terrain materials (GLOBAL.RSC road entries).
    {
        RoadMaterials roads = read_road_materials(exe, assets.read("DATA/GLOBAL.RSC"));
        uint32_t index = 0;
        auto push_tex = [&](const CelTexture& tex) {
            if (tex.mips.empty()) return;
            jobs.push_back({"DATA/GLOBAL.RSC", 0x80000000u | (index++), tex.mips[0]});
        };
        for (const auto& entry : roads.entries)
            if (entry.texture) push_tex(*entry.texture);
        for (const auto& terrain : roads.terrain)
            if (terrain.texture) push_tex(*terrain.texture);
    }
    // Rider/cop/shadow bob sheets.
    {
        const auto exe = assets.read("ROADRASH.EXE");
        RiderSprites sprites = load_rider_sprites(assets, exe, true);
        uint32_t index = 0;
        for (const auto* sheet : {&sprites.bob, &sprites.cop, &sprites.shadow, &sprites.shadoc}) {
            for (const auto& bob : *sheet) {
                if (bob.width <= 0 || bob.height <= 0) {
                    ++index;
                    continue;
                }
                CelMip m;
                m.width = bob.width;
                m.height = bob.height;
                unsigned shift = 0;
                while ((1u << shift) < size_t(bob.width)) ++shift;
                m.stride_shift = shift;
                m.pixels = bob.pixels;
                jobs.push_back({"DATA/GLOBAL.RSC", index++, std::move(m)});
            }
        }
    }
    const int total = int(jobs.size() * 2);
    int done = 0;
    if (!progress(0, total)) return;
    for (const auto& job : jobs) {
        if (!generate_one(job.mip, cache, job.asset, job.index, luma, progress, done, total)) return;
    }
    progress(total, total);
}
} // namespace rr
