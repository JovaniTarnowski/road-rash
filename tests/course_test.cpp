#include "assets.hpp"
#include "course.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>
int main() {
    int failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
    if(!assets.exists("DATA/COURSES/NAPA.CRS")){std::cout<<"No imported course corpus\n";return 0;}
    size_t route_count=0,dual_count=0,event_count=0;
    // Plan 17 prerequisites (docs/research/post-11a-2026-10-12/simulation-and-courses.md):
    // split-node census (sequence of choices keyed to split identity) and
    // level-tagged finish recovery 0x004525c0 (first raw-type-6 event whose
    // parameter[0]-level==1). Expected graph-relative event-start samples from
    // analysis/oracle/post11a-research/census.json (26 branch combos total).
    struct ComboExpect { uint32_t split; int branch; };
    struct CourseExpect { const char* name; std::vector<std::vector<int>> combos; size_t samples; int finish[5]; };
    const std::vector<CourseExpect> census{
        {"CITY",{{0},{1}},5606,{1821,2840,3865,4510,5419}},
        {"HIWAY",{{0,0},{1,0},{0,1},{1,1}},0,{0,0,0,0,0}},
        {"MEDLY",{{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}},0,{0,0,0,0,0}},
        {"NAPA",{{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}},0,{0,0,0,0,0}},
        {"CANYN",{{0,0},{1,0},{0,1},{1,1}},0,{0,0,0,0,0}},
    };
    // Full per-route expectations from analysis/oracle/post11a-research/census.json:
    // {route length, level 1..5 first-match finish event-start samples}.
    struct RouteExpect { std::vector<int> combo; size_t samples; int finish[5]; };
    const std::vector<RouteExpect> hiway{{{{0,0}},6005,{1810,2650,3295,4205,5566}},{{{1,0}},5985,{1790,2630,3275,4185,5546}},
        {{{0,1}},6095,{1810,2650,3295,4295,5656}},{{{1,1}},6075,{1790,2630,3275,4275,5636}}};
    const std::vector<RouteExpect> medly{{{{0,0,0}},5634,{1818,2719,3642,4963,5532}},{{{1,0,0}},5618,{1802,2703,3626,4947,5516}},
        {{{0,1,0}},5681,{1818,2719,3689,5010,5579}},{{{1,1,0}},5665,{1802,2703,3673,4994,5563}},
        {{{0,0,1}},5614,{1818,2719,3642,4943,5512}},{{{1,0,1}},5598,{1802,2703,3626,4927,5496}},
        {{{0,1,1}},5661,{1818,2719,3689,4990,5559}},{{{1,1,1}},5645,{1802,2703,3673,4974,5543}}};
    const std::vector<RouteExpect> napa{{{{0,0,0}},6224,{1715,2565,3570,4280,6164}},{{{1,0,0}},6249,{1740,2590,3595,4305,6189}},
        {{{0,1,0}},6294,{1715,2565,3640,4350,6234}},{{{1,1,0}},6319,{1740,2590,3665,4375,6259}},
        {{{0,0,1}},6224,{1715,2565,3570,4280,6164}},{{{1,0,1}},6249,{1740,2590,3595,4305,6189}},
        {{{0,1,1}},6294,{1715,2565,3640,4350,6234}},{{{1,1,1}},6319,{1740,2590,3665,4375,6259}}};
    const std::vector<RouteExpect> canyn{{{{0,0}},5423,{1862,2719,3685,4215,5361}},{{{1,0}},5443,{1882,2739,3705,4235,5381}},
        {{{0,1}},5423,{1862,2719,3685,4215,5361}},{{{1,1}},5443,{1882,2739,3705,4235,5381}}};
    for(const auto& expect:census) {
        const auto course=rr::read_course(assets.read(std::string("DATA/COURSES/")+expect.name+".CRS"));
        const auto splits=rr::split_nodes(course);
        const std::string tag=expect.name;
        check(splits.size()==expect.combos[0].size(),("Split count for "+tag).c_str());
        size_t combos=1<<splits.size();
        check(expect.combos.size()==combos,("Branch combination count for "+tag).c_str());
        std::set<size_t> lengths;
        const std::vector<RouteExpect>* table=nullptr;
        if(tag=="HIWAY")table=&hiway; else if(tag=="MEDLY")table=&medly;
        else if(tag=="NAPA")table=&napa; else if(tag=="CANYN")table=&canyn;
        for(const auto& combo:expect.combos) {
            const auto route=rr::make_route_choices(course,combo);
            lengths.insert(route.size());
            for(int level=1;level<=5;++level) {
                const auto at=rr::finish_event_sample(course,route,level-1);
                if(expect.finish[0])check(at==expect.finish[level-1],("Level-tagged finish for "+tag).c_str());
                else check(at>0,("Positive level-tagged finish for "+tag).c_str());
            }
            if(table) for(const auto& row:*table) if(row.combo==combo) {
                check(route.size()==row.samples,("Route length for "+tag).c_str());
                for(int level=1;level<=5;++level)
                    check(rr::finish_event_sample(course,route,level-1)==row.finish[level-1],
                          ("Route finish L"+std::to_string(level)+" for "+tag).c_str());
            }
        }
        if(tag=="CITY")check(lengths==std::set<size_t>{5606},"CITY route length");
        else check(lengths.size()>1||splits.size()==1,"Distinct physical route lengths differ");
    }
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
