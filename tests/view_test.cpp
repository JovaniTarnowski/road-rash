#include "view.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
    // C++20 guarantees these; the ported code relies on them (arithmetic shift of negatives, two's complement).
    static_assert((-8 >> 1) == -4 && (-1 >> 31) == -1, "arithmetic right shift");
    static_assert(int32_t(uint32_t(0x80000000u)) == INT32_MIN, "two's complement conversion");
    static_assert(uint32_t(int32_t(-1)) == 0xffffffffu, "modular conversion");
int main() {
    int failures=0;
    auto check=[&](bool ok,const char* what) { if (!ok) { ++failures;std::cerr<<what<<'\n'; } };
    const auto low=rr::view_mode(false),high=rr::view_mode(true),inset=rr::view_mode(false,true);
    check(low.surface_width==320 && low.surface_height==240 && low.coordinate_shift==16,"low dimensions");
    check(high.surface_width==640 && high.surface_height==480 && high.scale_shift==1,"high dimensions");
    check(inset.surface_width==320 && inset.display_width==640 && inset.origin_x==160 && inset.origin_y==120,"inset dimensions and origin");
    const int32_t input[]={0,65535,65536,65537,-1,-65535,-65536,-65537,32768,-32768,INT32_MAX,INT32_MIN};
    const int32_t expected[]={0,0,1,1,-1,-1,-1,-2,0,-1,32767,-32768};
    for (size_t i=0;i<12;++i) {
        check(low.map(input[i])==expected[i],"signed floor for low coordinates");
        check(high.map(input[i])==2*expected[i],"high coordinates snapped to even pixels");
    }
    // Scaled mapping (plan 19 Task 2): s=1 is the original high mode
    // exactly; s=2..4 multiply by s before shifting and keep sub-pixel
    // bits; out-of-range scales throw (no giant allocations from settings).
    {
        const rr::ViewMode one=rr::view_mode_scaled(1),high=rr::view_mode(true);
        check(one.surface_width==640 && one.surface_height==480,"scale 1 is the original high mode");
        for (int32_t q:{0,1,32767,32768,65535,65536,-1,-32768,-32769,-65536,0x7fffffff,int32_t(0x80000000)})
            check(one.map(q)==high.map(q),"scale 1 maps exactly like the original");
        const rr::ViewMode three=rr::view_mode_scaled(3);
        check(three.surface_width==1920 && three.surface_height==1440,"scale 3 surface");
        check(three.map(65536)==6 && three.map(32768)==3 && three.map(10923)==1 && three.map(-1)==-1,
            "scale 3 is floor(q16 * 6 / 65536) with sub-pixel bits kept");
        check(three.map(0x7fffffff)==int32_t((int64_t(0x7fffffff)*3)>>15),"no 32-bit overflow in the multiply");
        try { rr::view_mode_scaled(0);check(false,"scale 0 throws"); } catch (const std::runtime_error&) {}
        try { rr::view_mode_scaled(5);check(false,"scale 5 throws"); } catch (const std::runtime_error&) {}
        check(rr::view_center_q16(320)==160*65536,"4:3 center is 160");
        check(rr::view_center_q16(427)==427*65536/2,"16:9 logical width keeps half pixel");
        try { rr::view_center_q16(0);check(false,"zero width throws"); } catch (const std::runtime_error&) {}
    }
    const auto path=rr::golden::oracle_dir()+"/raster-fixtures.tsv";
    if (std::filesystem::exists(path)) {
        try {
            const auto golden=rr::golden::load(path);
            if (golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.empty()) throw std::runtime_error("Empty mapping capture");
            for (int mode=0;mode<2;++mode) for (int i=0;i<12;++i) {
                const std::string prefix="view.map."+std::to_string(mode)+"."+std::to_string(i)+".";
                const auto& frame=golden.frames.front();
                const auto source=frame.find(prefix+"input"),mapped=frame.find(prefix+"output");
                if (source==frame.end() || mapped==frame.end() || source->second<INT32_MIN || source->second>INT32_MAX)
                    throw std::runtime_error("Missing/invalid original coordinate mapping");
                check(rr::view_mode(mode!=0).map(int32_t(source->second))==mapped->second,"original coordinate mapping equality");
            }
        } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
    }
    std::cout<<failures<<" failures\n";return failures ? 1 : 0;
}
