#include "wav.hpp"
#include "assets.hpp"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
void u16(std::vector<uint8_t>& b,uint16_t n) { b.push_back(uint8_t(n));b.push_back(uint8_t(n>>8)); }
void u32(std::vector<uint8_t>& b,uint32_t n) { u16(b,uint16_t(n));u16(b,uint16_t(n>>16)); }
void tag(std::vector<uint8_t>& b,const char* text) { b.insert(b.end(),text,text+4); }
std::vector<uint8_t> wav(int channels,int bits,const std::vector<uint8_t>& data,bool junk=false) {
    std::vector<uint8_t> b;
    tag(b,"RIFF");u32(b,0);tag(b,"WAVE");
    if (junk) { tag(b,"JUNK");u32(b,1);b.push_back(7);b.push_back(0); }
    tag(b,"fmt ");u32(b,16);u16(b,1);u16(b,uint16_t(channels));u32(b,11025);
    u32(b,uint32_t(11025*channels*bits/8));u16(b,uint16_t(channels*bits/8));u16(b,uint16_t(bits));
    tag(b,"data");u32(b,uint32_t(data.size()));b.insert(b.end(),data.begin(),data.end());
    if (data.size()%2) b.push_back(0);
    const uint32_t size=uint32_t(b.size()-8);
    for (int i=0;i<4;++i) b[4+i]=uint8_t(size>>(i*8));
    return b;
}
}
int main(int argc,char**) {
    if (argc>1) {
        const auto root=std::filesystem::path(RR_SOURCE_DIR)/"../cdrom/ROADRASH";
        if (!std::filesystem::exists(root)) { std::cerr << "SKIP disc missing\n";return 77; }
        try {
            rr::Assets assets(root.string());size_t effects=0,music=0;
            for (const std::string directory:{"AUDIO/EFFECTS","AUDIO/MUSIC"}) {
                for (const auto& entry:std::filesystem::directory_iterator(root/directory)) {
                    const auto extension=entry.path().extension();
                    if (extension!=".WAV" && extension!=".RRA") continue;
                    const auto sound=rr::read_wav(assets.read(directory+"/"+entry.path().filename().string()));
                    const bool effect=extension==".WAV";
                    if (sound.rate!=(effect ? 11025 : 22050) || sound.channels!=(effect ? 1 : 2) || sound.samples.empty())
                        throw std::runtime_error("Unexpected corpus audio format: "+entry.path().string());
                    if (effect) ++effects;else ++music;
                }
            }
            std::cout << effects << " effects, " << music << " music tracks decoded\n";
            return effects==72 && music==14 ? 0 : 1;
        } catch (const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
    }
    int failures=0;
    auto check=[&](bool yes,const char* what) { if (!yes) { ++failures;std::cerr << "FAIL " << what << '\n'; } };
    const auto bytes=wav(1,8,{0x80,0xff,0},true);
    const auto sound=rr::read_wav(bytes);
    check(sound.rate==11025 && sound.channels==1 && sound.samples==std::vector<int16_t>{0,32512,-32768},"8-bit samples and padded unknown chunk");
    const auto stereo=rr::read_wav(wav(2,16,{0,0x80,0xff,0x7f,1,0,0xff,0xff}));
    check(stereo.samples==std::vector<int16_t>{-32768,32767,1,-1},"16-bit signed little-endian stereo");
    for (size_t length=0;length<bytes.size();++length) {
        try { rr::read_wav(std::vector<uint8_t>(bytes.begin(),bytes.begin()+length));check(false,"all truncations rejected"); }
        catch (const std::runtime_error&) {}
    }
    for (const auto invalid:{wav(0,8,{1}),wav(3,8,{1,2,3}),wav(1,24,{1,2,3}),wav(2,16,{1,2})}) {
        try { rr::read_wav(invalid);check(false,"invalid channel/depth/alignment rejected"); } catch (const std::runtime_error&) {}
    }
    for (size_t at:{size_t(22),size_t(46)}) {
        auto corrupt=bytes;corrupt[at]='X';
        try { rr::read_wav(corrupt);check(false,"missing format/data rejected"); } catch (const std::runtime_error&) {}
    }
    std::vector<uint8_t> ima;
    tag(ima,"RIFF");u32(ima,0);tag(ima,"WAVE");
    tag(ima,"fmt ");u32(ima,20);u16(ima,17);u16(ima,1);u32(ima,11025);u32(ima,9800);u16(ima,8);u16(ima,4);u16(ima,2);u16(ima,9);
    tag(ima,"fact");u32(ima,4);u32(ima,3);
    tag(ima,"data");u32(ima,8);ima.insert(ima.end(),{100,0,0,0,0x21,0,0,0});
    for (int i=0;i<4;++i) ima[4+i]=uint8_t((ima.size()-8)>>(i*8));
    check(rr::read_wav(ima).samples==std::vector<int16_t>{100,101,104},"IMA format and fact sample trimming");
    for (size_t n=0;n<ima.size();++n) {
        try { rr::read_wav(std::vector<uint8_t>(ima.begin(),ima.begin()+n));check(false,"truncated IMA WAV accepted"); } catch (const std::runtime_error&) {}
    }
    ima[48]=10;
    try { rr::read_wav(ima);check(false,"fact beyond decoded samples accepted"); } catch (const std::runtime_error&) {}
    std::cout << failures << " failures\n";return failures ? 1 : 0;
}
