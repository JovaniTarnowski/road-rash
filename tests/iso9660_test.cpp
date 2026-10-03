#include "iso9660.hpp"
#include "iso_fixture.hpp"
#include <iostream>
#include <stdexcept>
int main() {
    int failures=0;
    auto check=[&](bool yes,const char* what) { if (!yes) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    auto bytes=fixture::iso();
    auto open=[&]() { return rr::Iso9660([&](uint64_t at,uint8_t* out,size_t n) { return fixture::read_at(bytes,at,out,n); }); };
    const auto iso=open();
    check(iso.list("ROADRASH")==std::vector<std::string>{"A.BIN"},"file version suffix stripped");
    check(iso.read("roadrash/a.bin")==std::vector<uint8_t>({'h','e','l','l','o'}),"case-normalized lookup and contents");
    for (const char* path:{"ROADRASH/NOPE","../ROADRASH/A.BIN","ROADRASH/../A.BIN","/ROADRASH/A.BIN","ROADRASH/A.BIN/", "ROADRASH"}) {
        try { iso.read(path); check(false,"missing/unsafe path throws"); } catch (const std::runtime_error&) {}
    }
    for (size_t length=0;length<bytes.size();++length) {
        try {
            rr::Iso9660 short_iso([&](uint64_t at,uint8_t* out,size_t n) {
                if (at>length || n>length-size_t(at)) return size_t(0);
                return fixture::read_at(bytes,at,out,n);
            });
            check(false,"truncated volume rejected"); break;
        } catch (const std::runtime_error&) {}
    }
    fixture::both32(bytes,19*2048+2,99);
    try { open().read("ROADRASH/A.BIN"); check(false,"out-of-volume extent rejected"); } catch (const std::runtime_error&) {}
    bytes=fixture::iso(); bytes[19*2048+6]^=1;
    try { open().list("ROADRASH"); check(false,"inconsistent byte order rejected"); } catch (const std::runtime_error&) {}
    bytes=fixture::iso(); fixture::record(bytes,19*2048,"LOOP",18,2048,true);
    try { open().list("ROADRASH/LOOP"); check(false,"directory cycle rejected"); } catch (const std::runtime_error&) {}
    bytes=fixture::iso(); bytes[19*2048]=33;
    try { open().list("ROADRASH"); check(false,"short directory record rejected"); } catch (const std::runtime_error&) {}
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
