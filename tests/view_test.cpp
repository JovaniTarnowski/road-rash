#include "view.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
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
