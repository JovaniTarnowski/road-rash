#include "actor_animation.hpp"
#include "assets.hpp"
#include "exe_tables.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
int main(int argc,char** argv) {
    try {
        rr::ActorAnimationFrame first;first.duration=2;
        rr::ActorAnimationFrame loop;loop.duration=1;loop.control=0x11;
        rr::ActorAnimationBank bank={rr::ActorSequence{first,loop},std::nullopt};
        rr::ActorAnimationState state;
        if(!rr::set_animation(state,bank,0,100) || rr::set_animation(state,bank,1,100))throw std::runtime_error("Unavailable sequence selection");
        rr::advance_animation(state,bank,100);if(state.deadline!=102 || state.frame!=0)throw std::runtime_error("Initial deadline");
        rr::advance_animation(state,bank,102);if(state.frame!=1 || !state.selected || (*state.selected)[1]!=0)throw std::runtime_error("Pre-advance selected frame");
        rr::advance_animation(state,bank,103);if(state.frame!=1 || state.loops!=1 || state.deadline!=104)throw std::runtime_error("Loop frame and accumulated deadline");
        if(!rr::set_animation(state,bank,-1,500) || state.deadline!=4100)throw std::runtime_error("Idle deadline");
        bool rejected=false;state.frame=-2;try{rr::advance_animation(state,bank,500);}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("Invalid signed frame accepted");
        std::ifstream file(argc>1?argv[1]:RR_SOURCE_DIR "/analysis/oracle/napa-actor-animation-fixtures.tsv");
        if(file) {
            std::map<std::string,int64_t> values;std::string line,key;int64_t value;
            while(std::getline(file,line)){std::istringstream row(line);if(row>>key>>value)values[key]=value;}
            rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
            const auto bytes=rr::read_exe_data(assets.read("ROADRASH.EXE"),0x464430,16);std::vector<int8_t> fallback;
            for(int i=0;i<4;++i)fallback.push_back(int8_t(bytes[i*4]));
            size_t steps=0;
            for(int c=0;c<values.at("actor.animation.count");++c) {
                const auto prefix="actor.animation."+std::to_string(c)+".";
                auto get=[&](const std::string& k){return values.at(prefix+k);};
                rr::ActorAnimationBank sequences(4);
                for(int k=0;k<3;++k) {
                    sequences[k]=rr::ActorSequence(4);
                    for(int f=0;f<4;++f){auto root="sequence."+std::to_string(k)+"."+std::to_string(f)+".";auto& frame=(*sequences[k])[f];frame.duration=int8_t(get(root+"duration"));frame.control=uint8_t(get(root+"control"));}
                }
                rr::ActorAnimationState s;
                for(int t=0;t<32;++t) {
                    const auto root=std::to_string(t)+".";const uint32_t tick=uint32_t(get(root+"tick"));
                    auto check=[&](const std::string& field,int64_t result){if(result!=get(root+field))throw std::runtime_error(prefix+root+field+" native="+std::to_string(result)+" original="+std::to_string(get(root+field)));};
                    if(get(root+"set_code")!=-2)check("set_result",rr::set_animation_with_fallback(s,sequences,int8_t(get(root+"set_code")),tick,fallback));
                    if(get(root+"queue_code")!=-2)check("queue_result",rr::queue_animation(s,sequences,int8_t(get(root+"queue_code"))));
                    rr::advance_animation(s,sequences,tick);
                    check("animation",s.animation);check("frame",s.frame);check("queued",s.queued);check("deadline",s.deadline);check("loops",s.loops);check("selected_sequence",s.selected?(*s.selected)[0]:-1);check("selected_frame",s.selected?(*s.selected)[1]:-1);
                    ++steps;
                }
            }
            std::cout<<steps<<" original actor animation steps match\n";
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
