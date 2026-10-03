#include "cel.hpp"
#include "resource.hpp"
#include "golden.hpp"
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
using Bytes=std::vector<uint8_t>;
static void word(Bytes& b,size_t p,uint32_t v){for(int i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
int main() {
    int failures=0;auto check=[&](bool ok,const char* what){if(!ok){++failures;std::cerr<<what<<'\n';}};
    Bytes b(176);word(b,0,0x43434220);word(b,4,80);word(b,72,0x80000002);word(b,76,2);
    word(b,80,0x504c5554);word(b,84,16);word(b,88,2);b[92]=0x34;b[93]=0x12;b[94]=0x78;b[95]=0x56;
    word(b,96,0x50444154);word(b,100,80);word(b,104,0xbc614e);word(b,136,4);word(b,140,2);word(b,144,1);word(b,160,36);
    for(int i=0;i<8;++i)b[168+i]=uint8_t(i);
    const auto cel=rr::read_cel(b);
    check(cel.width==4 && cel.height==2,"CCB dimensions");check(cel.palette.entries==std::vector<uint16_t>{0x1234,0x5678},"PLUT endian");
    check(cel.texture && cel.texture->mips[0].pixels==Bytes({0,1,2,3,4,5,6,7}),"indexed PDAT pixels");
    for(size_t n=0;n<b.size();++n){try{rr::read_cel(Bytes(b.begin(),b.begin()+n));check(false,"truncated cel accepted");}catch(const std::runtime_error&){}}
    auto corrupt=b;word(corrupt,160,0xffffffffu);try{rr::read_cel(corrupt);check(false,"bad mip offset accepted");}catch(const std::runtime_error&){}
    corrupt=b;word(corrupt,136,3);try{rr::read_cel(corrupt);check(false,"bad stride accepted");}catch(const std::runtime_error&){}
    const auto data=std::filesystem::path(RR_SOURCE_DIR)/"../cdrom/ROADRASH/DATA";
    const auto golden_path=rr::golden::oracle_dir()+"/raster-fixtures.tsv";
    std::optional<rr::golden::File> golden;
    if(std::filesystem::exists(golden_path))golden=rr::golden::load(golden_path);
    if(std::filesystem::exists(data)) {
        size_t cels=0,textures=0,legacy=0,mips=0;
        try {
            for(const auto& entry:std::filesystem::recursive_directory_iterator(data)) {
                const auto ext=entry.path().extension();if(ext!=".RSC" && ext!=".CRS" && ext!=".CAR")continue;
                std::ifstream in(entry.path(),std::ios::binary);Bytes bytes((std::istreambuf_iterator<char>(in)),{});
                for(const auto& resource:rr::read_resources(bytes))if(resource.tag=="CEL ") {
                    try {
                        const auto value=rr::read_cel(Bytes(bytes.begin()+resource.offset,bytes.begin()+resource.offset+resource.size));
                        ++cels;if(value.texture){++textures;mips+=value.texture->mips.size();}else ++legacy;
                        if(golden) {
                            if(golden->exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden->frames.empty())throw std::runtime_error("Wrong CEL golden");
                            const std::array<std::string,6> files={"GLOBAL.RSC","CANYN.CRS","CITY.CRS","HIWAY.CRS","MEDLY.CRS","NAPA.CRS"};
                            auto file=std::find(files.begin(),files.end(),entry.path().filename().string());
                            if(file==files.end())throw std::runtime_error("Unexpected CEL container");
                            const std::string prefix="cel."+std::to_string(file-files.begin())+"."+std::to_string(resource.id)+".";
                            const auto& fields=golden->frames.front();
                            check(fields.at(prefix+"width")==value.width && fields.at(prefix+"height")==value.height,"original CCB dimensions");
                            check(fields.at(prefix+"flags")==value.flags,"original CCB flags");
                            if(value.texture) {
                                check(fields.at(prefix+"magic")==0xbc614e && fields.at(prefix+"levels")==int64_t(value.texture->mips.size()),"original mip header");
                                check(fields.at(prefix+"texture_flags")==value.texture->flags,"original texture flags");
                                for(size_t level=0;level<value.texture->mips.size();++level) {
                                    const auto raw=std::filesystem::path(golden_path).parent_path()/(golden->scenario+"-cel"+std::to_string(file-files.begin())+"-"+std::to_string(resource.id)+"-mip"+std::to_string(level)+".raw");
                                    std::ifstream reference(raw,std::ios::binary);if(!reference)throw std::runtime_error("Missing original mip bytes");
                                    const Bytes expected((std::istreambuf_iterator<char>(reference)),{});
                                    check(expected==value.texture->mips[level].pixels,"original loaded mip bytes");
                                }
                            }else check(fields.at(prefix+"magic")==*value.legacy_pixel_word,"original legacy word retained");
                        }
                    }catch(const std::exception& e){throw std::runtime_error(entry.path().filename().string()+" cel "+std::to_string(resource.id)+": "+e.what());}
                }
            }
            if(golden)check(golden->frames.front().at("cel.count")==int64_t(cels),"original CEL count");
            std::cout<<cels<<" CEL resources: "<<textures<<" indexed textures, "<<mips<<" mips, "<<legacy<<" retained legacy words\n";
        }catch(const std::exception& e){check(false,e.what());}
    }
    std::cout<<failures<<" failures\n";return failures?1:0;
}
