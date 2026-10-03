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
    std::cout<<failures<<" failures\n";return failures ? 1 : 0;
}
