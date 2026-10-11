#include "scenery.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
int main(int argc,char** argv){
 try {
    rr::CourseSection section;section.samples.resize(6);for(auto& path:section.samples){path.left=path.right=500;}
    section.roadside_profiles={0x014c0182}; // selector 0x4c, two objects, size 8, spacing 128
    const auto rows=rr::roadside_objects(section,[](){return 0u;});
    if(rows.size()!=4 || rows[0].lateral!=404 || rows[1].lateral!=532 || rows[2].sample!=1 || rows[0].selector!=0x4c)throw std::runtime_error("Roadside descriptor spacing/boundary");
    section.roadside_profiles={0x114c0182};const auto left=rr::roadside_objects(section,[](){return 0u;});
    if(left[0].lateral!=-404 || left[1].lateral!=-532)throw std::runtime_error("Left roadside spacing");
    section.scenery_events={{{0x01208311,0xfff}},{{0x01208411,0x25}}};
    const auto objects=rr::placed_scenery(section,0,[](){return 0u;});
    if(objects.size()!=2 || objects[0].sample!=1 || objects[0].lateral!=-1 || objects[1].sample!=2 || objects[1].lateral!=37 || objects[0].selector!=0x83 || !objects[0].static_family)throw std::runtime_error("Static event placement");
    rr::AnimationFrame frame;auto texture=std::make_shared<rr::CelTexture>();texture->bounds={0,0,48,17};frame.texture=texture;frame.hotspot={24,17};
    rr::DepthTable table;table.scale.assign(4352,32768);rr::SceneryObject object;object.lateral=200;object.size=8;
    const auto node=rr::scenery_node(frame,object,0,0,0,512,{0,150},table);
    if(node.view_points[0].x!=236*65536 || node.view_points[0].y!=148*65536 || node.view_points[2].x!=284*65536 || node.view_points[2].y!=166*65536)throw std::runtime_error("Billboard projection");
    object.mirror=true;const auto mirror=rr::scenery_node(frame,object,0,0,0,512,{0,150},table);
    if(!mirror.flip_u || mirror.view_points[0].x!=284*65536 || mirror.view_points[1].x!=236*65536)throw std::runtime_error("Billboard mirror");
    const std::string path=argc>1?argv[1]:rr::golden::oracle_dir()+"/napa-scenery-fixtures.tsv";
    if(std::filesystem::exists(path)) {
        const auto capture=rr::golden::load(path);const auto& fields=capture.frames.at(0);
        if(capture.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2")throw std::runtime_error("Wrong scenery oracle");
        for(int c=0;c<32;++c) {
            const std::string prefix="scenery."+std::to_string(c)+".";
            rr::CourseSection course;course.samples.resize(32);
            for(auto& sample:course.samples){sample.left=int32_t(-fields.at(prefix+"left"));sample.right=int32_t(fields.at(prefix+"right"));}
            for(int i=0;i<4;++i)course.roadside_profiles.push_back(uint32_t(fields.at(prefix+"profile."+std::to_string(i))));
            const auto actual=rr::roadside_objects(course,[&](){return uint32_t(fields.at(prefix+"random"));});
            for(int j=0;j<32;++j) {
                const std::string row=prefix+"sample."+std::to_string(j)+".";
                size_t count=0;
                for(const auto& object:actual)if(object.sample==uint32_t(j)) {
                    if(object.lateral!=fields.at(row+"x."+std::to_string(count)) || object.selector!=fields.at(row+"selector") || object.size!=fields.at(row+"size") || object.mirror!=bool(fields.at(row+"mirror")))throw std::runtime_error("Original roadside mismatch at "+row+std::to_string(count));
                    ++count;
                }
                if(count!=size_t(fields.at(row+"count")))throw std::runtime_error("Original roadside count at "+row);
            }
        }
        std::cout<<"1,024 original roadside samples match\n";
    }
    std::cout<<"Scenery descriptor, placement and billboard checks pass\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
