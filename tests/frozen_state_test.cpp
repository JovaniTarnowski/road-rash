#include "frozen_state.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using Header = std::map<std::string, int64_t>;
Header capture() {
    Header h{{"probe.frozen_words",1}, {"nodeaddr.0",100},
        {"nodeaddr.0.type",4}, {"nodeaddr.0.position",500},
        {"nodeaddr.0.half",200}, {"nodeaddr.0.lateral",-123},
        {"nodeaddr.1",200}, {"nodeaddr.1.type",8},
        {"nodeaddr.1.position",500}, {"nodeaddr.1.lateral",0}};
    for (const char* off : {"30","34","38","3c","40","108","10c","110","c0","2f4"})
        h[std::string("nodeaddr.0.bike_")+off] = 0;
    h["nodeaddr.0.bike_3c"] = -999;
    h["nodeaddr.0.bike_108"] = std::numeric_limits<int32_t>::min();
    h["nodeaddr.0.bike_10c"] = 6417;
    h["nodeaddr.0.bike_c0"] = 255;
    return h;
}
std::vector<rr::Rider> riders() {
    std::vector<rr::Rider> r(2);
    r[0].bike.i32(0x1c)=400; r[0].bike.i32(0x28)=777;
    r[1].frozen=true; r[1].bike.i32(0x1c)=500; r[1].bike.i32(0x28)=888;
    return r;
}
int main() {
    int failures=0;
    auto check=[&](bool yes,const char* what) { if(!yes) {++failures;std::cerr<<"FAIL "<<what<<'\n';} };
    for (bool demo : {false,true}) {
        auto r=riders(); rr::golden::apply_frozen_state(capture(),r,demo);
        check(r[1].bike.i32(0x28)==-123 && r[1].bike.i32(0x3c)==-999,"measured current differs from previous for human and demo");
        check(r[1].bike.i32(0x108)==std::numeric_limits<int32_t>::min() && r[1].bike.i32(0x10c)==6417 && r[1].bike.i32(0x110)==0,"negative and zero velocities");
        check(r[1].bike.u8(0xc0)==255 && r[1].bike.i32(0x2f4)==0,"flags byte and zero crash");
        check(r[1].half_detached && r[1].half_lateral==0,"linked zero lateral half");
        check(r[0].bike.i32(0x28)==777 && !r[0].half_detached,"moving player untouched");
    }
    // A same-position unrelated half must not replace the pointer-linked half.
    auto linked=capture(); linked["nodeaddr.9"]=900; linked["nodeaddr.9.type"]=8;
    linked["nodeaddr.9.position"]=500; linked["nodeaddr.9.lateral"]=777;
    auto linked_riders=riders(); rr::golden::apply_frozen_state(linked,linked_riders,false);
    check(linked_riders[1].half_lateral==0,"half identity comes from bike pointer");
    auto atomic_riders=riders(); atomic_riders.push_back(atomic_riders[1]);
    atomic_riders.back().bike.i32(0x1c)=600;
    bool atomic_threw=false;
    try {rr::golden::apply_frozen_state(capture(),atomic_riders,false);}
    catch(const std::runtime_error&) {atomic_threw=true;}
    check(atomic_threw && atomic_riders[1].bike.i32(0x28)==888,"later incomplete rider leaves earlier rider untouched");
    auto reject=[&](Header h,const char* what) {
        auto r=riders(); bool threw=false;
        try {rr::golden::apply_frozen_state(h,r,false);} catch(const std::runtime_error&) {threw=true;}
        check(threw,what); check(r[1].bike.i32(0x28)==888,"invalid declaration does not mutate state");
    };
    auto h=capture(); h.erase("nodeaddr.0.bike_3c"); reject(h,"missing declared word");
    h=capture(); h["nodeaddr.2"]=300; h["nodeaddr.2.type"]=4; h["nodeaddr.2.position"]=500; reject(h,"duplicate bike position");
    h=capture(); h["nodeaddr.2"]=200; h["nodeaddr.2.type"]=8; h["nodeaddr.2.position"]=500; reject(h,"duplicate node address");
    h=capture(); h["nodeaddr.0.half"]=999; reject(h,"missing linked half");
    h=capture(); h["nodeaddr.1.type"]=4; reject(h,"wrong linked half type");
    h=capture(); h["nodeaddr.1.position"]=501; reject(h,"wrong linked half position");
    h=capture(); h["nodeaddr.0.bike_c0"]=256; reject(h,"byte overflow");
    h=capture(); h["nodeaddr.0.bike_108"]=int64_t(std::numeric_limits<int32_t>::max())+1; reject(h,"word overflow");
    h=capture(); h["probe.frozen_words"]=2; reject(h,"unsupported declaration");
    h=capture(); h.erase("probe.frozen_words");
    auto r=riders(); rr::golden::apply_frozen_state(h,r,false);
    check(r[1].bike.i32(0x28)==888,"legacy human behavior preserved");
    rr::golden::apply_frozen_state(h,r,true);
    check(r[1].bike.i32(0x28)==-123 && r[1].bike.i32(0x3c)==-123 && r[1].bike.i32(0x108)==0,"legacy demo lateral-only fallback");
    r=riders(); rr::golden::apply_frozen_state({},r,true);
    check(r[1].bike.i32(0x28)==888,"legacy no node dump preserves grid");
    // Scheduler metadata hydrates from sched-era captures; legacy declared
    // captures keep the build dt 1 with sched_live false.
    auto s=capture(); s["sched.64.last"]=1; s["sched.64.next"]=5;
    s["sched.64.interval"]=4; s["sched.64.dt"]=5;
    r=riders(); rr::golden::apply_frozen_state(s,r,true);
    check(r[1].sched_live && r[1].sched_last==1 && r[1].sched_next==5 && r[1].sched_interval==4,"sched metadata hydrates");
    check(r[1].bike.i32(0x18)==5,"post-tick-1 dt lands in bike +0x18");
    r=riders(); rr::golden::apply_frozen_state(capture(),r,true);
    check(!r[1].sched_live && r[1].bike.i32(0x18)==riders()[1].bike.i32(0x18),"legacy declared capture keeps build dt");
    auto partial=s; partial.erase("sched.64.dt");
    auto pr=riders(); bool sched_threw=false;
    try {rr::golden::apply_frozen_state(partial,pr,true);} catch(const std::runtime_error&) {sched_threw=true;}
    check(sched_threw && pr[1].bike.i32(0x28)==888 && !pr[1].sched_live,"partial sched keys fail atomically");
    auto bad_iv=s; bad_iv["sched.64.interval"]=0;
    auto br=riders(); bool iv_threw=false;
    try {rr::golden::apply_frozen_state(bad_iv,br,true);} catch(const std::runtime_error&) {iv_threw=true;}
    check(iv_threw && !br[1].sched_live,"non-positive interval rejected");
    std::cout<<failures<<" failures\n"; return failures?1:0;
}
