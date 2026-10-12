#include "raster.hpp"
#include <iostream>
#include <algorithm>
int main() {
    int failures=0;
    auto check=[&](bool yes,const char* message) { if (!yes) { ++failures;std::cerr<<message<<'\n'; } };
    rr::Framebuffer fb(8,8);fb.clear(0);
    rr::RasterQuad rectangle{{{{1,1},{5,1},{5,5},{1,5}}},7};
    rr::fill_quad(fb,rectangle,{0,0,8,7});
    for (int y=0;y<8;++y) for (int x=0;x<8;++x) check(fb.row(y)[x]==((x>=1 && x<5 && y>=1 && y<=5) ? 7 : 0),"rectangle inclusive bottom/exclusive right");
    fb.clear(0);rr::RasterQuad diamond{{{{3,0},{6,3},{3,6},{0,3}}},9};
    rr::fill_quad(fb,diamond,{0,0,8,7});
    const int widths[]={0,2,4,6,4,2,0,0};
    for (int y=0;y<8;++y) { int pixels=0;for (int x=0;x<8;++x) pixels+=fb.row(y)[x]==9;check(pixels==widths[y],"diamond shared endpoint replacement"); }
    fb.clear(0);rr::RasterQuad horizontal{{{{-2,2},{7,2},{4,2},{0,2}}},11};
    rr::fill_quad(fb,horizontal,{1,0,5,7});
    for (int x=0;x<8;++x) check(fb.row(2)[x]==((x>=1 && x<5) ? 11 : 0),"horizontal degenerate clipped span");
    fb.clear(0);rr::RasterQuad negative{{{{-4,-3},{4,-3},{4,3},{-4,3}}},13};
    rr::fill_quad(fb,negative,{0,1,8,2});
    for (int y=0;y<8;++y) for (int x=0;x<8;++x) check(fb.row(y)[x]==((x<4 && y>=1 && y<=2) ? 13 : 0),"negative vertices and top/bottom clipping");
    fb.clear(0);rr::RasterQuad triangle{{{{1,1},{1,1},{6,6},{1,6}}},17};
    rr::fill_quad(fb,triangle,{0,0,8,7});
    check(fb.row(1)[1]==0 && fb.row(6)[1]==17 && fb.row(6)[5]==17,"duplicate vertex triangle");
    auto rotated=rectangle;rotated.points={rectangle.points[2],rectangle.points[3],rectangle.points[0],rectangle.points[1]};
    rr::Framebuffer other(8,8);other.clear(0);fb.clear(0);rr::fill_quad(fb,rectangle,{0,0,8,7});rr::fill_quad(other,rotated,{0,0,8,7});
    for (int y=0;y<8;++y) check(std::equal(fb.row(y),fb.row(y)+8,other.row(y)),"rotation preserves fill");
    // Band-parallel contract (plan 19 Task 6): union of bands covers every
    // row exactly once; banded fill equals the single fill. Edge steps come
    // from original endpoints before band clipping (fill_quad takes the full
    // quad plus a row clip per band).
    for (int bands:{1,2,4,7}) {
        const auto ranges=rr::band_rows(8,bands);
        int covered=0;for (const auto& r:ranges) covered+=r.second-r.first+1;
        check(covered==8,"band union covers all rows");
        for (size_t i=1;i<ranges.size();++i) check(ranges[i].first==ranges[i-1].second+1,"bands are contiguous");
        rr::Framebuffer banded(8,8);banded.clear(0);
        for (const auto& r:ranges) rr::fill_quad(banded,diamond,{0,r.first,8,r.second});
        rr::Framebuffer whole(8,8);whole.clear(0);rr::fill_quad(whole,diamond,{0,0,8,7});
        for (int y=0;y<8;++y) check(std::equal(banded.row(y),banded.row(y)+8,whole.row(y)),"banded fill is bit-identical");
    }
    // Crossing polygon: vertices above band 1 reach into band 3.
    {
        rr::Framebuffer whole(16,24);whole.clear(0);
        rr::RasterQuad cross{{{{2,-4},{13,-4},{13,20},{2,20}}},5};
        rr::fill_quad(whole,cross,{0,0,16,23});
        rr::Framebuffer banded(16,24);banded.clear(0);
        for (const auto& r:rr::band_rows(24,3)) rr::fill_quad(banded,cross,{0,r.first,16,r.second});
        for (int y=0;y<24;++y) check(std::equal(banded.row(y),banded.row(y)+16,whole.row(y)),"crossing polygon has no seams");
    }
    try { rr::band_rows(8,0);check(false,"bands<1 throws"); } catch (const std::runtime_error&) {}
    std::cout<<failures<<" failures\n";return failures ? 1 : 0;
}
