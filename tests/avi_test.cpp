#include "avi.hpp"
#include "assets.hpp"
#include "ima_adpcm.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using Bytes=std::vector<uint8_t>;
void u32(Bytes& b,uint32_t v) { for (int i=0;i<4;++i) b.push_back(uint8_t(v>>(i*8))); }
Bytes tag(const char* s) { return Bytes(s,s+4); }
Bytes chunk(const char* id,Bytes data) { Bytes b=tag(id);u32(b,uint32_t(data.size()));b.insert(b.end(),data.begin(),data.end());if (data.size()&1) b.push_back(0);return b; }
void append(Bytes& a,const Bytes& b) { a.insert(a.end(),b.begin(),b.end()); }
Bytes list(const char* kind,Bytes data) { Bytes b=tag(kind);append(b,data);return chunk("LIST",b); }
Bytes minimal() {
    Bytes header;u32(header,rr::fourcc('v','i','d','s'));u32(header,rr::fourcc('c','v','i','d'));
    for (int i=0;i<3;++i) u32(header,0);u32(header,1);u32(header,15);while (header.size()<56) u32(header,0);
    Bytes format;u32(format,40);u32(format,4);u32(format,4);format.insert(format.end(),{1,0,24,0});u32(format,rr::fourcc('c','v','i','d'));while (format.size()<40) u32(format,0);
    Bytes stream=chunk("strh",header);append(stream,chunk("strf",format));
    Bytes root=tag("AVI ");append(root,list("hdrl",list("strl",stream)));
    Bytes movie=chunk("00dc",{1,2,3});append(movie,chunk("00dc",{4,5}));append(root,list("movi",movie));
    return chunk("RIFF",root);
}
}
int main(int argc,char** argv) {
    try {
        if (argc==3) {
            rr::Assets assets(std::filesystem::path(argv[1]).parent_path().string());
            const auto bytes=assets.read(std::filesystem::path(argv[1]).filename().string());const auto avi=rr::read_avi(bytes);
            std::ofstream pcm(argv[2],std::ios::binary);if (!pcm) throw std::runtime_error("Cannot write PCM");
            size_t frames=0;
            for (const auto& c:avi.chunks) {
                const auto& stream=avi.streams[c.stream];
                if (stream.type==rr::fourcc('v','i','d','s')) { ++frames;continue; }
                if (stream.type!=rr::fourcc('a','u','d','s')) continue;
                if (stream.audio_format!=17) throw std::runtime_error("Expected IMA corpus audio");
                const auto decoded=rr::decode_ima_adpcm(bytes.data()+c.offset,c.size,stream.audio_channels,stream.block_align);
                for (const int16_t s:decoded) { const char raw[2]={char(uint16_t(s)),char(uint16_t(s)>>8)};pcm.write(raw,2); }
            }
            if (!pcm) throw std::runtime_error("PCM write failure");
            std::cout<<frames<<" video packets, "<<avi.chunks.size()<<" total packets\n";return 0;
        }
        const auto bytes=minimal();const auto avi=rr::read_avi(bytes);
        if (avi.streams.size()!=1 || avi.streams[0].width!=4 || avi.streams[0].height!=4 || avi.chunks.size()!=2 || avi.truncated || avi.chunks[0].size!=3)
            throw std::runtime_error("Synthetic AVI parse failed");
        for (size_t cut=avi.chunks[0].offset+3;cut<bytes.size();++cut) {
            auto copy=bytes;copy.resize(cut);const auto truncated=rr::read_avi(copy);
            if (!truncated.truncated || truncated.chunks.empty()) throw std::runtime_error("Lost complete packet on truncation");
        }
        auto bad=bytes;bad[0]='X';
        try { rr::read_avi(bad);throw std::logic_error("Invalid magic accepted"); } catch (const std::runtime_error&) {}
        const auto root=std::filesystem::path(RR_SOURCE_DIR)/"../cdrom/ROADRASH";
        if (std::filesystem::exists(root)) {
            rr::Assets assets(root.string());size_t files=0,packets=0;
            for (const auto& e:std::filesystem::recursive_directory_iterator(root/"VIDEO")) {
                if (e.path().extension()!=".AVI") continue;
                const auto data=assets.read(std::filesystem::relative(e.path(),root).generic_string());const auto movie=rr::read_avi(data);
                if (movie.truncated || (movie.streams.size()!=1 && movie.streams.size()!=2) || movie.chunks.empty()) throw std::runtime_error("Invalid corpus AVI "+e.path().string());
                ++files;packets+=movie.chunks.size();
                if (e.path().filename()=="INTRO.AVI") {
                    auto cut=data;cut.resize(data.size()*3/5);const auto partial=rr::read_avi(cut);
                    if (!partial.truncated || partial.chunks.empty() || partial.chunks.size()>=movie.chunks.size()) throw std::runtime_error("INTRO truncated scan failed");
                }
            }
            if (files!=59) throw std::runtime_error("Unexpected AVI corpus count");
            std::cout<<files<<" AVIs, "<<packets<<" packets\n";
        }
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
