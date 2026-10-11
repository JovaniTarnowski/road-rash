#include "building_panels.hpp"
#include "assets.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
int main(int argc,char** argv) {
    try {
        rr::CelTexture texture;
        auto material=[&](bool,int32_t,int32_t){return &texture;};
        rr::BuildingPanelInput in;
        in.near.shoulder={60*65536,180*65536};in.far.shoulder={110*65536,100*65536};
        in.near.height=50*65536;in.near.forward_height=20*65536;in.near.selector=0;
        in.near.roof_offset=-25*65536;in.near.forward_roof_offset=-10*65536;
        const auto nodes=rr::building_draw_nodes(in,material);
        if(nodes.size()!=2 || !nodes[0].flat_index || nodes[1].texture!=&texture)throw std::runtime_error("Building facade/roof order");
        std::ifstream file(argc>1?argv[1]:RR_SOURCE_DIR "/analysis/oracle/napa-building-fixtures.tsv");
        if(file) {
            std::map<std::string,int32_t> values;std::string line,key;int64_t value;
            while(std::getline(file,line)){std::istringstream row(line);if(row>>key>>value)values[key]=int32_t(value);}
            size_t count=0;
            for(int c=0;c<values.at("building.count");++c) {
                const std::string prefix="building."+std::to_string(c)+".";
                auto get=[&](const std::string& k){return values.at(prefix+k);};
                rr::BuildingPanelInput p;p.side=uint8_t(get("side"));p.logical_width=get("width");p.near.selector=uint8_t(get("selector"));p.far.type=uint8_t(get("far_type"));p.near.cut_roof=get("cut_roof");
                auto load=[&](rr::BuildingRoad& road,const std::string& name){road.shoulder={get(name+".0"),get(name+".1")};road.height=get(name+".2");road.forward_height=get(name+".3");road.roof_offset=get(name+".4");road.forward_roof_offset=get(name+".5");};
                load(p.near,"near");load(p.far,"far");auto result=rr::building_draw_nodes(p,material);
                if(result.size()!=size_t(get("count")))throw std::runtime_error(prefix+"count native="+std::to_string(result.size())+" original="+std::to_string(get("count")));
                for(size_t n=0;n<result.size();++n) {
                    auto root=std::to_string(n)+".";
                    for(int i=0;i<8;++i){const auto& point=result[n].view_points[i/2];if((i%2?point.y:point.x)!=get(root+"point."+std::to_string(i)))throw std::runtime_error(prefix+root+"point."+std::to_string(i));}
                    if(result[n].flip_u!=bool(get(root+"flip_u")) || result[n].flip_v!=bool(get(root+"flip_v")))throw std::runtime_error(prefix+root+"flip");
                    ++count;
                }
            }
            std::cout<<values.at("building.count")<<" original building cases, "<<count<<" nodes match\n";
            const auto depths=rr::read_depth_table(rr::Assets(RR_SOURCE_DIR "/../cdrom/ROADRASH").read("ROADRASH.EXE"));
            size_t shoulders=0;
            for(int c=0;c<values.at("building.count");++c) {
                const std::string prefix="building.shoulder."+std::to_string(c)+".";
                auto get=[&](const std::string& k){return values.at(prefix+k);};
                rr::BuildingShoulderInput p;p.top.side=uint8_t(get("side"));p.logical_width=get("width");p.near_depth=get("near_depth");p.top.far_depth=get("far_depth");p.top.occlusion_row=get("ceiling");p.near_base_y=get("near_base");p.far_base_y=get("far_base");p.near_world_base_y=get("near_world_base");p.far_world_base_y=get("far_world_base");
                for(int i=0;i<4;++i){p.top.world[i]={get("world."+std::to_string(i*2)),get("world."+std::to_string(i*2+1))};p.top.projected[i]={get("projected."+std::to_string(i*2)),get("projected."+std::to_string(i*2+1))};}
                auto result=rr::building_shoulder_nodes(p,{0,150},depths,[&](uint8_t,int32_t,int32_t){return &texture;});
                if(result.size()!=size_t(get("count")))throw std::runtime_error(prefix+"count native="+std::to_string(result.size())+" original="+std::to_string(get("count")));
                for(size_t n=0;n<result.size();++n) {
                    auto root=std::to_string(n)+".";
                    for(int i=0;i<8;++i){const auto& point=result[n].view_points[i/2];if((i%2?point.y:point.x)!=get(root+"point."+std::to_string(i)))throw std::runtime_error(prefix+root+"point."+std::to_string(i)+" native="+std::to_string(i%2?point.y:point.x)+" original="+std::to_string(get(root+"point."+std::to_string(i))));}
                    if(result[n].flip_u!=bool(get(root+"flip_u")) || result[n].flip_v!=bool(get(root+"flip_v")))throw std::runtime_error(prefix+root+"flip");
                    ++shoulders;
                }
            }
            std::cout<<"128 original building shoulder cases, "<<shoulders<<" nodes match\n";
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
