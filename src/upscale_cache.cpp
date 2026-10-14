#include "atomic_file.hpp"
#include "upscale_cache.hpp"
#include "sha256.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace rr {
namespace {
uint32_t read_u32(const std::vector<uint8_t>& b,size_t& p) {
    if (b.size()-p<4) throw std::runtime_error("Truncated rrup");
    uint32_t v=uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);
    p+=4;return v;
}
void write_u32(std::vector<uint8_t>& b,uint32_t v) {
    b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));b.push_back(uint8_t(v>>16));b.push_back(uint8_t(v>>24));
}
std::string method_name(UpscaleMethod m) { return m==UpscaleMethod::ai?"ai":"mmpx"; }
bool parse_method(const std::string& s,UpscaleMethod& m) {
    if (s=="ai") { m=UpscaleMethod::ai;return true; }
    if (s=="mmpx") { m=UpscaleMethod::mmpx;return true; }
    return false;
}
// Cache keys include source content hash, resource/subframe identity,
// method/version, palette/remap policy, alpha policy and scale.
std::filesystem::path entry_path(const std::string& root,const std::string& asset,
    uint32_t index,int scale,UpscaleMethod m) {
    if (asset.empty() || asset.size()>256 || scale<1 || scale>4)
        throw std::runtime_error("Invalid upscale cache key");
    for (char c:asset)
        if (c=='\0'||c=='/'||c=='\\'||c==':'||(unsigned char)c<32) {
            if (c!='/'&&c!='\\') throw std::runtime_error("Invalid asset name in cache key");
        }
    std::filesystem::path p(root);
    p/=method_name(m);p/=std::to_string(scale);
    // Reject path traversal: asset is a single path element below.
    std::string safe;
    for (char c:asset) safe += (c=='/'||c=='\\') ? '_' : c;
    if (safe=="."||safe==".."||safe.find("..")!=std::string::npos)
        throw std::runtime_error("Invalid asset name in cache key");
    p/=safe;safe.clear();
    p/=std::to_string(index)+".rrup";
    return p;
}
std::vector<uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary);
    if (!in) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}
} // namespace
std::array<uint8_t,32> source_hash32(const std::vector<uint8_t>& source) {
    const std::string hex=sha256(source);
    if (hex.size()!=64) throw std::runtime_error("SHA-256 hex length");
    std::array<uint8_t,32> out{};
    for (int i=0;i<32;++i)
        out[size_t(i)]=uint8_t(std::stoul(hex.substr(size_t(i)*2,2),nullptr,16));
    return out;
}
std::vector<uint8_t> encode_rrup(UpscaleMethod m,uint32_t method_version,
    const std::array<uint8_t,32>& hash,const UpscaledImage& img) {
    if (img.width<=0||img.height<=0||img.width>8192||img.height>8192)
        throw std::runtime_error("Invalid upscaled image dimensions");
    const size_t n=size_t(img.width)*size_t(img.height);
    if (img.indexed) { if (img.indices.size()!=n) throw std::runtime_error("Upscaled index size mismatch"); }
    else if (img.rgb.size()!=n) throw std::runtime_error("Upscaled rgb size mismatch");
    // Cap memory/cache growth: single entry bounded before allocation.
    if (n>size_t(8192)*8192) throw std::runtime_error("Upscaled image too large");
    std::vector<uint8_t> b;
    b.insert(b.end(),{'R','R','U','P'});
    write_u32(b,1);
    b.push_back(uint8_t(m));write_u32(b,method_version);
    b.insert(b.end(),hash.begin(),hash.end());
    write_u32(b,uint32_t(img.width));write_u32(b,uint32_t(img.height));
    b.push_back(img.indexed?1:0);
    if (img.indexed) b.insert(b.end(),img.indices.begin(),img.indices.end());
    else for (uint32_t px:img.rgb) write_u32(b,px);
    return b;
}
std::optional<UpscaledImage> decode_rrup(const std::vector<uint8_t>& bytes,
    UpscaleMethod* method_out,uint32_t* method_version_out,
    std::array<uint8_t,32>* hash_out) {
    size_t p=0;
    auto fail=[]{return std::optional<UpscaledImage>{};};
    if (bytes.size()<4+p||bytes[0]!='R'||bytes[1]!='R'||bytes[2]!='U'||bytes[3]!='P') return fail();
    p=4;
    try {
        if (read_u32(bytes,p)!=1) return fail();
        if (p>=bytes.size()) return fail();
        const uint8_t m=bytes[p++];
        if (m>1) return fail();
        const uint32_t mv=read_u32(bytes,p);
        if (bytes.size()-p<32) return fail();
        std::array<uint8_t,32> h{};
        for (int i=0;i<32;++i) h[size_t(i)]=bytes[p++];
        const uint32_t w=read_u32(bytes,p),hh=read_u32(bytes,p);
        if (w==0||hh==0||w>8192||hh>8192) return fail();
        if (p>=bytes.size()) return fail();
        const uint8_t idx=bytes[p++];
        if (idx>1) return fail();
        const size_t n=size_t(w)*size_t(hh);
        UpscaledImage img;img.width=int(w);img.height=int(hh);img.indexed=(idx==1);
        if (img.indexed) {
            if (bytes.size()-p!=n) return fail();
            img.indices.assign(bytes.begin()+long(p),bytes.end());
        } else {
            if (bytes.size()-p!=n*4) return fail();
            img.rgb.resize(n);
            for (size_t i=0;i<n;++i) img.rgb[i]=read_u32(bytes,p);
            if (p!=bytes.size()) return fail();
        }
        if (method_out) *method_out=UpscaleMethod(m);
        if (method_version_out) *method_version_out=mv;
        if (hash_out) *hash_out=h;
        return img;
    } catch (const std::runtime_error&) { return fail(); }
}
std::array<uint8_t, 32> hash_source32(const std::vector<uint8_t>& source) { return source_hash32(source); }
UpscaleCache::UpscaleCache(std::string root):root_(std::move(root)) {}
std::optional<UpscaledImage> UpscaleCache::find(const std::string& asset,uint32_t index,int scale,
    const std::vector<uint8_t>& source,std::initializer_list<UpscaleMethod> methods) const {
    const auto want=source_hash32(source);
    for (UpscaleMethod m:methods) {
        std::filesystem::path p;
        try { p=entry_path(root_,asset,index,scale,m); }
        catch (const std::runtime_error&) { throw; }
        const auto bytes=read_file(p);
        if (bytes.empty()) continue;
        UpscaleMethod got{};uint32_t mv=0;std::array<uint8_t,32> h{};
        auto img=decode_rrup(bytes,&got,&mv,&h);
        if (!img||got!=m||h!=want) continue;
        const uint32_t current=(m==UpscaleMethod::mmpx)?mmpx_version:1;
        if (mv!=current) continue; // stale method version: ignore, never show
        return img;
    }
    return std::nullopt;
}
std::optional<UpscaledImage> UpscaleCache::find_by_content(const std::vector<uint8_t>& source, int scale,
    std::initializer_list<UpscaleMethod> methods) const {
    const auto want = source_hash32(source);
    for (UpscaleMethod m : methods) {
        const std::string mdir = m == UpscaleMethod::mmpx ? "mmpx" : "ai";
        std::error_code ec;
        const std::filesystem::path base = std::filesystem::path(root_) / mdir / std::to_string(scale);
        if (!std::filesystem::is_directory(base, ec)) continue;
        for (std::filesystem::recursive_directory_iterator it(base, ec), end; it != end; it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file(ec) || it->path().extension() != ".rrup") continue;
            const auto bytes = read_file(it->path());
            if (bytes.empty()) continue;
            UpscaleMethod got{};
            uint32_t mv = 0;
            std::array<uint8_t, 32> h{};
            auto img = decode_rrup(bytes, &got, &mv, &h);
            if (!img || got != m || h != want) continue;
            const uint32_t current = (m == UpscaleMethod::mmpx) ? mmpx_version : 1;
            if (mv != current) continue;
            return img;
        }
    }
    return std::nullopt;
}
std::map<std::pair<std::array<uint8_t, 32>, int>, UpscaledImage> UpscaleCache::content_index(
    std::initializer_list<int> scales, std::initializer_list<UpscaleMethod> methods) const {
    std::map<std::pair<std::array<uint8_t, 32>, int>, UpscaledImage> out;
    for (UpscaleMethod m : methods) {
        const std::string mdir = m == UpscaleMethod::mmpx ? "mmpx" : "ai";
        for (int scale : scales) {
            std::error_code ec;
            const std::filesystem::path base = std::filesystem::path(root_) / mdir / std::to_string(scale);
            if (!std::filesystem::is_directory(base, ec)) continue;
            for (std::filesystem::recursive_directory_iterator it(base, ec), end; it != end; it.increment(ec)) {
                if (ec) break;
                if (!it->is_regular_file(ec) || it->path().extension() != ".rrup") continue;
                const auto bytes = read_file(it->path());
                if (bytes.empty()) continue;
                UpscaleMethod got{};
                uint32_t mv = 0;
                std::array<uint8_t, 32> h{};
                auto img = decode_rrup(bytes, &got, &mv, &h);
                if (!img || got != m) continue;
                const uint32_t current = (m == UpscaleMethod::mmpx) ? mmpx_version : 1;
                if (mv != current) continue;
                out.emplace(std::make_pair(h, scale), *img);
            }
        }
    }
    return out;
}
bool UpscaleCache::store(const std::string& asset,uint32_t index,int scale,
    const std::vector<uint8_t>& source,UpscaleMethod m,const UpscaledImage& img) const {
    try {
        const auto p=entry_path(root_,asset,index,scale,m);
        const uint32_t mv=(m==UpscaleMethod::mmpx)?mmpx_version:1;
        const auto bytes=encode_rrup(m,mv,source_hash32(source),img);
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(),ec);
        if (ec) return false;
        if (!write_file_atomic(p, bytes.data(), bytes.size())) return false;
        return true;
    } catch (...) { return false; }
}
// ---- Minimal stored-deflate ZIP reader for packs (plan 20 Task 5) ----
std::vector<std::pair<std::string,std::vector<uint8_t>>> unzip_checked(
    const std::vector<uint8_t>& zip,size_t max_total) {
    if (zip.size()<22) throw std::runtime_error("ZIP too small");
    // Find end-of-central-directory (no comment search beyond 64k).
    size_t eocd=zip.size();
    bool found=false;
    const size_t lo=zip.size()>65557?zip.size()-65557:0;
    for (size_t i=zip.size()-22;;--i) {
        if (zip[i]=='P'&&zip[i+1]=='K'&&zip[i+2]==5&&zip[i+3]==6) { eocd=i;found=true;break; }
        if (i==lo) break;
    }
    if (!found) throw std::runtime_error("ZIP end record missing");
    size_t p=eocd+4;
    auto u16=[&]{ if (p+2>zip.size()) throw std::runtime_error("Truncated ZIP end");uint16_t v=uint16_t(zip[p]|(zip[p+1]<<8));p+=2;return v; };
    auto u32=[&]{ if (p+4>zip.size()) throw std::runtime_error("Truncated ZIP end");uint32_t v=uint32_t(zip[p])|(uint32_t(zip[p+1])<<8)|(uint32_t(zip[p+2])<<16)|(uint32_t(zip[p+3])<<24);p+=4;return v; };
    (void)u16();(void)u16();
    const uint16_t entries=u16(),entries_total=u16();
    (void)u32();const uint32_t cd_off=u32();(void)u16();
    if (entries!=entries_total) throw std::runtime_error("ZIP multi-disk unsupported");
    size_t c=cd_off;
    auto cu16=[&]{ if (c+2>zip.size()) throw std::runtime_error("Truncated ZIP directory");uint16_t v=uint16_t(zip[c]|(zip[c+1]<<8));c+=2;return v; };
    auto cu32=[&]{ if (c+4>zip.size()) throw std::runtime_error("Truncated ZIP directory");uint32_t v=uint32_t(zip[c])|(uint32_t(zip[c+1])<<8)|(uint32_t(zip[c+2])<<16)|(uint32_t(zip[c+3])<<24);c+=4;return v; };
    std::vector<std::pair<std::string,std::vector<uint8_t>>> out;
    size_t total=0;
    for (uint16_t e=0;e<entries;++e) {
        if (c+46>zip.size()) throw std::runtime_error("Truncated ZIP entry");
        if (!(zip[c]=='P'&&zip[c+1]=='K'&&zip[c+2]==1&&zip[c+3]==2)) throw std::runtime_error("Bad ZIP entry magic");
        c+=4;(void)cu16();(void)cu16();(void)cu16();(void)cu16();(void)cu16();(void)cu16();
        const uint32_t crc=cu32(),comp=cu32(),uncomp=cu32();
        const uint16_t nlen=cu16(),xlen=cu16(),clen=cu16();
        (void)cu16();(void)cu16();(void)cu32();const uint32_t local_off=cu32();
        if (c+nlen+xlen+clen>zip.size()) throw std::runtime_error("Truncated ZIP name");
        const std::string name(zip.begin()+long(c),zip.begin()+long(c+nlen));
        c+=nlen+xlen+clen;
        if (name.empty()||name[0]=='/'||name.find("..")!=std::string::npos)
            throw std::runtime_error("ZIP path traversal rejected");
        for (auto& kv:out) if (kv.first==name) throw std::runtime_error("ZIP duplicate name rejected");
        if (uncomp>max_total) throw std::runtime_error("ZIP entry exceeds inflated bound");
        // Local header.
        size_t l=local_off;
        if (l+30>zip.size()||!(zip[l]=='P'&&zip[l+1]=='K'&&zip[l+2]==3&&zip[l+3]==4))
            throw std::runtime_error("Bad ZIP local header");
        const uint16_t method=uint16_t(zip[l+8]|(zip[l+9]<<8));
        const uint16_t ln=uint16_t(zip[l+26]|(zip[l+27]<<8)),lx=uint16_t(zip[l+28]|(zip[l+29]<<8));
        const size_t data=l+30+ln+lx;
        if (data+comp>zip.size()) throw std::runtime_error("Truncated ZIP data");
        std::vector<uint8_t> raw;
        if (method==0) {
            raw.assign(zip.begin()+long(data),zip.begin()+long(data+comp));
            if (raw.size()!=uncomp) throw std::runtime_error("ZIP stored size mismatch");
        } else if (method==8) {
            // Inflate raw deflate (stored, fixed and dynamic Huffman) with
            // bounded work: output capped at uncomp, blocks capped, lengths
            // capped. Throws std::runtime_error on any malformation.
            raw.reserve(std::min<size_t>(uncomp,1<<20));
            size_t bp=data;uint32_t hold=0;int bits=0;
            auto need=[&](int n){ while(bits<n){ if(bp>=data+comp) throw std::runtime_error("Truncated deflate");hold|=uint32_t(zip[bp++])<<bits;bits+=8; } };
            auto take=[&](int n){ need(n);uint32_t v=(n==32)?hold:(hold&((n==1)?1u:((1u<<n)-1)));hold>>=n;bits-=n;return v; };
            struct Decoder { std::vector<int> counts{std::vector<int>(16,0)},symbols; std::vector<int> start{std::vector<int>(16,0)}; };
            auto build=[](const std::vector<int>& lens){ Decoder d;const int n=int(lens.size());d.symbols.reserve(size_t(n));std::vector<std::vector<int>> by(16);for(int i=0;i<n;++i)if(lens[size_t(i)]>0&&lens[size_t(i)]<16){d.counts[size_t(lens[size_t(i)])]++;by[size_t(lens[size_t(i)])].push_back(i);}int code=0;for(int len=1;len<16;++len){d.start[size_t(len)]=code;code+=d.counts[size_t(len)];code<<=1;}for(int len=1;len<16;++len)for(int s:by[size_t(len)])d.symbols.push_back(s);return d; };
            auto decode=[&](const Decoder& d){ int code=0;for(int len=1;len<16;++len){ need(len);code=int(hold&((len==32)?0xffffffffu:((1u<<len)-1)));const int c=d.counts[size_t(len)];if(code-d.start[size_t(len)]<c){ hold>>=len;bits-=len;int off=0;for(int k=1;k<len;++k)off+=d.counts[size_t(k)];return d.symbols[size_t(off+code-d.start[size_t(len)])]; } } throw std::runtime_error("Bad Huffman code"); };
            auto fixed_lit=build([]{ std::vector<int> v(288,0);for(int i=0;i<144;++i)v[size_t(i)]=8;for(int i=144;i<256;++i)v[size_t(i)]=9;for(int i=256;i<280;++i)v[size_t(i)]=7;for(int i=280;i<288;++i)v[size_t(i)]=8;return v; }());
            auto fixed_dist=build(std::vector<int>(32,5));
            static const int len_base[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
            static const int len_extra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
            static const int dist_base[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
            static const int dist_extra[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
            bool last=false;int guard=0;
            auto run_codes=[&](const Decoder& lit,const Decoder& dist){
                for(;;){
                    if(raw.size()>uncomp) throw std::runtime_error("ZIP inflated size mismatch");
                    const int s=decode(lit);
                    if(s<256){ raw.push_back(uint8_t(s)); }
                    else if(s==256) return;
                    else{
                        const int li=s-257;if(li<0||li>=29) throw std::runtime_error("Bad length code");
                        size_t len=size_t(len_base[li])+take(len_extra[li]);
                        const int ds=decode(dist);if(ds<0||ds>=30) throw std::runtime_error("Bad distance code");
                        size_t d=size_t(dist_base[ds])+take(dist_extra[ds]);
                        if(d==0||d>raw.size()) throw std::runtime_error("Bad deflate distance");
                        if(raw.size()+len>uncomp) throw std::runtime_error("ZIP inflated size mismatch");
                        for(size_t k=0;k<len;++k) raw.push_back(raw[raw.size()-d]);
                    }
                }
            };
            while(!last) {
                if(++guard>1000) throw std::runtime_error("Deflate block limit");
                last=take(1)!=0;const unsigned type=take(2);
                if(type==0){ hold=0;bits=0;size_t bl=bp;if(bl+4>data+comp) throw std::runtime_error("Truncated stored block");const uint16_t len=uint16_t(zip[bl]|(zip[bl+1]<<8));if((len^uint16_t(zip[bl+2]|(zip[bl+3]<<8)))!=0xffff) throw std::runtime_error("Bad stored block");bl+=4;if(bl+len>data+comp) throw std::runtime_error("Truncated stored block");if(raw.size()+len>uncomp) throw std::runtime_error("ZIP inflated size mismatch");raw.insert(raw.end(),zip.begin()+long(bl),zip.begin()+long(bl+len));bp=bl+len; }
                else if(type==1){ run_codes(fixed_lit,fixed_dist); }
                else if(type==2){
                    const int hlit=int(take(5))+257,hdist=int(take(5))+1,hcl=int(take(4))+4;
                    static const int ord[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                    std::vector<int> cl(19,0);for(int i=0;i<hcl;++i)cl[size_t(ord[i])]=int(take(3));
                    Decoder cld=build(cl);
                    std::vector<int> lens;lens.reserve(size_t(hlit+hdist));
                    while(int(lens.size())<hlit+hdist){ const int s=decode(cld);if(s<16)lens.push_back(s);else if(s==16){ if(lens.empty()) throw std::runtime_error("Bad repeat code");const int r=int(take(2))+3,v=lens.back();for(int k=0;k<r;++k)lens.push_back(v);}else if(s==17){const int r=int(take(3))+3;for(int k=0;k<r;++k)lens.push_back(0);}else{const int r=int(take(7))+11;for(int k=0;k<r;++k)lens.push_back(0);} if(lens.size()>size_t(hlit+hdist)) throw std::runtime_error("Too many code lengths"); }
                    std::vector<int> ll(lens.begin(),lens.begin()+hlit),dl(lens.begin()+hlit,lens.end());
                    run_codes(build(ll),build(dl));
                } else throw std::runtime_error("Bad deflate type");
            }
            if(raw.size()!=uncomp) throw std::runtime_error("ZIP inflated size mismatch");
            (void)crc;
        } else throw std::runtime_error("ZIP unsupported compression");
        total+=raw.size();
        if (total>max_total) throw std::runtime_error("ZIP total inflated size exceeds bound");
        // Symlinks in packs rejected: names ending .lnk or with link marker.
        if (name.size()>4&&name.compare(name.size()-4,4,".lnk")==0)
            throw std::runtime_error("ZIP symlink rejected");
        out.emplace_back(name,std::move(raw));
    }
    return out;
}
UpscaleCache::PackResult UpscaleCache::import_pack(const std::vector<uint8_t>& zip,
    const std::function<std::vector<uint8_t>(const std::string&,uint32_t)>& expected_source) const {
    PackResult r;
    auto entries=unzip_checked(zip);
    for (auto& kv:entries) {
        // Pack layout mirrors entry_path: <method>/<scale>/<asset>/<index>.rrup
        UpscaleMethod m;int scale=0;std::string asset;uint32_t index=0;
        size_t a=kv.first.find('/'),b=kv.first.find('/',a==std::string::npos?0:a+1);
        size_t c2=kv.first.rfind('/');
        if (a==std::string::npos||b==std::string::npos||c2==std::string::npos||c2<=b) { ++r.skipped;continue; }
        if (!parse_method(kv.first.substr(0,a),m)) { ++r.skipped;continue; }
        try { scale=std::stoi(kv.first.substr(a+1,b-a-1)); } catch (...) { ++r.skipped;continue; }
        asset=kv.first.substr(b+1,c2-b-1);
        const std::string tail=kv.first.substr(c2+1);
        if (tail.size()<6||tail.compare(tail.size()-5,5,".rrup")!=0) { ++r.skipped;continue; }
        try { index=uint32_t(std::stoul(tail.substr(0,tail.size()-5))); } catch (...) { ++r.skipped;continue; }
        UpscaleMethod got{};uint32_t mv=0;std::array<uint8_t,32> h{};
        auto img=decode_rrup(kv.second,&got,&mv,&h);
        if (!img||got!=m) { ++r.skipped;continue; }
        const auto expect=expected_source(asset,index);
        if (source_hash32(expect)!=h) { ++r.skipped;continue; }
        if (!store(asset,index,scale,expect,m,*img)) ++r.skipped; else ++r.imported;
    }
    return r;
}
}
