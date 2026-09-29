#include "resource.hpp"
#include <iostream>
#include <functional>

int main() {
    int failures = 0;
    auto check = [&](bool value) { if (!value) ++failures; };
    auto reject = [&](const std::vector<uint8_t>& b) {
        try { rr::read_resources(b); ++failures; } catch (const std::runtime_error&) {}
    };
    std::vector<uint8_t> b(112);
    auto word = [&](size_t p, uint32_t v) { for (int i=0; i<4; ++i) b[p+i] = uint8_t(v>>(8*i)); };
    auto tag = [&](size_t p, const char* s) { for (int i=0; i<4; ++i) b[p+i] = s[i]; };
    tag(0,"CRSR"); tag(12,"LBTR"); word(16,48); tag(48,"LBTR"); word(56,1);
    tag(64,"CEPS"); word(68,7); word(72,96); word(76,16);
    auto items = rr::read_resources(b);
    check(items.size()==1 && items[0].tag=="SPEC" && items[0].id==7 && items[0].offset==96 && items[0].size==16);
    for (size_t length=0; length<112; ++length) reject(std::vector<uint8_t>(b.begin(),b.begin()+length));
    word(72,0xfffffff0); reject(b); word(72,96);
    word(76,0xffffffff); reject(b); word(76,16);
    word(56,0xffffffff); reject(b); word(56,1);
    word(16,0xfffffff0); reject(b); word(16,48);
    b[0]=0; reject(b);
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
