#include "importer.hpp"
#include "iso9660.hpp"
#include "sha256.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>
namespace rr {
namespace fs=std::filesystem;
namespace {
struct CloseIO { void operator()(SDL_IOStream* p) const { if (p) SDL_CloseIO(p); } };
using IO=std::unique_ptr<SDL_IOStream,CloseIO>;
void validate_path(const std::string& path) {
    if (path.empty() || path.find('\\')!=std::string::npos || path.find('\0')!=std::string::npos || fs::path(path).is_absolute())
        throw std::runtime_error("Invalid import reference path");
    if (path==".rr-import" || path==".rr-source.iso") throw std::runtime_error("Reserved import metadata path");
    for (const auto& part:fs::path(path)) if (part==".." || part=="." || part.empty())
        throw std::runtime_error("Import reference escapes destination");
}
std::vector<uint8_t> load_file(const fs::path& path) {
    IO io(SDL_IOFromFile(path.string().c_str(),"rb"));
    if (!io) throw std::runtime_error("Cannot open source file: "+path.string());
    const Sint64 size=SDL_GetIOSize(io.get());
    if (size<0 || size>256*1024*1024) throw std::runtime_error("Invalid source file size: "+path.string());
    std::vector<uint8_t> result(static_cast<size_t>(size));
    if (SDL_ReadIO(io.get(),result.data(),result.size())!=result.size()) throw std::runtime_error("Truncated source file: "+path.string());
    return result;
}
}
const std::vector<ReferenceHash>& reference_hashes() {
    static const std::vector<ReferenceHash> hashes={
#include "reference_hashes.inc"
    };
    return hashes;
}
std::string data_root() {
    if (const char* override_path=SDL_getenv("RR_PREF_OVERRIDE")) return (fs::path(override_path)/"game"/"ROADRASH").string();
    char* pref=SDL_GetPrefPath("RoadRashNative","RoadRash");
    if (!pref) throw std::runtime_error(SDL_GetError());
    const std::string result=(fs::path(pref)/"game"/"ROADRASH").string();
    SDL_free(pref);
    return result;
}
ImportResult import_game(const std::string& source,const std::string& dest,
                         const std::function<void(const ImportProgress&)>& progress) {
    return import_game(source,dest,reference_hashes(),progress);
}
ImportResult import_game(const std::string& source,const std::string& dest,
                         const std::vector<ReferenceHash>& hashes,
                         const std::function<void(const ImportProgress&)>& progress) {
    ImportResult result;
    fs::path staging;
    try {
        if (hashes.empty()) throw std::runtime_error("Empty import reference table");
        std::set<std::string> paths;
        for (const auto& entry:hashes) {
            validate_path(entry.path);
            if (!paths.insert(entry.path).second || entry.hash.size()!=64 || entry.hash.find_first_not_of("0123456789abcdef")!=std::string::npos)
                throw std::runtime_error("Invalid import reference table");
        }
        const fs::path destination(dest);
        if (dest.empty() || destination==destination.root_path()) throw std::runtime_error("Invalid import destination");
        if (fs::exists(destination) && (!fs::is_directory(destination) || !fs::is_empty(destination)))
            throw std::runtime_error("Import destination already contains files: "+dest);
        if (!destination.parent_path().empty()) fs::create_directories(destination.parent_path());
        for (unsigned attempt=0;attempt<100;++attempt) {
            const fs::path candidate=dest+".partial."+std::to_string(SDL_GetTicksNS())+"."+std::to_string(attempt);
            if (fs::create_directory(candidate)) { staging=candidate; break; }
        }
        if (staging.empty()) throw std::runtime_error("Cannot create import staging directory");
        std::error_code source_error;
        const bool folder=fs::is_directory(source,source_error);
        fs::path source_root(source);
        if (folder && fs::is_directory(source_root/"ROADRASH")) source_root/="ROADRASH";
        IO iso_io;
        std::unique_ptr<Iso9660> iso;
        if (!folder) {
            iso_io.reset(SDL_IOFromFile(source.c_str(),"rb"));
            if (!iso_io) throw std::runtime_error(std::string("Cannot open ISO: ")+SDL_GetError());
            // Android document providers may expose an unseekable pipe. Cache
            // it inside our exclusively owned staging directory before parsing.
            if (SDL_SeekIO(iso_io.get(),0,SDL_IO_SEEK_SET)!=0) {
                const auto cache=staging/".rr-source.iso";
                std::ofstream stream(cache,std::ios::binary);
                std::array<uint8_t,65536> buffer;
                uint64_t copied=0;
                for (;;) {
                    if (progress) progress({0,hashes.size(),"Reading disc image"});
                    const size_t n=SDL_ReadIO(iso_io.get(),buffer.data(),buffer.size());
                    if (n) {
                        copied+=n;
                        if (copied>4ull*1024*1024*1024) throw std::runtime_error("Disc image exceeds import limit");
                        stream.write(reinterpret_cast<const char*>(buffer.data()),std::streamsize(n));
                        if (!stream) throw std::runtime_error("Cannot cache disc image");
                    }
                    if (n<buffer.size()) {
                        const auto status=SDL_GetIOStatus(iso_io.get());
                        if (status==SDL_IO_STATUS_EOF) break;
                        if (status!=SDL_IO_STATUS_READY || !n) throw std::runtime_error("Cannot read disc image stream");
                    }
                }
                stream.close();
                if (!stream) throw std::runtime_error("Cannot finish disc image cache");
                iso_io.reset(SDL_IOFromFile(cache.string().c_str(),"rb"));
                if (!iso_io) throw std::runtime_error("Cannot reopen cached disc image");
            }
            iso=std::make_unique<Iso9660>([&](uint64_t offset,uint8_t* out,size_t size) {
                if (offset>INT64_MAX || SDL_SeekIO(iso_io.get(),Sint64(offset),SDL_IO_SEEK_SET)!=Sint64(offset)) return size_t(0);
                return SDL_ReadIO(iso_io.get(),out,size);
            });
        }
        size_t done=0;
        for (const auto& entry:hashes) {
            if (progress) progress({done,hashes.size(),entry.path});
            std::vector<uint8_t> bytes;
            try { bytes=folder ? load_file(source_root/entry.path) : iso->read("ROADRASH/"+entry.path); }
            catch (const std::exception& e) {
                result.missing.push_back(entry.path+": "+e.what());
                ++done;
                continue;
            }
            if (sha256(bytes)!=entry.hash) { result.mismatched.push_back(entry.path); ++done; continue; }
            const auto output=staging/entry.path;
            fs::create_directories(output.parent_path());
            std::ofstream stream(output,std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
            stream.close();
            if (!stream) throw std::runtime_error("Cannot write imported file: "+entry.path);
            ++done;
        }
        if (progress) progress({done,hashes.size(),{}});
        if (result.missing.empty() && result.mismatched.empty()) {
            iso.reset();
            iso_io.reset();
            fs::remove(staging/".rr-source.iso");
            std::ofstream marker(staging/".rr-import",std::ios::binary);
            marker << "rr-import 1\n" << hashes.size() << '\n';
            marker.close();
            if (!marker) throw std::runtime_error("Cannot write import marker");
            if (fs::exists(destination)) fs::remove(destination); // previously checked empty; remove refuses a raced-in nonempty directory
            fs::rename(staging,destination);
            staging.clear();
            result.ok=true;
        }
    } catch (const std::exception& e) { result.error=e.what(); }
    if (!staging.empty()) { std::error_code error; fs::remove_all(staging,error); }
    return result;
}
}
