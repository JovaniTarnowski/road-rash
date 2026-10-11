#include "assets.hpp"
#include "course.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
int main() {
    int failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
    if(!assets.exists("DATA/COURSES/NAPA.CRS")){std::cout<<"No imported course corpus\n";return 0;}
    size_t route_count=0,dual_count=0,event_count=0;
    for(const char* name:{"CITY","HIWAY","MEDLY","NAPA","CANYN"}) {
        const auto course=rr::read_course(assets.read(std::string("DATA/COURSES/")+name+".CRS"));
        for(const auto& section:course.sections)event_count+=section.family_events.size();
        for(bool second:{false,true})for(const auto& sample:rr::make_route(course,second)) {
            ++route_count;
            check(sample.local<course.sections.at(sample.section).samples.size(),"Own route section/local metadata");
            if(!sample.dual)continue;
            ++dual_count;const auto& node=course.nodes.at(sample.other_node);
            check(node.kind==0 && node.link==sample.other_section && sample.other_local<node.length,"Other route section/node/local metadata");
            const auto& other=course.sections.at(sample.other_section).samples.at(sample.other_local);
            check(other.bend==sample.other.bend && other.rise==sample.other.rise && other.left_q8==sample.other.left_q8 && other.right_q8==sample.other.right_q8,"Other roadway path corresponds to terrain source");
        }
    }
    std::cout<<route_count<<" route samples, "<<dual_count<<" dual references, "<<event_count<<" family records\n";
    const std::string path=rr::golden::oracle_dir()+"/napa-events.tsv";
    if(std::filesystem::exists(path)) {
        try {
            const auto capture=rr::golden::load(path);
            if(capture.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || capture.frames.size()!=1)throw std::runtime_error("Wrong course event oracle");
            const auto& fields=capture.frames.at(0);const auto course=rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
            check(fields.at("course.events.sections")==int64_t(course.sections.size()),"Original course section count");
            size_t compared=0;
            for(size_t section=0;section<course.sections.size();++section) {
                const auto prefix="course.events."+std::to_string(section)+".";
                const auto& samples=course.sections[section].samples;
                check(fields.at(prefix+"count")==int64_t(samples.size()),"Original section sample count");
                for(size_t j=0;j<samples.size();++j) {
                    const auto key=prefix+std::to_string(j)+".";const auto& sample=samples[j];++compared;
                    if(sample.marking_flags!=fields.at(key+"flags") || sample.event_type!=fields.at(key+"value.0")) {check(false,"Original course event type/marking mismatch");std::cerr<<section<<':'<<j<<'\n';}
                    for(int v=0;v<3;++v)check(sample.event_parameters[v]==fields.at(key+"value."+std::to_string(v+1)),"Original event parameter mismatch");
                }
            }
            std::cout<<compared<<" original Napa event samples compared\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    std::cout<<failures<<" failures\n";return failures?1:0;
}
