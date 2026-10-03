#include "terrain.hpp"
#include "course.hpp"
#include "golden.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using Bytes=std::vector<uint8_t>;
static void word(Bytes& b,size_t p,uint32_t v) { for(int i=0;i<4;++i) b[p+i]=uint8_t(v>>(8*i)); }
static Bytes fixture(int kind) {
    const size_t size=kind==0?144:kind==1?248:248;
    Bytes b(size);const char* tag=kind==0?"DLBR":kind==1?"NTMR":"LIHR";
    for(int i=0;i<4;++i)b[i]=tag[i];word(b,4,uint32_t(size));word(b,8,kind==2?3:64);
    size_t selectors=16;
    if(kind==1) {
        word(b,12,3);word(b,16,3);selectors=120;
        for(int side=0;side<2;++side) for(int j=0;j<3;++j) {
            size_t p=24+(side*3+j)*16;word(b,p,j==2?64:j*20);
            const uint8_t values[]={0,uint8_t(3+j),uint8_t(20+side*5),4,30,6,5,40};
            for(int i=0;i<8;++i)b[p+4+i]=values[i];word(b,p+12,uint32_t(side? -40:70));
        }
    } else if(kind==2) {
        for(int j=0;j<3;++j) {
            const size_t p=20+76*j;word(b,p,j*20);word(b,p+4,(j+1)*20);b[p+8]=uint8_t(10+j);b[p+9]=uint8_t(20+j);
            for(int i=0;i<8;++i) {
                size_t q=p+12+8*i;
                const uint8_t values[]={uint8_t(10+i*12),uint8_t(20+i*12),3,uint8_t(-10+i),uint8_t(12+i),4,1,5};
                for(int k=0;k<8;++k)b[q+k]=values[k];
            }
        }
    }
    if(kind!=2) for(int i=0;i<64;++i) {b[selectors+2*i]=uint8_t(i);b[selectors+2*i+1]=uint8_t(63-i);}
    return b;
}
int main() {
    int failures=0;
    auto check=[&](bool b,const char* msg) {if(!b){++failures;std::cerr<<msg<<'\n';}};
    for(int kind=0;kind<3;++kind) {
        const auto bytes=fixture(kind);const auto d=rr::read_terrain(bytes,64);
        rr::TerrainIterator a(d,0,[]{return 123u;}),b(d,0,[]{return 123u;});
        for(int i=0;i<48;++i) {
            const auto x=a.slice(-500,500),y=b.slice(-500,500);
            check(x.type==(kind==0?0:kind==1?1:4),"terrain type");
            check(x.points==y.points && x.walls==y.walls,"deterministic random source");
            a.step(256);b.step(256);
        }
        auto reject=[&](Bytes input,uint32_t samples) {
            try {rr::read_terrain(input,samples);check(false,"accepted malformed terrain");}catch(const std::runtime_error&){}
        };
        auto corrupt=bytes;word(corrupt,4,0xffffffffu);reject(corrupt,64);
        corrupt=bytes;word(corrupt,8,0xffffffffu);reject(corrupt,64);
        corrupt=bytes;corrupt[0]='?';reject(corrupt,64);
        if(kind==1) {corrupt=bytes;word(corrupt,12,0);reject(corrupt,64);}
        if(kind==2) {corrupt=bytes;corrupt[32]=255;corrupt[33]=0;reject(corrupt,64);corrupt=bytes;corrupt[38]=0;reject(corrupt,64);}
        else reject(bytes,63);
        for(size_t n=0;n<bytes.size();++n) {
            try {rr::read_terrain(Bytes(bytes.begin(),bytes.begin()+n),64);check(false,"accepted truncated terrain");}catch(const std::runtime_error&){}
        }
    }
    const auto golden_path=rr::golden::oracle_dir()+"/raster-fixtures.tsv";
    if(std::filesystem::exists(golden_path)) {
        try {
            const auto golden=rr::golden::load(golden_path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.empty()) throw std::runtime_error("Wrong terrain oracle");
            const auto& fields=golden.frames.front();
            for(int kind=0;kind<3;++kind) for(int random=0;random<3;++random) for(int reverse=0;reverse<2;++reverse) {
                const uint32_t values[]={0,123,32767};const auto d=rr::read_terrain(fixture(kind),64);
                rr::TerrainIterator iterator(d,reverse?48*256:0,[&]{return values[random];});
                for(int tick=0;tick<48;++tick) {
                    const std::string prefix="terrain."+std::to_string(kind)+"."+std::to_string(random)+"."+std::to_string(reverse)+"."+std::to_string(tick)+".";
                    auto compare=[&](const std::string& key,int32_t value) {
                        const auto i=fields.find(prefix+key);
                        if(i==fields.end()) throw std::runtime_error("Missing terrain oracle key: "+prefix+key);
                        if(i->second!=value) { if(failures<12) std::cerr<<prefix+key<<" native="<<value<<" original="<<i->second<<'\n';++failures; }
                    };
                    const auto slice=iterator.slice(-500,500);
                    compare("type",slice.type);
                    for(int i=0;i<2;++i) compare("selector."+std::to_string(i),slice.selectors[i]);
                    if(kind==1) for(int i=0;i<6;++i) compare("wall."+std::to_string(i),slice.walls[i/3][i%3]);
                    if(kind==2) {
                        for(int i=0;i<12;++i) compare("point."+std::to_string(i),slice.points[i/2][i%2]);
                        for(int i=0;i<6;++i) compare("shade."+std::to_string(i),slice.shades[i]);
                    }
                    iterator.step(reverse?-256:256);
                }
            }
            std::cout<<"864 original terrain slices compared\n";
        }catch(const std::exception& e){check(false,e.what());}
    }
    const auto dir=std::filesystem::path(RR_SOURCE_DIR)/"../cdrom/ROADRASH/DATA/COURSES";
    if(std::filesystem::exists(dir)) {
        size_t sections=0;
        try {
            for(const auto& entry:std::filesystem::directory_iterator(dir)) if(entry.path().extension()==".CRS") {
                std::ifstream in(entry.path(),std::ios::binary);Bytes bytes((std::istreambuf_iterator<char>(in)),{});
                const auto course=rr::read_course(bytes);
                for(const auto& section:course.sections) {
                    ++sections;uint32_t state=1;
                    rr::TerrainIterator iterator(section.terrain,0,[&]{state=state*214013u+2531011u;return(state>>16)&32767;});
                    for(size_t i=0;i<section.samples.size();++i) {iterator.slice(-500,500);iterator.step(256);}
                    rr::TerrainIterator reverse(section.terrain,int32_t((section.samples.size()-1)*256),[&]{state=state*214013u+2531011u;return(state>>16)&32767;});
                    for(size_t i=section.samples.size();i>0;--i) {reverse.slice(-500,500);if(i>1)reverse.step(-256);}
                }
            }
            std::cout<<sections<<" terrain sections decoded and traversed\n";
        }catch(const std::exception& e){check(false,e.what());}
    }
    std::cout<<failures<<" failures\n";return failures?1:0;
}
