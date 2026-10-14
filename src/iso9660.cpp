#include "iso9660.hpp"
#include "byte_reader.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>
#include <stdexcept>
namespace rr {
namespace {
uint32_t both32(const ByteReader& r, size_t off) {
    const uint32_t value = r.le<uint32_t>(off);
    const uint32_t other = uint32_t(r.bytes(off + 4, 1)[0]) << 24 | uint32_t(r.bytes(off + 5, 1)[0]) << 16 |
                           uint32_t(r.bytes(off + 6, 1)[0]) << 8 | uint32_t(r.bytes(off + 7, 1)[0]);
    if (value != other) throw std::runtime_error("ISO inconsistent dual-endian field");
    return value;
}
}
Iso9660::Iso9660(ReadAt reader) : read_at_(std::move(reader)) {
    std::vector<uint8_t> pvd(2048);
    bool found=false;
    for (uint64_t sector=16;sector<64;++sector) {
        if (read_at_(sector*2048,pvd.data(),pvd.size())!=pvd.size()) throw std::runtime_error("Truncated ISO descriptor");
        if (std::memcmp(pvd.data()+1,"CD001",5) || pvd[6]!=1) throw std::runtime_error("Not an ISO 9660 image");
        if (pvd[0]==255) break;
        if (pvd[0]!=1) continue;
        if (pvd[128]!=0 || pvd[129]!=8 || pvd[130]!=8 || pvd[131]!=0) throw std::runtime_error("ISO block size must be 2048");
        volume_size_=uint64_t(both32(ByteReader(pvd),80))*2048;
        uint8_t last=0;
        if (!volume_size_ || read_at_(volume_size_-1,&last,1)!=1) throw std::runtime_error("ISO declared volume is truncated");
        root_=record(pvd.data()+156,pvd.size()-156);
        if (!root_.directory) throw std::runtime_error("ISO root is not a directory");
        found=true;
        break;
    }
    if (!found) throw std::runtime_error("ISO primary volume descriptor missing");
}
IsoRecord parse_iso_record(std::span<const uint8_t> record) {
    ByteReader r(record);
    const size_t available = r.size();
    if (available < 34) throw std::runtime_error("Malformed ISO directory record");
    const uint8_t len = r.bytes(0, 1)[0], flags = r.bytes(25, 1)[0];
    const uint8_t namelen = r.bytes(32, 1)[0];
    if (len < 34 || len > available || namelen > len - 33 || !namelen)
        throw std::runtime_error("Malformed ISO directory record");
    if (r.bytes(1, 1)[0] || r.bytes(26, 1)[0] || r.bytes(27, 1)[0] || (flags & 0x80))
        throw std::runtime_error("Unsupported ISO interleaving or multi-extent file");
    const auto namebytes = r.bytes(33, namelen);
    IsoRecord e{std::string(reinterpret_cast<const char*>(namebytes.data()), namelen), both32(r, 2),
                both32(r, 10), (flags & 2) != 0};
    const auto semicolon = e.name.find(';');
    if (semicolon != std::string::npos) e.name.resize(semicolon);
    return e;
}
Iso9660::Entry Iso9660::record(const uint8_t* p, size_t available) const {
    const IsoRecord r = parse_iso_record({p, available});
    Entry e{r.name, r.sector, r.size, r.directory};
    const uint64_t offset=uint64_t(e.sector)*2048;
    if (offset>volume_size_ || e.size>volume_size_-offset) throw std::runtime_error("ISO extent outside volume");
    return e;
}
std::vector<uint8_t> Iso9660::load(const Entry& e) const {
    if (e.size>256*1024*1024 || (e.directory && e.size>16*1024*1024)) throw std::runtime_error("ISO extent too large");
    std::vector<uint8_t> data(e.size);
    if (read_at_(uint64_t(e.sector)*2048,data.data(),data.size())!=data.size()) throw std::runtime_error("Truncated ISO extent");
    return data;
}
std::vector<Iso9660::Entry> Iso9660::entries(const Entry& directory) const {
    if (!directory.directory) throw std::runtime_error("ISO path is not a directory");
    const auto data=load(directory);
    std::vector<Entry> result;
    std::set<std::string> names;
    for (size_t at=0;at<data.size();) {
        if (!data[at]) { at=(at/2048+1)*2048; continue; }
        if (data[at]>2048-at%2048) throw std::runtime_error("ISO record crosses sector");
        auto e=record(data.data()+at,data.size()-at);
        at+=data[at];
        if (e.name==std::string(1,'\0') || e.name==std::string(1,'\1')) continue;
        if (e.name.empty() || e.name=="." || e.name==".." || e.name.find_first_of("/\\")!=std::string::npos)
            throw std::runtime_error("Invalid ISO entry name");
        if (!names.insert(e.name).second) throw std::runtime_error("Duplicate ISO name");
        result.push_back(std::move(e));
    }
    return result;
}
Iso9660::Entry Iso9660::locate(const std::string& path) const {
    Entry current=root_;
    std::set<uint32_t> visited{root_.sector};
    size_t at=0;
    while (at<path.size()) {
        const size_t end=path.find('/',at);
        std::string name=path.substr(at,end==std::string::npos ? end : end-at);
        if (name.empty() || name=="." || name==".." || name.find('\\')!=std::string::npos) throw std::runtime_error("Invalid ISO path");
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c) { return char(std::toupper(c)); });
        const auto children=entries(current);
        const auto child=std::find_if(children.begin(),children.end(),[&](const Entry& e) { return e.name==name; });
        if (child==children.end()) throw std::runtime_error("Missing ISO path: "+path);
        current=*child;
        if (current.directory && !visited.insert(current.sector).second) throw std::runtime_error("ISO directory cycle");
        if (end==std::string::npos) break;
        at=end+1;
        if (at==path.size()) throw std::runtime_error("Invalid trailing ISO slash");
    }
    return current;
}
std::vector<std::string> Iso9660::list(const std::string& path) const {
    std::vector<std::string> result;
    for (const auto& e:entries(locate(path))) result.push_back(e.name);
    return result;
}
std::vector<uint8_t> Iso9660::read(const std::string& path) const {
    const auto e=locate(path);
    if (e.directory) throw std::runtime_error("Cannot read ISO directory as file");
    return load(e);
}
}
