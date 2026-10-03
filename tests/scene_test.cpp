#include "scene.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
int main() {
    int failures=0;auto check=[&](bool ok,const char* msg){if(!ok){++failures;std::cerr<<msg<<'\n';}};
    rr::TerrainSlice hills;hills.type=4;
    for(int i=0;i<6;++i) hills.points[i]={i%2?500:-500,i*16};
    const auto projected=rr::project_hills(hills,0,0,{0,150},32768);
    check(projected.points[0][0]==-90*65536 && projected.points[1][0]==410*65536,"far hill x");
    check(projected.points[0][1]==165*65536 && projected.maximum_row==165*65536,"hill y and maximum row");
    hills.points[0][0]=-40000;hills.points[1][0]=40000;
    const auto near=rr::project_hills(hills,0,0,{0,150},65536);
    check(near.points[0][0]==-0x40000000 && near.points[1][0]==0x40000000,"near overflow sentinels");
    const auto path=rr::golden::oracle_dir()+"/raster-fixtures.tsv";
    if(std::filesystem::exists(path)) {
        try {
            const auto golden=rr::golden::load(path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.empty()) throw std::runtime_error("Wrong scene oracle");
            const auto& frame=golden.frames.front();
            for(int c=0;c<96;++c) {
                const std::string prefix="hill.project."+std::to_string(c)+".";
                auto field=[&](const std::string& key) {
                    const auto i=frame.find(prefix+key);
                    if(i==frame.end() || i->second<INT32_MIN || i->second>INT32_MAX) throw std::runtime_error("Missing/invalid hill field: "+prefix+key);
                    return int32_t(i->second);
                };
                for(int i=0;i<6;++i) hills.points[i]={field("input."+std::to_string(i*2)),field("input."+std::to_string(i*2+1))};
                const auto actual=rr::project_hills(hills,field("world_x"),field("world_y"),{field("camera_x"),field("camera_height")},field("scale"),field("width"));
                for(int i=0;i<12;++i) check(actual.points[i/2][i%2]==field("output."+std::to_string(i)),"original hill point");
                check(actual.maximum_row==field("maximum_row"),"original hill maximum row");
            }
            std::cout<<"96 original hill projections compared\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    std::cout<<failures<<" failures\n";return failures?1:0;
}
