#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace fixture {
inline void both32(std::vector<uint8_t>& b,size_t at,uint32_t n) {
    for (unsigned i=0;i<4;++i) { b.at(at+i)=uint8_t(n>>(i*8)); b.at(at+4+i)=uint8_t(n>>((3-i)*8)); }
}
inline void record(std::vector<uint8_t>& b,size_t at,const std::string& name,uint32_t sector,uint32_t size,bool directory) {
    const size_t length=(33+name.size()+1)&~size_t(1);
    b.at(at)=uint8_t(length); both32(b,at+2,sector); both32(b,at+10,size);
    b.at(at+25)=directory ? 2 : 0; b.at(at+28)=1; b.at(at+31)=1;
    b.at(at+32)=uint8_t(name.size());
    std::copy(name.begin(),name.end(),b.begin()+at+33);
}
inline std::vector<uint8_t> iso() {
    std::vector<uint8_t> b(21*2048);
    const size_t p=16*2048;
    b[p]=1; std::memcpy(b.data()+p+1,"CD001",5); b[p+6]=1;
    both32(b,p+80,21); b[p+129]=8; b[p+130]=8;
    record(b,p+156,std::string(1,'\0'),18,2048,true);
    b[17*2048]=255; std::memcpy(b.data()+17*2048+1,"CD001",5); b[17*2048+6]=1;
    record(b,18*2048,"ROADRASH",19,2048,true);
    record(b,19*2048,"A.BIN;1",20,5,false);
    std::memcpy(b.data()+20*2048,"hello",5);
    return b;
}
inline size_t read_at(const std::vector<uint8_t>& b,uint64_t at,uint8_t* out,size_t n) {
    if (at>b.size() || n>b.size()-size_t(at)) return 0;
    if (n) std::memcpy(out,b.data()+at,n);
    return n;
}
}
