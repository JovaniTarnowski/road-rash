#include "actor_descriptors.hpp"
#include "actor_render.hpp"
#include "assets.hpp"
#include "exe_tables.hpp"
#include "resource.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
int main(int argc,char** argv) {
    try {
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if(!assets.exists("DATA/GLOBAL.RSC")){std::cout<<"Imported descriptor corpus absent\n";return 0;}
        const auto exe=assets.read("ROADRASH.EXE"),global=assets.read("DATA/GLOBAL.RSC");std::vector<std::vector<uint8_t>> cans(2);
        for(const auto& item:rr::read_resources(global))if(item.tag=="CANS" && item.id<=2 && item.id)cans[item.id-1]={global.begin()+item.offset,global.begin()+item.offset+item.size};
        const rr::ActorAnimationLibrary library(exe,global);
        size_t sequences=0,frames=0;
        for(int type=0;type<2;++type) {
            if(library.playback[type].size()!=52)throw std::runtime_error("Actor bank code count");
            for(size_t code=0;code<52;++code)if(library.playback[type][code]){++sequences;frames+=library.playback[type][code]->size();}
        }
        for (int type = 0; type < 2; ++type)
            for (const auto& sequence : library.descriptors[type]) if (sequence)
                for (const auto& frame : *sequence) {
                    const auto& views = library.views[type].at(frame.source_frame);
                    if (views.empty() || frame.playback.flags != views.front().flags)
                        throw std::runtime_error("Playback descriptor must retain its original graphics flags");
                }
        std::cout<<sequences<<" imported actor sequences, "<<frames<<" playback frames\n";
        const auto source=rr::read_exe_data(exe,0x4746c0,8);
        const auto first=rr::read_actor_sequence(cans[0],source);
        if(!first || first->size()!=1)throw std::runtime_error("Initial rider descriptor unavailable");
        for(size_t length:{size_t(0),size_t(11),size_t(15),cans[0].size()-1}) {
            bool rejected=false;try{rr::read_actor_sequence({cans[0].begin(),cans[0].begin()+length},source);}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("Truncated SNAC accepted");
        }
        std::ifstream file(argc>1?argv[1]:RR_SOURCE_DIR "/analysis/oracle/napa-actor-animation-fixtures.tsv");
        if(file) {
            std::map<std::string,int64_t> values;std::string line,key;int64_t value;
            while(std::getline(file,line)){std::istringstream row(line);if(row>>key>>value)values[key]=value;}
            size_t crash_rider_steps=0;
            for(int c=0;c<values.at("actor.crash_rider.count");++c) {
                const auto prefix="actor.crash_rider."+std::to_string(c)+".";rr::ActorAnimationState state;state.animation=int8_t(values.at(prefix+"initial"));state.deadline=UINT32_MAX;rr::CrashRiderAnimationInput input;input.crash=int32_t(values.at(prefix+"initial_crash"));
                for(int t=0;t<32;++t) {
                    const auto root=prefix+std::to_string(t)+".";auto get=[&](const char* key){return values.at(root+key);};
                    input.speed=int32_t(get("speed"));input.height_q8=int32_t(get("height"));
                    const auto* selected=rr::advance_crash_rider_animation(state,library.playback[0],library.fallback,input,uint32_t(get("tick")),[&]{return uint32_t(get("random"));});
                    if(input.crash!=get("crash") || state.animation!=get("animation") || state.frame!=get("frame") || state.queued!=get("queued") || state.loops!=get("loops") || state.deadline!=get("deadline"))throw std::runtime_error(root+"crash rider state mismatch");
                    for(int b=0;b<14;++b) {
                        const int actual=selected?(b<12?int(selected->render[b]):b==12?int(uint8_t(selected->duration)):int(selected->control)):-1;
                        if(actual!=values.at(root+"selected."+std::to_string(b)))throw std::runtime_error(root+"crash rider descriptor mismatch");
                    }
                    ++crash_rider_steps;
                }
            }
            std::cout<<crash_rider_steps<<" original crash rider animation steps match\n";
            size_t crash_steps=0;
            for(int c=0;c<values.at("actor.crash_bike.count");++c) {
                const auto prefix="actor.crash_bike."+std::to_string(c)+".";rr::ActorAnimationState state;state.animation=int8_t(values.at(prefix+"initial"));state.deadline=UINT32_MAX;
                for(int t=0;t<32;++t) {
                    const auto root=prefix+std::to_string(t)+".";auto get=[&](const char* key){return values.at(root+key);};
                    const auto* selected=rr::advance_crash_bike_animation(state,library.playback[0],library.fallback,int32_t(get("speed")),uint32_t(get("tick")),[&]{return uint32_t(get("random"));});
                    if(state.animation!=get("animation") || state.frame!=get("frame") || state.queued!=get("queued") || state.loops!=get("loops") || state.deadline!=get("deadline"))throw std::runtime_error(root+"crash bike state mismatch");
                    for(int b=0;b<14;++b) {
                        const int actual=selected?(b<12?int(selected->render[b]):b==12?int(uint8_t(selected->duration)):int(selected->control)):-1;
                        if(actual!=values.at(root+"selected."+std::to_string(b)))throw std::runtime_error(root+"crash bike descriptor mismatch");
                    }
                    ++crash_steps;
                }
            }
            std::cout<<crash_steps<<" original crash bike animation steps match\n";
            size_t riding_steps=0;
            for(int c=0;c<values.at("actor.riding.count");++c) {
                const auto prefix="actor.riding."+std::to_string(c)+".";rr::ActorAnimationState state;state.animation=int8_t(values.at(prefix+"initial"));state.deadline=UINT32_MAX;
                for(int t=0;t<16;++t) {
                    const auto root=prefix+std::to_string(t)+".";auto get=[&](const char* key){return values.at(root+key);};
                    rr::RidingAnimationInput input;input.speed=int32_t(get("speed"));input.throttle=int32_t(get("throttle"));input.lateral_speed=int32_t(get("lateral_speed"));input.boost=int32_t(get("boost"));input.flags=uint8_t(get("flags"));input.wrecked=get("wrecked")!=0;
                    const auto* selected=rr::advance_riding_animation(state,library.playback[0],library.fallback,input,uint32_t(get("tick")));
                    if(state.animation!=get("animation") || state.frame!=get("frame") || state.queued!=get("queued") || state.loops!=get("loops") || state.deadline!=get("deadline"))throw std::runtime_error(root+"riding animation state mismatch");
                    for(int i=0;i<14;++i) {
                        const auto actual=selected?(i<12?int(selected->render[i]):i==12?int(uint8_t(selected->duration)):int(selected->control)):-1;
                        if(actual!=values.at(root+"selected."+std::to_string(i)))throw std::runtime_error(root+"riding selected descriptor mismatch");
                    }
                    ++riding_steps;
                }
            }
            std::cout<<riding_steps<<" original player riding animation steps match\n";
            size_t view_count=0;
            for(size_t bank=0;bank<2;++bank) {
                const auto root="actor.views."+std::to_string(bank)+".";
                if(values.at(root+"count")!=int64_t(library.views[bank].size()))throw std::runtime_error("Original actor view group count");
                for(size_t group=0;group<library.views[bank].size();++group) {
                    const auto prefix=root+std::to_string(group)+".";const auto& cells=library.views[bank][group];
                    if(values.at(prefix+"count")!=int64_t(cells.size()))throw std::runtime_error("Original actor view cell count");
                    for(size_t f=0;f<cells.size();++f) {
                        const auto key=prefix+std::to_string(f)+".";
                        if(cells[f].frame!=values.at(key+"frame") || cells[f].flags!=values.at(key+"flags"))throw std::runtime_error(key+"original actor graphic mismatch");
                        ++view_count;
                    }
                }
            }
            std::cout<<view_count<<" original actor view cells match\n";
            size_t descriptors=0,selections=0;
            for(int c=0;c<values.at("actor.descriptor.count");++c) {
                const auto prefix="actor.descriptor."+std::to_string(c)+".";auto get=[&](const std::string& k){return values.at(prefix+k);};
                const auto bytes=rr::read_exe_data(exe,uint32_t(get("source")),size_t(get("count"))*8);
                for(size_t f=0;f<size_t(get("count"));++f) {
                    const auto root=std::to_string(f)+".";const auto frame=rr::read_actor_sequence(cans.at(size_t(get("bank"))),{bytes.begin()+f*8,bytes.begin()+f*8+8});
                    if(bool(frame)!=bool(get(root+"available")))throw std::runtime_error(prefix+root+"availability");
                    if(!frame)break;
                    std::array<uint8_t,16> packed{};const auto& result=frame->at(0).playback;
                    std::copy(result.render.begin(),result.render.end(),packed.begin());packed[12]=uint8_t(result.duration);packed[13]=result.control;
                    for(int b=0;b<16;++b)if(packed[b]!=get(root+"byte."+std::to_string(b)))throw std::runtime_error(prefix+root+"byte."+std::to_string(b)+" native="+std::to_string(packed[b])+" original="+std::to_string(get(root+"byte."+std::to_string(b))));
                    for(int sample=0;sample<48;++sample) {
                        ++selections;
                        const auto key="actor.selection."+std::to_string(c)+"."+std::to_string(f)+"."+std::to_string(sample)+".";
                        rr::ActorOrientation orientation;orientation.depth=256;orientation.heading=int32_t(values.at(key+"heading"));orientation.roll=int32_t(values.at(key+"roll"));
                        const auto angles=rr::actor_angles(orientation,0);
                        if(angles.yaw!=values.at(key+"yaw") || angles.view_angle!=values.at(key+"view_angle"))throw std::runtime_error(key+"orientation mismatch");
                        const auto bank=size_t(get("bank"));
                        const auto selection=rr::select_actor_view(result,library.views[bank],int32_t(values.at(key+"yaw")),int32_t(values.at(key+"roll")));
                        const auto& cell=library.views[bank].at(selection.group).at(selection.subframe);
                        const auto& graphic=library.graphics[bank].frames.at(cell.frame);
                        if(cell.frame!=values.at(key+"frame") || cell.flags!=uint32_t(values.at(key+"flags")) || selection.roll!=values.at(key+"selected_roll") || graphic.hotspot[0]!=values.at(key+"hotspot_x") || graphic.hotspot[1]!=values.at(key+"hotspot_y"))
                            throw std::runtime_error(key+"original actor view selection mismatch native frame="+std::to_string(cell.frame)+" original="+std::to_string(values.at(key+"frame"))+" flags="+std::to_string(cell.flags)+"/"+std::to_string(values.at(key+"flags"))+" roll="+std::to_string(selection.roll)+"/"+std::to_string(values.at(key+"selected_roll"))+" hotspot="+std::to_string(graphic.hotspot[0])+","+std::to_string(graphic.hotspot[1])+"/"+std::to_string(values.at(key+"hotspot_x"))+","+std::to_string(values.at(key+"hotspot_y")));
                    }
                    ++descriptors;
                }
            }
            std::cout<<descriptors<<" original actor descriptors and "<<selections<<" original view selections match\n";
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
