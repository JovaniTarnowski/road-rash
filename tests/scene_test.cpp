#include "scene.hpp"
#include "golden.hpp"
#include "assets.hpp"
#include "terrain_panels.hpp"
#include "exe_tables.hpp"
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
    rr::DepthTable synthetic;synthetic.scale.assign(4352,65536);
    rr::HillStrip strip{{{{100,0},{200,0},{200,0},{100,0}}},
        {{{100*65536,90*65536},{200*65536,90*65536},{200*65536,100*65536},{100*65536,100*65536}}},1024,400*65536,1,false};
    check(rr::subdivide_hill(strip,{0,150},synthetic).size()==2,"far hill splits twice");
    strip.far_depth=465;check(rr::subdivide_hill(strip,{0,150},synthetic).size()==7,"near hill clips last segment below minimum depth");
    strip.far_depth=210;check(rr::subdivide_hill(strip,{0,150},synthetic).empty(),"reject too-near hill strip");
    strip.far_depth=1024;strip.occlusion_row=80*65536;
    check(rr::subdivide_hill(strip,{0,150},synthetic).empty(),"reject occluded hill strip");
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
    const auto strip_path=rr::golden::oracle_dir()+"/napa-hill-strips.tsv";
    if(std::filesystem::exists(strip_path)) {
        try {
            const auto golden=rr::golden::load(strip_path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.size()!=1)throw std::runtime_error("Wrong hill strip oracle");
            const auto& fields=golden.frames[0];
            rr::Assets assets((std::filesystem::path(RR_SOURCE_DIR).parent_path()/"cdrom/ROADRASH").string());
            const auto table=rr::read_depth_table(assets.read("ROADRASH.EXE"));
            for(int c=0;c<128;++c) {
                const std::string prefix="hill.strip."+std::to_string(c)+".";
                auto field=[&](const std::string& key) {const auto i=fields.find(prefix+key);if(i==fields.end() || i->second<INT32_MIN || i->second>INT32_MAX)throw std::runtime_error("Missing hill strip field: "+prefix+key);return int32_t(i->second);};
                rr::HillStrip input;input.side=uint8_t(field("input.0"));input.far_depth=field("input.1");input.occlusion_row=field("input.2");input.adjust_seam=field("seam")!=0;
                for(int i=0;i<4;++i){input.world[i]={field("input."+std::to_string(3+i*2)),field("input."+std::to_string(4+i*2))};input.projected[i]={field("projected."+std::to_string(i*2)),field("projected."+std::to_string(i*2+1))};}
                const auto actual=rr::subdivide_hill(input,{field("camera_x"),field("camera_height")},table,field("width"));
                check(int(actual.size())==field("count"),"original hill subdivision count");
                for(size_t n=0;n<actual.size() && int(n)<field("count");++n)for(int i=0;i<8;++i)
                    check((i%2?actual[n][i/2].y:actual[n][i/2].x)==field(std::to_string(n)+".point."+std::to_string(i)),"original hill subdivision point");
            }
            std::cout<<"128 original hill strip subdivisions compared\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    const auto panels_path=rr::golden::oracle_dir()+"/napa-hill-panels.tsv";
    if(std::filesystem::exists(panels_path)) {
        try {
            const auto golden=rr::golden::load(panels_path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.size()!=1)throw std::runtime_error("Wrong hill panel oracle");
            const auto& fields=golden.frames[0];
            rr::Assets assets((std::filesystem::path(RR_SOURCE_DIR).parent_path()/"cdrom/ROADRASH").string());
            const auto exe=assets.read("ROADRASH.EXE"),joins=rr::read_exe_data(exe,0x004688e0,128);
            const auto table=rr::read_depth_table(exe);
            rr::CelTexture texture;size_t total=0;
            for(int c=0;c<256;++c) {
                const std::string prefix="hill.panels."+std::to_string(c)+".";
                auto field=[&](const std::string& key) {const auto i=fields.find(prefix+key);if(i==fields.end() || i->second<INT32_MIN || i->second>INT32_MAX)throw std::runtime_error("Missing hill panel field: "+prefix+key);return int32_t(i->second);};
                rr::HillPanelInput input;input.side=uint8_t(field("side"));input.near.depth=field("near_depth");input.far.depth=field("far_depth");input.far.type=uint8_t(field("far_type"));input.occlusion_row=field("occlusion");input.logical_width=field("width");input.overlap=field("overlap")!=0;
                for(int i=0;i<16;++i) {auto word=[&](int p){uint32_t v=0;for(int b=0;b<4;++b)v|=uint32_t(joins[p+b])<<(b*8);return int32_t(v);};input.join_offsets[i]={word(i*8),word(i*8+4)};}
                for(int i=0;i<4;++i){auto point=[&](const std::string& kind){return rr::RasterPoint{field(kind+std::to_string(i*2)),field(kind+std::to_string(i*2+1))};};input.near.projected[i]=point("near.projected.");input.far.projected[i]=point("far.projected.");input.near.world[i]=point("near.world.");input.far.world[i]=point("far.world.");}
                const auto actual=rr::hill_draw_nodes(input,{field("camera_x"),field("camera_height")},table,[&](uint8_t,int32_t,int32_t){return &texture;});
                if(int(actual.size())!=field("count")){check(false,"original hill panel count");std::cerr<<"case "<<c<<": native "<<actual.size()<<" original "<<field("count")<<'\n';}
                total+=actual.size();
                for(size_t n=0;n<actual.size() && int(n)<field("count");++n) {
                    check(actual[n].flip_u==bool(field(std::to_string(n)+".flip_u")) && actual[n].flip_v==bool(field(std::to_string(n)+".flip_v")),"original hill panel flips");
                    for(int i=0;i<8;++i)if((i%2?actual[n].view_points[i/2].y:actual[n].view_points[i/2].x)!=field(std::to_string(n)+".point."+std::to_string(i))){check(false,"original hill panel point");std::cerr<<"case "<<c<<" node "<<n<<" component "<<i<<'\n';}
                }
            }
            std::cout<<"256 original hill panel cases compared, "<<total<<" nodes\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    const auto shoulder_path=rr::golden::oracle_dir()+"/napa-shoulders.tsv";
    if(std::filesystem::exists(shoulder_path)) {
        try {
            const auto golden=rr::golden::load(shoulder_path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.size()!=1)throw std::runtime_error("Wrong shoulder oracle");
            const auto& fields=golden.frames[0];
            rr::Assets assets((std::filesystem::path(RR_SOURCE_DIR).parent_path()/"cdrom/ROADRASH").string());
            const auto table=rr::read_depth_table(assets.read("ROADRASH.EXE"));
            rr::CelTexture texture;size_t total=0;
            for(int c=0;c<256;++c) {
                const std::string prefix="shoulder."+std::to_string(c)+".";
                auto field=[&](const std::string& key) {const auto i=fields.find(prefix+key);if(i==fields.end() || i->second<INT32_MIN || i->second>INT32_MAX)throw std::runtime_error("Missing shoulder field: "+prefix+key);return int32_t(i->second);};
                rr::ShoulderPanelInput input;input.strip.side=uint8_t(field("side"));input.strip.far_depth=field("far_depth");input.near_depth=field("near_depth");input.logical_width=field("width");input.logical_height=field("height");input.has_lanes=field("lanes")!=0;
                for(int i=0;i<4;++i){input.strip.world[i]={field("world."+std::to_string(i*2)),field("world."+std::to_string(i*2+1))};input.strip.projected[i]={field("projected."+std::to_string(i*2)),field("projected."+std::to_string(i*2+1))};}
                const auto actual=rr::shoulder_draw_nodes(input,{field("camera_x"),field("camera_height")},table,[&](int32_t,int32_t){return &texture;});
                if(int(actual.size())!=field("count")){check(false,"original shoulder count");std::cerr<<"case "<<c<<": native "<<actual.size()<<" original "<<field("count")<<'\n';}
                total+=actual.size();
                for(size_t n=0;n<actual.size() && int(n)<field("count");++n) {
                    check(actual[n].flip_u==bool(field(std::to_string(n)+".flip_u")) && actual[n].flip_v==bool(field(std::to_string(n)+".flip_v")),"original shoulder flips");
                    for(int i=0;i<8;++i)if((i%2?actual[n].view_points[i/2].y:actual[n].view_points[i/2].x)!=field(std::to_string(n)+".point."+std::to_string(i))){check(false,"original shoulder point");std::cerr<<"case "<<c<<" node "<<n<<" component "<<i<<'\n';}
                }
            }
            std::cout<<"256 original shoulder cases compared, "<<total<<" nodes\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    std::cout<<failures<<" failures\n";return failures?1:0;
}
