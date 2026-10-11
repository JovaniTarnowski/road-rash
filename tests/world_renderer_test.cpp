#include "world_renderer.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
int main(int argc,char** argv) {
    // Synthetic checks run even when the owned-game oracle captures are absent.
    {
        rr::Framebuffer fb(320,240);fb.clear(125);
        rr::CelTexture texture;texture.bounds={0,0,7,7};texture.flat_index=99;
        rr::CelMip mip;mip.width=mip.height=8;mip.stride_shift=3;mip.pixels.assign(64,34);texture.mips.push_back(mip);
        rr::DrawNode node;node.texture=&texture;
        node.view_points={{{2*65536,3*65536},{10*65536,3*65536},{10*65536,11*65536},{2*65536,11*65536}}};
        rr::render_nodes(fb,{node,{}},rr::view_mode(false),16);
        if(fb.row(6)[6]!=34 || fb.row(0)[0]!=125){std::cerr<<"Synthetic textured node failed\n";return 1;}
        texture.flags=16;rr::render_nodes(fb,{node},rr::view_mode(false),16);
        if(fb.row(6)[6]!=99){std::cerr<<"Quality mask dispatch failed\n";return 1;}
        node.texture=nullptr;node.flat_index=17;rr::render_nodes(fb,{node},rr::view_mode(false),16);
        if(fb.row(6)[6]!=17){std::cerr<<"Flat override failed\n";return 1;}
        try{rr::render_nodes(fb,{},rr::view_mode(true),16);std::cerr<<"Mismatched framebuffer accepted\n";return 1;}catch(const std::runtime_error&){}
    }
    const std::string path=argc>1?argv[1]:rr::golden::oracle_dir()+"/raster-fixtures.tsv";
    if(!std::filesystem::exists(path)){std::cerr<<"SKIP missing nodes oracle\n";return 0;}
    size_t failures=0;
    try {
        const auto golden=rr::golden::load(path);if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.empty())throw std::runtime_error("Wrong node golden");
        const auto& fields=golden.frames.front();
        for(int c=0;c<(fields.count("nodes.32.high")?33:32);++c) {
            const std::string prefix="nodes."+std::to_string(c)+".";
            const auto mode=rr::view_mode(fields.at(prefix+"high")!=0);
            std::array<rr::CelTexture,12> textures;std::vector<rr::DrawNode> nodes(12);
            for(int n=0;n<12;++n) {
                const std::string key=prefix+std::to_string(n)+".";auto& node=nodes[n];auto& texture=textures[n];
                texture.bounds={0,0,31,31};texture.flags=uint32_t(fields.at(key+"flags"));texture.flat_index=uint8_t(fields.at(key+"index"));
                for(int level=0;level<3;++level){rr::CelMip mip;mip.width=mip.height=32>>level;mip.stride_shift=unsigned(5-level);mip.pixels.resize(size_t(1024)>>(level*2));for(size_t i=0;i<mip.pixels.size();++i)mip.pixels[i]=uint8_t(i*37+i/7+level*53);texture.mips.push_back(std::move(mip));}
                if(c==32) {
                    texture.bounds={0,0,255,31};texture.mips.clear();
                    for(int level=0;level<2;++level){rr::CelMip mip;mip.width=256>>level;mip.height=32>>level;mip.stride_shift=8-level;mip.pixels.resize(size_t(mip.width)*mip.height);mip.sampling_storage.resize(65536);for(size_t i=0;i<mip.sampling_storage.size();++i){const size_t at=i+(level?8192:0);mip.sampling_storage[i]=uint8_t(at*37+at/7);}std::copy_n(mip.sampling_storage.begin(),mip.pixels.size(),mip.pixels.begin());texture.mips.push_back(std::move(mip));}
                }
                if(fields.at(key+"flat")>=0)node.flat_index=uint8_t(fields.at(key+"flat"));else node.texture=&texture;
                node.flip_u=fields.at(key+"flip_u")!=0;node.flip_v=fields.at(key+"flip_v")!=0;
                for(int i=0;i<4;++i)node.view_points[i]={int32_t(fields.at(key+"point."+std::to_string(i*2))),int32_t(fields.at(key+"point."+std::to_string(i*2+1)))};
            }
            rr::Framebuffer fb(mode.surface_width,mode.surface_height);fb.clear(125);
            rr::render_nodes(fb,nodes,mode,uint32_t(fields.at(prefix+"mask")));
            const auto raw=std::filesystem::path(path).parent_path()/(golden.scenario+"-nodes"+std::to_string(c)+".raw");
            std::ifstream in(raw,std::ios::binary);if(!in)throw std::runtime_error("Missing original node pixels");
            const std::vector<uint8_t> expected((std::istreambuf_iterator<char>(in)),{});
            if(expected.size()!=size_t(fb.width())*fb.height())throw std::runtime_error("Wrong node image size");
            size_t pixels=0;for(int y=0;y<fb.height();++y)for(int x=0;x<fb.width();++x)if(fb.row(y)[x]!=expected[size_t(y)*fb.width()+x]){if(!pixels)std::cerr<<"case "<<c<<" first mismatch "<<x<<","<<y<<'\n';++pixels;}
            if(pixels){++failures;std::cerr<<"case "<<c<<": "<<pixels<<" mismatches\n";}
        }
        std::cout<<"Original display lists compared; "<<failures<<" failures\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    return failures?1:0;
}
