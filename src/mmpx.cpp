#include "mmpx.hpp"
#include <stdexcept>
namespace rr {
std::array<uint32_t,256> mmpx_luma(const std::array<uint8_t,1024>& rgbx) {
    std::array<uint32_t,256> out{};
    for (int i = 0; i < 256; ++i)
        out[size_t(i)] = uint32_t(rgbx[size_t(i)*4]) + uint32_t(rgbx[size_t(i)*4+1]) + uint32_t(rgbx[size_t(i)*4+2]);
    return out;
}
namespace {
// 3x3 neighbourhood with edge clamping. Labels follow the MMPX paper
// (McGuire & Gagiu 2021, Listings 3-4): centre C, orthogonal N/S/E/W,
// diagonals NW/NE/SW/SE.
struct Neighbourhood { uint8_t n,s,e,w,nw,ne,sw,se,c; };
uint8_t at(const Image8& img,int x,int y) {
    if (x<0) x=0;if (y<0) y=0;
    if (x>=img.width) x=img.width-1;if (y>=img.height) y=img.height-1;
    return img.pixels[size_t(y)*size_t(img.width)+size_t(x)];
}
bool rgb_equal(uint8_t a,uint8_t b) { return a==b; }
} // namespace
Image8 mmpx2x(const Image8& src, const std::array<uint32_t,256>& luma, int transparent) {
    if (src.width<=0 || src.height<=0 || src.width>1024 || src.height>1024)
        throw std::runtime_error("MMPX source dimensions out of range");
    if (src.pixels.size()!=size_t(src.width)*size_t(src.height))
        throw std::runtime_error("MMPX source size mismatch");
    if (transparent<-1 || transparent>255) throw std::runtime_error("MMPX transparent out of range");
    Image8 out{src.width*2, src.height*2, std::vector<uint8_t>(size_t(src.width)*size_t(src.height)*4)};
    auto bright = [&](uint8_t i) { return luma[i]; };
    for (int y = 0; y < src.height; ++y) for (int x = 0; x < src.width; ++x) {
        Neighbourhood h{at(src,x,y-1),at(src,x,y+1),at(src,x+1,y),at(src,x-1,y),
            at(src,x-1,y-1),at(src,x+1,y-1),at(src,x-1,y+1),at(src,x+1,y+1),at(src,x,y)};
        // 2x2 output block: [o00 o10; o01 o11].
        uint8_t o00=h.c,o10=h.c,o01=h.c,o11=h.c;
        // Flat/dither rule: pairs of equal orthogonals keep the centre.
        // Diagonal rule (paper Listing 3): where exactly one diagonal pair
        // agrees through the centre, extend that diagonal's colour into the
        // two adjacent output pixels; ties break by RGB-sum brightness
        // (alpha-aware in the paper; our inputs are opaque indices, and the
        // transparent index is never blended, so RGB sum suffices).
        struct Diag { uint8_t corner, o_a, o_b; uint8_t *pa, *pb; };
        // Evaluated per corner; each corner's rule writes its two neighbours.
        auto corner_rule = [&](uint8_t corner, uint8_t adj_a, uint8_t adj_b,
                               uint8_t *pa, uint8_t *pb) {
            if (rgb_equal(corner,h.c) || corner==uint8_t(transparent) || h.c==uint8_t(transparent)) return;
            if (!rgb_equal(adj_a,h.c) && !rgb_equal(adj_b,h.c)) return;
            // Only connect when the corner agrees with neither orthogonal
            // neighbour chain except through the centre diagonal.
            if (rgb_equal(adj_a,corner) || rgb_equal(adj_b,corner)) return;
            uint32_t bc=bright(h.c),bk=bright(corner);
            uint8_t winner = (bk!=bc) ? (bk>bc ? corner : h.c) : h.c;
            // Preserve the input palette: only indices already present.
            *pa=winner;*pb=winner;
        };
        corner_rule(h.nw,h.n,h.w,&o00,&o00);
        corner_rule(h.ne,h.n,h.e,&o10,&o10);
        corner_rule(h.sw,h.s,h.w,&o01,&o01);
        corner_rule(h.se,h.s,h.e,&o11,&o11);
        // Thin-line rule (paper Listing 4): a 1-pixel 45-degree diagonal
        // stays 4-connected: when both orthogonals differ from the centre but
        // the diagonal corner matches the centre, keep the corner-side pixel.
        auto thin = [&](uint8_t corner, uint8_t adj_a, uint8_t adj_b, uint8_t *p) {
            if (rgb_equal(corner,h.c) && !rgb_equal(adj_a,h.c) && !rgb_equal(adj_b,h.c)) *p=h.c;
        };
        thin(h.nw,h.n,h.w,&o00);thin(h.ne,h.n,h.e,&o10);
        thin(h.sw,h.s,h.w,&o01);thin(h.se,h.s,h.e,&o11);
        out.pixels[(size_t(y)*2*size_t(out.width))+size_t(x)*2]=o00;
        out.pixels[(size_t(y)*2*size_t(out.width))+size_t(x)*2+1]=o10;
        out.pixels[(size_t(y)*2*size_t(out.width))+size_t(out.width)+size_t(x)*2]=o01;
        out.pixels[(size_t(y)*2*size_t(out.width))+size_t(out.width)+size_t(x)*2+1]=o11;
    }
    return out;
}
Image8 mmpx4x(const Image8& src, const std::array<uint32_t,256>& luma, int transparent) {
    return mmpx2x(mmpx2x(src,luma,transparent),luma,transparent);
}
}
