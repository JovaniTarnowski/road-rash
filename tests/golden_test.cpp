#include "golden.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    const std::string ok = "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na.b\t-5\nframe\t1\na.b\t7\nend\n";
    auto f = rr::golden::parse(ok);
    check(f.exe=="abc" && f.scenario=="s" && f.frames.size()==2, "header and frames");
    check(f.frames[0].at("a.b")==-5 && f.frames[1].at("a.b")==7, "values");
    for (const char* bad : {
            "exe\tabc\nframe\t0\nend\n",                               // no magic line
            "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na\t1\n",   // truncated: no end
            "# rr-oracle 1\nexe\tabc\nscenario\ts\na\t1\nend\n",        // key before frame
            "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na\tx\nend\n" }) { // non-integer
        try { rr::golden::parse(bad); check(false, bad); } catch (const std::runtime_error&) {}
    }
    for(const std::string& suffix:{"a\t9223372036854775808\n","a\t-9223372036854775809\n","a\t1\na\t2\n","frame\t3\n","exe\tabc\n"}) {
        try {rr::golden::parse("# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\n"+suffix+"end\n");check(false,"invalid duplicate/range/sequence accepted");}catch(const std::runtime_error&){}
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
