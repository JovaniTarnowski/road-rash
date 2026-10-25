// Plan 25 Task 7: texture_edge_wide must never write past the scratch.
// A segment wholly below the surface (from.y >= height) drove the trailing
// sentinel re-fill loop past the vector end: heap-buffer-overflow under ASan
// (hires-render and race-soak reports at texture.cpp:120), silent heap
// corruption otherwise. Out-of-bounds rows clip; edge math on in-bounds
// rows is bit-identical (pinned by the spanning case below).
// Synthetic, always runs (no exit 77).
#include "texture.hpp"
#include <iostream>

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL " << what << '\n';
    }
}
bool same_row(const rr::TextureEdge64& a, const rr::TextureEdge64& b) {
    return a.x == b.x && a.u == b.u && a.v == b.v;
}
bool same_rows(const rr::RasterScratch64& a, const rr::RasterScratch64& b) {
    if (a.rows.size() != b.rows.size()) return false;
    for (size_t i = 0; i < a.rows.size(); ++i)
        if (!same_row(a.rows[i], b.rows[i])) return false;
    return true;
}
// All-sentinel reference: an above-surface edge fills every row with the
// empty sentinel and computes no in-bounds row.
rr::RasterScratch64 all_sentinel(int height) {
    rr::RasterScratch64 scratch(height);
    rr::TextureVertexWide a{10, -200, 0, 0}, b{20, -100, 64, 64};
    rr::texture_edge_wide(scratch, a, b);
    return scratch;
}
} // namespace

int main() {
    // 1. Tall-scratch case from the ASan report: a 1920-row scratch with an
    // edge wholly below the surface. Pre-fix this writes past the vector
    // end (ASan aborts); post-fix every row keeps the empty sentinel.
    {
        rr::RasterScratch64 scratch(1920);
        const rr::RasterScratch64 expect = all_sentinel(1920);
        rr::TextureVertexWide a{10, 2000, 0, 0}, b{20, 2100, 64, 64};
        rr::texture_edge_wide(scratch, a, b);
        check(same_rows(scratch, expect), "below-surface edge on a tall scratch clips");
    }
    // 2. Small scratch, same shape: edge below the scratch end.
    {
        rr::RasterScratch64 scratch(64);
        const rr::RasterScratch64 expect = all_sentinel(64);
        rr::TextureVertexWide a{10, 100, 0, 0}, b{20, 200, 64, 64};
        rr::texture_edge_wide(scratch, a, b);
        check(same_rows(scratch, expect), "below-surface edge leaves every row untouched");
    }
    // 3. Edge starting exactly at the scratch end.
    {
        rr::RasterScratch64 scratch(64);
        const rr::RasterScratch64 expect = all_sentinel(64);
        rr::TextureVertexWide a{10, 64, 0, 0}, b{20, 100, 64, 64};
        rr::texture_edge_wide(scratch, a, b);
        check(same_rows(scratch, expect), "edge at the scratch end leaves every row untouched");
    }
    // 4. Spanning edge: in-bounds rows are bit-identical to the same edge
    // computed on a taller scratch (clipping changes no in-bounds math).
    {
        rr::RasterScratch64 short_s(64), tall(256);
        rr::TextureVertexWide a{10, -10, 0, 0}, b{20, 200, 64, 64};
        rr::texture_edge_wide(short_s, a, b);
        rr::texture_edge_wide(tall, a, b);
        bool identical = true;
        for (size_t y = 0; y < 64; ++y) identical &= same_row(short_s.rows[y], tall.rows[y]);
        check(identical, "spanning edge bit-identical on in-bounds rows");
    }
    // 5. In-bounds edge still spans every row (existing behaviour kept).
    {
        rr::RasterScratch64 scratch(64);
        rr::TextureVertexWide a{10, 0, 0, 0}, b{20, 63, 64, 64};
        rr::texture_edge_wide(scratch, a, b);
        check(scratch.rows[0].x == (int64_t(10) << 16), "in-bounds edge starts at from.x");
        check(scratch.rows[0].x != all_sentinel(64).rows[0].x, "in-bounds edge computes row 0");
    }
    if (failures == 0) std::cout << "texture_edge OK\n";
    return failures ? 1 : 0;
}
