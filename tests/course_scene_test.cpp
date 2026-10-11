#include "course_scene.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
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
