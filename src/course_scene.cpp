#include "course_scene.hpp"
#include "exe_tables.hpp"
#include "terrain_ground.hpp"
#include <algorithm>
#include <cstring>
#include <set>

namespace rr {
namespace {
int32_t bits(uint32_t v){int32_t r;std::memcpy(&r,&v,4);return r;}
int32_t wrap(int64_t v){return bits(uint32_t(v));}
}
CourseScene::CourseScene(const Assets& assets,const std::vector<uint8_t>& exe,
    const Course& course,const std::string& name,uint32_t seed,int level)
    :roads_(read_road_materials(exe,assets.read("DATA/GLOBAL.RSC"))),
     logarithms_(read_exe_data(exe,0x00476af8,256)),
     horizon_(assets.read("IMAGES/HORIZONS/"+name+".BOB")) {
    if(horizon_.size()!=720*320)throw std::runtime_error("Invalid course horizon");
    const auto joins=read_exe_data(exe,0x004688e0,128);
    for(int i=0;i<16;++i) {auto word=[&](int p){uint32_t v=0;for(int b=0;b<4;++b)v|=uint32_t(joins[p+b])<<(b*8);return bits(v);};joins_[i]={word(i*8),word(i*8+4)};}
    std::set<uint32_t> needed;
    for(const auto& section:course.sections) {
        terrain_.emplace_back();family_slots_.emplace_back();
        // The original terrain helpers consume CRT rand values. This isolated
        // deterministic stream has the same recurrence; full race random-call
        // interleaving is still a separate integration requirement.
        auto random=[&seed](){seed=seed*214013u+2531011u;return (seed>>16)&0x7fffu;};
        TerrainIterator iterator(section.terrain,0,random);
        std::array<uint16_t,4> slots{};size_t event=0;
        for(size_t j=0;j<section.samples.size();++j) {
            if(j)iterator.step(256);
            const auto& path=section.samples[j];
            auto t=iterator.slice(-path.left-path.shoulder_left,path.right+path.shoulder_right);
            if(t.type==4 && path.marking_flags && !(path.marking_flags&0x40))for(auto& point:t.points)point[1]=0;
            terrain_.back().push_back(t);
            // Resolve residency at the projection window's forward cursor.
            const int64_t cursor=std::max<int64_t>(0,int64_t(j)+view_slices+section.family_forward_bias);
            while(event<section.family_events.size() && section.family_events[event].sample<=cursor) {
                const auto& e=section.family_events[event++];
                if(e.type==1 || e.type==3)slots[e.slot]=e.family;
                // Keep data resident across unload/preload requests. Original
                // selectors stop referring to that slot until its replacement.
                needed.insert(e.family);
            }
            family_slots_.back().push_back(slots);
        }
        for(const auto& e:section.family_events)needed.insert(e.family);
        objects_.emplace_back(section.samples.size());
        for(const auto& object:roadside_objects(section,random))objects_.back().at(object.sample).push_back(object);
        for(const auto& object:placed_scenery(section,level,random))objects_.back().at(object.sample).push_back(object);
    }
    const auto file=assets.read("DATA/FAMILIES.RSC");
    for(const auto& entry:read_resources(file))if(needed.count(entry.id) && entry.size>24)
        families_.emplace(entry.id,read_family(std::vector<uint8_t>(file.begin()+entry.offset,file.begin()+entry.offset+entry.size)));
    for(size_t section=0;section<terrain_.size();++section)for(size_t local=0;local<terrain_[section].size();++local) {
        auto& t=terrain_[section][local];if(t.type!=0)continue;
        for(int side=0;side<2;++side) {
            const auto selector=t.selectors[side];if(selector==255)continue;
            const auto family=families_.find(family_slots_[section][local][selector>>6]);
            if(family==families_.end())throw std::runtime_error("Missing building family");
            const auto offset=family->second.resolve(1,selector&63);
            if(!offset || !family->second.groups.count(*offset))throw std::runtime_error("Missing building group");
            const auto& group=family->second.groups.at(*offset);
            const int height=group.nominal_height*16;
            if(height<17){t.selectors[side]=255;continue;}
            t.building_height[side]=height;
            const auto& cell=group.select(0x08000000,0x08000000,logarithms_);
            t.building_cut_roof[side]=!cell.texture.mips.empty() && !cell.texture.mips[0].pixels.empty() && cell.texture.mips[0].pixels[0]==0;
        }
    }

}
const TerrainSlice& CourseScene::terrain(uint32_t section,uint32_t local) const {return terrain_.at(section).at(local);}
const std::vector<SceneryObject>& CourseScene::objects(uint32_t section,uint32_t local) const {return objects_.at(section).at(local);}
const AnimationFrame* CourseScene::object_frame(uint32_t section,uint32_t local,const SceneryObject& object) const {
    const uint16_t id=family_slots_.at(section).at(local)[object.selector>>6];
    const auto family=families_.find(id);if(family==families_.end())return nullptr;
    const auto offset=family->second.resolve(object.static_family?4:0,object.selector&63,object.static_family?std::optional<uint32_t>(0):std::nullopt);
    if(!offset)return nullptr;
    const auto node=family->second.nodes.find(*offset);if(node==family->second.nodes.end() || node->second.tag!=0x5252414e)return nullptr;
    const auto key=std::make_pair(uint32_t(id),*offset);
    auto animation=animations_.find(key);
    if(animation==animations_.end())animation=animations_.emplace(key,read_animation(family->second.bytes,*offset)).first;
    if(animation->second.frames.size()<3)return nullptr;
    return &animation->second.frames[2];
}
const CelTexture* CourseScene::material(uint32_t section,uint32_t local,uint8_t selector,
    uint8_t category,std::optional<uint32_t> frame,int32_t width,int32_t height) const {
    const uint16_t id=family_slots_.at(section).at(local)[selector>>6];
    const auto family=families_.find(id);if(family==families_.end())return nullptr;
    const auto offset=family->second.resolve(category,selector&63,frame);if(!offset)return nullptr;
    const auto group=family->second.groups.find(*offset);if(group==family->second.groups.end())return nullptr;
    return &group->second.select(width,height,logarithms_).texture;
}
void CourseScene::draw_horizon(Framebuffer& fb,int32_t scroll) const {
    if(fb.width()!=640 || fb.height()!=480)throw std::runtime_error("Course horizon needs high-resolution surface");
    const int32_t start=int32_t(((-int64_t(scroll)%720)+720)%720);
    fb.clear(0);
    for(int y=0;y<320;++y)for(int x=0;x<fb.width();++x)fb.row(y)[x]=horizon_[size_t(y)*720+(x+start)%720];
}
std::vector<DrawNode> CourseScene::draw_nodes(const std::vector<RouteSample>& route,
    const std::vector<RoadSlice>& slices,const Camera& camera,const DepthTable& depths,ActorEmitter actors) const {
    struct Road {const PathSample* path;const TerrainSlice* terrain;uint32_t section,local;HillRoad hill[2];BuildingRoad building[2];int32_t ceiling;};
    std::vector<std::array<Road,2>> roads(slices.size());
    int32_t ceiling=240*65536;
    for(size_t n=0;n<slices.size();++n) {
        const auto& slice=slices[n];const auto& sample=route.at(slice.sample);
        for(int r=0;r<slice.count;++r) {
            auto& road=roads[n][r];const bool own=slice.count==1 || r==slice.own;
            road.section=own?sample.section:sample.other_section;road.local=own?sample.local:sample.other_local;
            road.path=own?&sample.path:&sample.other;road.terrain=&terrain(road.section,road.local);
            const auto& s=slice.road[r];const int32_t world_x=s.road_x+camera.x;
            int32_t max_row=s.screen_y;
            HillProjection hills;
            if(road.terrain->type==4){hills=project_hills(*road.terrain,world_x,s.world_y,camera,slice.scale);max_row=hills.maximum_row;}
            for(int side=0;side<2;++side) {
                auto& h=road.hill[side];h.type=road.terrain->type;h.depth=slice.depth;
                const int32_t shoulder=side?s.shoulder_right:s.shoulder_left;
                const int32_t shoulder_y=wrap(int64_t(camera.height-s.world_y-(side?road.path->height_right:road.path->height_left))*slice.scale+90*65536);
                h.world[0]={world_x+shoulder,s.world_y};h.projected[0]={wrap(slice.screen_x(r,shoulder)),shoulder_y};
                if(road.terrain->type==0) {
                    auto& building=road.building[side];building.shoulder=h.projected[0];building.type=0;
                    building.selector=road.terrain->selectors[side];building.marking_flags=road.path->marking_flags;
                    const int32_t shoulder_width=side?road.path->shoulder_right:road.path->shoulder_left;
                    const int32_t ns=(building.marking_flags&16)?depths.scale.at(size_t((slice.depth+shoulder_width)&~1)/2):slice.scale;
                    const int32_t fd=slice.depth+256-((building.marking_flags&8)?shoulder_width:0);
                    building.height=wrap(int64_t(road.terrain->building_height[side])*ns);
                    building.forward_height=wrap(int64_t(road.terrain->building_height[side])*depths.scale.at(size_t(fd&~1)/2));
                    building.roof_offset=(side?1:-1)*(building.height/2);
                    building.forward_roof_offset=(side?1:-1)*(building.forward_height/2);
                    building.cut_roof=road.terrain->building_cut_roof[side];
                } else {road.building[side].type=road.terrain->type;road.building[side].shoulder=h.projected[0];}
                for(int band=0;band<3;++band) {
                    const auto& point=road.terrain->points[band*2+side];
                    h.world[band+1]={world_x+point[0],s.world_y+point[1]};h.projected[band+1]={hills.points[band*2+side][0],hills.points[band*2+side][1]};
                }
            }
            road.ceiling=ceiling;ceiling=std::min(ceiling,max_row);
        }
    }
    for(size_t n=0;n+1<slices.size();++n)for(int r=0;r<std::min(slices[n].count,slices[n+1].count);++r) {
        auto& near=roads[n][r];const auto& far=roads[n+1][r];
        if(near.terrain->type!=0 || !(near.path->marking_flags&24))continue;
        for(int side=0;side<2;++side) {
            const int32_t width=side?near.path->shoulder_right:near.path->shoulder_left;
            const int32_t fraction=(near.path->marking_flags&8)?256-width:width;
            const int32_t depth=slices[n].depth+fraction;
            if(depth<0 || size_t((depth&~1)/2)>=depths.scale.size())throw std::runtime_error("Building transition depth out of range");
            const int32_t scale=depths.scale[size_t((depth&~1)/2)];
            const int32_t nx=near.hill[side].world[0].x,fx=far.hill[side].world[0].x;
            const int32_t ny=near.hill[side].world[0].y+(side?near.path->height_right:near.path->height_left),fy=far.hill[side].world[0].y+(side?far.path->height_right:far.path->height_left);
            const int32_t x=nx+(wrap(int64_t(fx-nx)*fraction)>>8);
            const int32_t y=ny+(wrap(int64_t(fy-ny)*fraction)>>8);
            near.building[side].transition={wrap(int64_t(x-camera.x)*scale+160*65536),wrap(int64_t(camera.height-y)*scale+90*65536)};
        }
    }
    std::vector<DrawNode> result;
    auto append=[&](const std::vector<DrawNode>& nodes){result.insert(result.end(),nodes.begin(),nodes.end());};
    for(size_t n=slices.size();n-->1;) {
        const auto& near=slices[n-1];const auto& far=slices[n];const int count=std::min(near.count,far.count);
        for(int i=0;i<count;++i) {
            const int r=count==2?(i==0?1-near.own:near.own):0;
            const auto& a=far.road[r];const auto& b=near.road[r];const auto& road=roads[n-1][r];const auto& old=roads[n][r];
            const bool visible=b.screen_y>=a.screen_y && road.ceiling>=a.screen_y;
            const auto& path=*road.path;
            const int32_t whole_left=-((path.left_q8+255)/256)*250;
            const int32_t far_left=-((old.path->left_q8+255)/256)*250;
            RoadPanelInput panels;
            panels.first_left=wrap(far.screen_x(r,far_left));panels.first_y=a.screen_y;panels.first_lane_width=wrap(int64_t(250)*far.scale);
            panels.second_left=wrap(near.screen_x(r,whole_left));panels.second_y=b.screen_y;panels.second_lane_width=wrap(int64_t(250)*near.scale);
            panels.left_lanes_q8=path.left_q8;panels.right_lanes_q8=path.right_q8;panels.left_lane_count=uint8_t((path.left_q8+255)/256);panels.right_lane_count=uint8_t((path.right_q8+255)/256);
            panels.marking_flags=path.marking_flags;panels.terrain_type=road.terrain->type;panels.next_terrain_type=old.terrain->type;panels.visible=visible;
            append(road_draw_nodes(road_panels(panels),roads_));
            if(path.marking_flags&32)for(int side=0;side<2;++side) {
                MarkedGroundInput ground{{wrap(near.screen_x(r,side?b.right:b.left)),b.screen_y},{wrap(far.screen_x(r,side?a.right:a.left)),a.screen_y},near.scale,far.scale,320,uint8_t(side),path.marking_flags,road.terrain->type==0?8:16};
                append(marked_ground_nodes(ground,[&](uint8_t index){return index==3?&*roads_.entries[0].texture:&*roads_.terrain.at(index).texture;}));
            }
            if(!(path.marking_flags&32) && road.terrain->type==4)for(int side=0;side<2;++side) {
                if(visible) {
                    ShoulderPanelInput shoulder;shoulder.near_depth=near.depth;shoulder.has_lanes=panels.left_lane_count+panels.right_lane_count!=0;shoulder.strip.side=uint8_t(side);shoulder.strip.far_depth=far.depth;
                    shoulder.strip.world={{{a.road_x+camera.x+(side?a.right:a.left),a.world_y},old.hill[side].world[0],road.hill[side].world[0],{b.road_x+camera.x+(side?b.right:b.left),b.world_y}}};
                    shoulder.strip.projected={{{wrap(far.screen_x(r,side?a.right:a.left)),old.hill[side].projected[0].y},old.hill[side].projected[0],road.hill[side].projected[0],{wrap(near.screen_x(r,side?b.right:b.left)),road.hill[side].projected[0].y}}};
                    append(shoulder_draw_nodes(shoulder,camera,depths,[&](int32_t w,int32_t h){return material(road.section,road.local,path.side_flags[side],5,0,w,h);}));
                }
                HillPanelInput hills;hills.near=road.hill[side];hills.far=old.hill[side];hills.side=uint8_t(side);hills.occlusion_row=road.ceiling;hills.overlap=true;hills.join_offsets=joins_;
                append(hill_draw_nodes(hills,camera,depths,[&](uint8_t band,int32_t w,int32_t h){return material(road.section,road.local,road.terrain->selectors[side],1,band,w,h);}));
            }
            if(!(path.marking_flags&32) && road.terrain->type==0)for(int side=0;side<2;++side) {
                BuildingShoulderInput shoulder;shoulder.near_depth=near.depth;shoulder.far_base_y=a.screen_y;shoulder.near_base_y=b.screen_y;shoulder.far_world_base_y=a.world_y;shoulder.near_world_base_y=b.world_y;
                shoulder.top.side=uint8_t(side);shoulder.top.far_depth=far.depth;shoulder.top.occlusion_row=road.ceiling;
                const int32_t fy=side?old.path->height_right:old.path->height_left,ny=side?path.height_right:path.height_left;
                shoulder.top.projected={{{wrap(far.screen_x(r,side?a.right:a.left)),old.hill[side].projected[0].y},old.hill[side].projected[0],road.hill[side].projected[0],{wrap(near.screen_x(r,side?b.right:b.left)),road.hill[side].projected[0].y}}};
                shoulder.top.world={{{a.road_x+camera.x+(side?a.right:a.left),a.world_y+fy},{old.hill[side].world[0].x,a.world_y+fy},{road.hill[side].world[0].x,b.world_y+ny},{b.road_x+camera.x+(side?b.right:b.left),b.world_y+ny}}};
                append(building_shoulder_nodes(shoulder,camera,depths,[&](uint8_t frame,int32_t w,int32_t h){return material(road.section,road.local,path.side_flags[side],5,frame,w,h);}));
                BuildingPanelInput building{road.building[side],old.building[side],uint8_t(side)};
                append(building_draw_nodes(building,[&](bool use_far,int32_t w,int32_t h){const auto& source=use_far?old:road;return material(source.section,source.local,source.terrain->selectors[side],1,std::nullopt,w,h);}));
            }
            if(actors)append(actors(near,far,r,road.ceiling,true));
            if(near.depth>146)for(const auto& object:objects(road.section,road.local)) {
                const auto* frame=object_frame(road.section,road.local,object);if(!frame || !frame->texture)continue;
                int32_t ground=terrain_height(*road.terrain,path,*old.terrain,*old.path,object.lateral*256,128,a.road_x-b.road_x);
                if(ground>998)ground=0;
                auto draw=scenery_node(*frame,object,(a.road_x+b.road_x)/2+camera.x,(a.world_y+b.world_y)/2,ground,near.depth,camera,depths);
                if(draw.view_points[0].y<road.ceiling)result.push_back(draw);
            }
            if(actors)append(actors(near,far,r,road.ceiling,false));
        }
    }
    return result;
}
}
