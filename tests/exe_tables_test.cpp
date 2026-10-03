#include "exe_tables.hpp"
#include <iostream>
static void word(std::vector<uint8_t>& b,size_t p,uint32_t v){for(int i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
int main(){
    std::vector<uint8_t> b(1024);word(b,0,0x5a4d);word(b,60,128);word(b,128,0x4550);word(b,132,0x0001014c);word(b,148,32);word(b,152,0x10b);word(b,180,0x400000);
    word(b,196,0x1000);word(b,200,512);word(b,204,512);b[528]=17;b[529]=23;
    if(rr::read_exe_data(b,0x401010,2)!=std::vector<uint8_t>{17,23})return 1;
    for(size_t n=0;n<530;++n){try{rr::read_exe_data({b.begin(),b.begin()+n},0x401010,2);return 1;}catch(const std::runtime_error&) {}}
    for(auto address:{0x3fffffu,0x401200u,0x402000u})try{rr::read_exe_data(b,address,2);return 1;}catch(const std::runtime_error&){}
    try{rr::read_exe_data(b,0x4011ff,2);return 1;}catch(const std::runtime_error&){}
    std::cout<<"PE virtual-address bounds pass\n";return 0;
}
