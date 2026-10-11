#include "road_materials.hpp"
#include "exe_tables.hpp"
#include "resource.hpp"
#include <algorithm>
#include <stdexcept>
namespace rr {
RoadMaterials read_road_materials(const std::vector<uint8_t>& exe,const std::vector<uint8_t>& global) {
    const auto descriptors=read_exe_data(exe,0x0046dfb0,300);
    auto word=[&](size_t p){return uint32_t(descriptors[p])|uint32_t(descriptors[p+1])<<8|uint32_t(descriptors[p+2])<<16|uint32_t(descriptors[p+3])<<24;};
    const auto resources=read_resources(global);RoadMaterials result;std::array<bool,8> loaded{};bool ended=false;
    const std::array<uint32_t,8> destinations={0x4c3cc0,0x4c3cb0,0x4c3cd0,0x4c3cd4,0x4c3cd8,0x4c3ce0,0x4c3ce4,0x4c3ce8};
    for(size_t p=0;p+20<=descriptors.size();p+=20) {
        const auto destination=word(p);if(!destination){ended=true;break;}
        const std::array<uint32_t,3> ground_destinations={0x4c3cc4,0x4c3cb8,0x4c3cbc};
        const auto ground=std::find(ground_destinations.begin(),ground_destinations.end(),destination);
        if(ground!=ground_destinations.end()) {
            const auto resource=std::find_if(resources.begin(),resources.end(),[&](const Resource& item){return item.tag=="CEL " && item.id==word(p+8)+1;});
            if(resource==resources.end())throw std::runtime_error("Missing marked ground material");
            auto& cel=result.terrain[size_t(ground-ground_destinations.begin())];cel=read_cel({global.begin()+resource->offset,global.begin()+resource->offset+resource->size});
            if(!cel.texture)throw std::runtime_error("Marked ground lacks indexed texture");
        }
        const auto found=std::find(destinations.begin(),destinations.end(),destination);if(found==destinations.end())continue;
        const size_t slot=size_t(found-destinations.begin());
        if(loaded[slot] || word(p+4)!=0x43454c20 || word(p+8)==0xffffffffu)throw std::runtime_error("Invalid road material descriptor");
        const uint32_t id=word(p+8)+1;
        const auto resource=std::find_if(resources.begin(),resources.end(),[&](const Resource& item){return item.tag=="CEL " && item.id==id;});
        if(resource==resources.end())throw std::runtime_error("Missing road material CEL");
        result.entries[slot]=read_cel({global.begin()+resource->offset,global.begin()+resource->offset+resource->size});
        if(!result.entries[slot].texture)throw std::runtime_error("Road material lacks indexed texture");loaded[slot]=true;
    }
    if(!ended)throw std::runtime_error("Unterminated road material table");
    // 0x00444e30 aliases the middle marked lane to the first marked material.
    result.entries[3]=result.entries[2];loaded[3]=loaded[2];
    if(std::find(loaded.begin(),loaded.end(),false)!=loaded.end())throw std::runtime_error("Incomplete road material table");
    return result;
}
std::vector<DrawNode> road_draw_nodes(const std::vector<RoadPanel>& panels,const RoadMaterials& materials) {
    std::vector<DrawNode> nodes;nodes.reserve(panels.size());
    for(const auto& panel:panels) {
        const auto slot=size_t(panel.material);if(slot>=materials.entries.size() || !materials.entries[slot].texture)throw std::runtime_error("Unbound road material");
        DrawNode node;node.view_points=panel.points;node.texture=&*materials.entries[slot].texture;node.flip_u=panel.flip_u;node.flip_v=panel.flip_v;nodes.push_back(node);
    }
    return nodes;
}
}
