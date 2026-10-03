#include "raster.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
int main() {
    int failures=0;
    const auto path=rr::golden::oracle_dir()+"/raster-fixtures.tsv";
    if(!std::filesystem::exists(path)) {std::cerr<<"SKIP missing polygon oracle\n";return rr::golden::skip;}
    try {
        const auto golden=rr::golden::load(path);
        if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.empty())throw std::runtime_error("Wrong polygon oracle");
        const auto& frame=golden.frames.front();
        for(int c=0;c<2048;++c) {
            const std::string prefix="polygon."+std::to_string(c)+".";
            auto field=[&](const std::string& key){const auto it=frame.find(prefix+key);if(it==frame.end() || it->second<INT32_MIN || it->second>INT32_MAX)throw std::runtime_error("Missing polygon field: "+prefix+key);return int32_t(it->second);};
            std::array<rr::RasterPoint,4> points;
            for(int i=0;i<4;++i)points[i]={field("input."+std::to_string(i*2)),field("input."+std::to_string(i*2+1))};
            bool flip=c%2!=0;const bool accepted=rr::normalize_quad(points,uint32_t(field("flags")),&flip);
            auto check=[&](bool match,const std::string& what){if(!match){if(failures<12)std::cerr<<prefix+what<<'\n';++failures;}};
            check(accepted==(field("accepted")!=0),"accepted");check(flip==(field("flip")!=0),"flip");
            for(int i=0;i<4;++i){check(points[i].x==field("output."+std::to_string(i*2)),"x"+std::to_string(i));check(points[i].y==field("output."+std::to_string(i*2+1)),"y"+std::to_string(i));}
        }
        std::cout<<"2048 original polygon normalizations compared; "<<failures<<" failures\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    return failures?1:0;
}
