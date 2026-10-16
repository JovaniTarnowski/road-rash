#include "course_scene.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <set>
#include <tuple>
#include <iostream>
int main(int argc,char** argv) {
    try {
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if(!assets.exists("DATA/COURSES/NAPA.CRS")){std::cout<<"No imported scene corpus\n";return 0;}
        const auto exe=assets.read("ROADRASH.EXE");const auto depths=rr::read_depth_table(exe);
        const auto course=rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
        rr::CourseScene scene(assets,exe,course,"NAPA");
        std::map<unsigned,size_t> building_flags;
        size_t samples=0,missing=0,hill_samples=0,nodes=0,views=0,objects=0,missing_objects=0;
        for(size_t section=0;section<course.sections.size();++section)for(size_t local=0;local<course.sections[section].samples.size();++local) {
            ++samples;const auto& terrain=scene.terrain(uint32_t(section),uint32_t(local));
            for(const auto& object:scene.objects(uint32_t(section),uint32_t(local))) {
                ++objects;const auto* frame=scene.object_frame(uint32_t(section),uint32_t(local),object);
                if(!frame || !frame->texture)++missing_objects;
            }
            if(terrain.type==0)++building_flags[course.sections[section].samples[local].marking_flags];
            if(terrain.type!=4)continue;++hill_samples;
            for(int side=0;side<2;++side)for(int band=0;band<3;++band)
                if(!scene.material(uint32_t(section),uint32_t(local),terrain.selectors[side],1,uint8_t(band),64*65536,16*65536)){++missing;if(band==0)std::cerr<<"Unresolved hill at "<<section<<':'<<local<<" side "<<side<<" selector "<<int(terrain.selectors[side])<<'\n';}
        }
        rr::Framebuffer sweep(640,480);
        for(bool second:{false,true}) {
            const auto route=rr::make_route(course,second);
            for(uint32_t p=0;p<route.size();p+=5) {
                const auto slices=rr::project_road(route,p*256+127,{0,150},depths);
                const auto draw=scene.draw_nodes(route,slices,{0,150},depths);nodes+=draw.size();++views;
                if(p%5==0) {
                    scene.draw_horizon(sweep,0);
                    try {for(const auto& node:draw) {
                        try {rr::render_nodes(sweep,{node},rr::view_mode(true),16);}
                        catch(const std::exception& e) {
                            if(node.texture)std::cerr<<node.texture->name<<" bounds "<<node.texture->bounds[0]<<','<<node.texture->bounds[1]<<','<<node.texture->bounds[2]<<','<<node.texture->bounds[3]<<" mip0 "<<node.texture->mips[0].width<<'x'<<node.texture->mips[0].height<<'\n';
                            for(const auto& v:node.view_points)std::cerr<<v.x<<','<<v.y<<' ';std::cerr<<" flips "<<node.flip_u<<','<<node.flip_v<<'\n';
                            throw;
                        }
                    }}
                    catch(const std::exception& e){throw std::runtime_error("Napa sample "+std::to_string(p)+" branch "+std::to_string(second)+": "+e.what());}
                }
            }
        }
        // Every course, every route sample, at the camera fraction that puts
        // slice 32 farthest (frac 0: depth 8448, building forward lookups
        // reach the table's zero padding). HIWAY/CANYN once threw here.
        for(const char* name:{"CITY","HIWAY","MEDLY","NAPA","CANYN"}) {
            const auto other=rr::read_course(assets.read(std::string("DATA/COURSES/")+name+".CRS"));
            const rr::CourseScene other_scene(assets,exe,other,name);
            size_t course_views=0;
            for(bool second:{false,true}) {
                const auto route=rr::make_route(other,second);
                for(uint32_t p=0;p+1<route.size();++p)for(const uint32_t frac:{0u,255u}) {
                    const auto slices=rr::project_road(route,p*256+frac,{0,150},depths);
                    try {
                        const auto draw=other_scene.draw_nodes(route,slices,{0,150},depths);
                        if(p%97==0)rr::render_nodes(sweep,draw,rr::view_mode(true),16);
                    } catch(const std::exception& e) {
                        throw std::runtime_error(std::string(name)+" sample "+std::to_string(p)+" frac "+std::to_string(frac)+" branch "+std::to_string(second)+": "+e.what());
                    }
                    ++course_views;
                }
            }
            std::cout<<name<<": "<<course_views<<" views built\n";
        }
        // Hor+ widescreen: road slices and every builder centre on the same
        // logical_width/2. Each node fully inside the 4:3 view must reappear
        // at 426 shifted by exactly 53 columns. Regression: panel inputs kept
        // their 320 default (sidewalk/hill strips tore) and nodes were shifted
        // a second time by the canvas margin (scenery stood in the road).
        for(const char* name:{"CITY","NAPA"}) {
            const auto other=rr::read_course(assets.read(std::string("DATA/COURSES/")+name+".CRS"));
            const rr::CourseScene other_scene(assets,exe,other,name);
            const auto route=rr::make_route(other,false);
            using Key=std::tuple<const rr::CelTexture*,int,bool,bool,std::array<int32_t,8>>;
            auto key=[](const rr::DrawNode& n,int32_t dx){std::array<int32_t,8> v{};for(size_t k=0;k<4;++k){v[2*k]=n.view_points[k].x+dx;v[2*k+1]=n.view_points[k].y;}
                return Key{n.texture,n.flat_index?int(*n.flat_index):-1,n.flip_u,n.flip_v,v};};
            size_t interior=0,unmatched=0;
            for(const rr::Camera camera:{rr::Camera{0,150},rr::Camera{-300,170}})for(uint32_t p=0;p+40<route.size();p+=37) {
                const auto narrow=other_scene.draw_nodes(route,rr::project_road(route,p*256+64,camera,depths),camera,depths,{},320);
                const auto wide=other_scene.draw_nodes(route,rr::project_road(route,p*256+64,camera,depths,426),camera,depths,{},426);
                std::multiset<Key> present;for(const auto& n:wide)present.insert(key(n,0));
                for(const auto& n:narrow) {
                    bool inside=true;for(const auto& v:n.view_points)inside&=v.x>=8*65536 && v.x<=312*65536;
                    if(!inside)continue;++interior;
                    const auto it=present.find(key(n,53*65536));
                    if(it==present.end())++unmatched;else present.erase(it);
                }
            }
            std::cout<<name<<" widescreen: "<<interior<<" interior nodes, "<<unmatched<<" not at +53\n";
            if(!interior || unmatched)throw std::runtime_error(std::string(name)+": widescreen nodes not centred with the road");
        }
        std::cout<<samples<<" terrain samples, "<<hill_samples<<" hill samples, "<<missing<<" unresolved hill materials, "<<views<<" views, "<<nodes<<" emitted nodes\n";
        for(const auto& entry:building_flags)std::cout<<"Building flags "<<entry.first<<": "<<entry.second<<" samples\n";
        std::cout<<objects<<" scenery objects, "<<missing_objects<<" unresolved scenery frames\n";
        if(missing_objects)throw std::runtime_error("Unresolved Napa scenery frame");
        if(missing)throw std::runtime_error("Unresolved Napa hill material");
        // Optional research preview, drawn entirely from native course state.
        if(argc>1) {
            const auto route=rr::make_route(course,false);
            const uint32_t position=argc>2?uint32_t(std::stoul(argv[2])):49*256;
            rr::Framebuffer fb(640,480);scene.draw_horizon(fb,0);
            const auto slices=rr::project_road(route,position,{0,150},depths);
            rr::render_nodes(fb,scene.draw_nodes(route,slices,{0,150},depths),rr::view_mode(true),16);
            std::vector<uint8_t> rgba(640*480*4);fb.to_rgba(rr::Palette::from_raw(assets.read("DATA/PALETTE.RAW")),rgba.data(),640*4);
            SDL_Surface* surface=SDL_CreateSurfaceFrom(640,480,SDL_PIXELFORMAT_RGBA32,rgba.data(),640*4);
            if(!surface || !SDL_SaveBMP(surface,argv[1]))throw std::runtime_error(SDL_GetError());SDL_DestroySurface(surface);
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
