#include "terrain_ground.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
int main(int argc,char** argv) {
    try {
        rr::TerrainSlice t;t.type=0;t.selectors={0,255};rr::PathSample p;p.left=p.right=250;p.shoulder_left=p.shoulder_right=100;p.height_left=16;p.height_right=8;
        if(rr::terrain_height(t,p,t,p,-400*256,0)!=999 || rr::terrain_height(t,p,t,p,400*256,0)!=8)throw std::runtime_error("Building ground selector boundaries");
        p.marking_flags=32;if(rr::terrain_height(t,p,t,p,-400*256,0)!=0)throw std::runtime_error("Marked ground");
        std::ifstream file(argc>1?argv[1]:RR_SOURCE_DIR "/analysis/oracle/napa-building-fixtures.tsv");
        if(file) {
            std::map<std::string,int32_t> values;std::string line,key;int64_t v;
            while(std::getline(file,line)){std::istringstream row(line);if(row>>key>>v)values[key]=int32_t(v);}
            size_t samples=0;
            for(int c=0;c<values.at("ground.count");++c) {
                const auto prefix="ground."+std::to_string(c)+".";auto get=[&](const std::string& k){return values.at(prefix+k);};
                rr::TerrainSlice a,b;rr::PathSample p,q;
                a.type=uint8_t(get("type"));p.marking_flags=uint8_t(get("flags"));a.selectors={uint8_t(get("selector_left")),uint8_t(get("selector_right"))};
                auto load=[&](rr::TerrainSlice& t,rr::PathSample& path,const std::string& name){path.left=-get(name+".0");path.right=get(name+".1");path.shoulder_left=-get(name+".2")-path.left;path.shoulder_right=get(name+".3")-path.right;path.height_left=get(name+".4");path.height_right=get(name+".5");for(int i=0;i<6;++i)t.points[i]={get(name+"."+std::to_string(6+i*2)),get(name+"."+std::to_string(7+i*2))};};
                load(a,p,"near");load(b,q,"far");
                for(int i=0;i<64;++i) {
                    const auto root=std::to_string(i)+".";const int32_t got=rr::terrain_height(a,p,b,q,get(root+"x"),uint8_t(get(root+"fraction")));
                    if(got!=get(root+"height"))throw std::runtime_error(prefix+root+"height native="+std::to_string(got)+" original="+std::to_string(get(root+"height")));
                    ++samples;
                }
            }
            std::cout<<samples<<" original terrain ground samples match\n";
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
