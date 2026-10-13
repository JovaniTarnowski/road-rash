#pragma once
// Plan 19 Task 6: fixed worker pool for band-parallel rasterisation.
// Threads are created once and reused; no thread creation per frame.
#include "world_renderer.hpp"
#include <functional>
namespace rr {
class RenderThreads {
public:
    explicit RenderThreads(int threads);          // 1 = run inline, no threads created
    ~RenderThreads();
    RenderThreads(const RenderThreads&) = delete;
    RenderThreads& operator=(const RenderThreads&) = delete;
    // Calls job(band, top_row, bottom_row_inclusive) for `bands` equal row
    // ranges and waits. Bands split rows [0, height) as evenly as possible.
    void run(int bands, int height, const std::function<void(int,int,int)>& job);
    int count() const { return threads_; }
private:
    struct Impl;
    Impl* impl_ = nullptr;
    int threads_ = 1;
};
void render_nodes_threaded(Framebuffer&, const std::vector<DrawNode>&, const ViewMode&, uint32_t flat_mask, RenderThreads&, bool perspective = false);
}
