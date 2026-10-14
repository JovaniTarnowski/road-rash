// Plan 22 Task 2 Step 2: writes synthetic valid seeds for every fuzz target
// into <root>/<target>/ (default fuzz/seeds). Every builder below mirrors the
// hand-made fixtures the unit tests use (tests/resource_test.cpp,
// tests/wav_test.cpp, tests/iso_fixture.hpp, ...), so the seeds contain no
// original data. Each seed is re-parsed after writing; any mismatch fails the
// program so tools/fuzz.sh never fuzzes from corrupt seeds. Output is fully
// deterministic, so re-running never dirties the tree.
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "animation.hpp"
#include "avi.hpp"
#include "bob.hpp"
#include "career.hpp"
#include "cel.hpp"
#include "explode.hpp"
#include "family.hpp"
#include "ima_adpcm.hpp"
#include "iso9660.hpp"
#include "pe_strings.hpp"
#include "resource.hpp"
#include "rri.hpp"
#include "settings.hpp"
#include "terrain.hpp"
#include "wav.hpp"
#include "../tests/iso_fixture.hpp"

namespace {
using Bytes = std::vector<uint8_t>;
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++failures; std::cerr << "FAIL " << what << '\n'; }
}
void word(Bytes& b, size_t p, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[p + i] = uint8_t(v >> (8 * i));
}
void tag_at(Bytes& b, size_t p, const char* s) {
    for (int i = 0; i < 4; ++i) b[p + i] = uint8_t(s[i]);
}
void write_seed(const std::filesystem::path& root, const char* target, const char* name, const Bytes& b) {
    const auto dir = root / target;
    std::filesystem::create_directories(dir);
    std::ofstream out(dir / name, std::ios::binary);
    if (!out) { ++failures; std::cerr << "FAIL cannot write " << (dir / name) << '\n'; return; }
    out.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
    if (!out) { ++failures; std::cerr << "FAIL short write " << (dir / name) << '\n'; }
}
void write_seed(const std::filesystem::path& root, const char* target, const char* name, const std::string& s) {
    write_seed(root, target, name, Bytes(s.begin(), s.end()));
}

// --- builders mirrored from the unit tests --------------------------------

// tests/resource_test.cpp: minimal CRSR/LBTR directory, one SPEC entry.
Bytes resource_container() {
    Bytes b(112);
    tag_at(b, 0, "CRSR"); tag_at(b, 12, "LBTR"); word(b, 16, 48);
    tag_at(b, 48, "LBTR"); word(b, 56, 1);
    tag_at(b, 64, "CEPS"); word(b, 68, 7); word(b, 72, 96); word(b, 76, 16);
    return b;
}

// tests/wav_test.cpp builder.
void wav_u16(Bytes& b, uint16_t n) { b.push_back(uint8_t(n)); b.push_back(uint8_t(n >> 8)); }
void wav_u32(Bytes& b, uint32_t n) { wav_u16(b, uint16_t(n)); wav_u16(b, uint16_t(n >> 16)); }
void wav_tag(Bytes& b, const char* text) { b.insert(b.end(), text, text + 4); }
Bytes wav_bytes(int channels, int bits, const Bytes& data, bool junk = false) {
    Bytes b;
    wav_tag(b, "RIFF"); wav_u32(b, 0); wav_tag(b, "WAVE");
    if (junk) { wav_tag(b, "JUNK"); wav_u32(b, 1); b.push_back(7); b.push_back(0); }
    wav_tag(b, "fmt "); wav_u32(b, 16); wav_u16(b, 1); wav_u16(b, uint16_t(channels));
    wav_u32(b, uint32_t(11025 * channels * bits / 8));
    wav_u32(b, uint32_t(11025 * channels * bits / 8)); wav_u16(b, uint16_t(channels * bits / 8));
    wav_u16(b, uint16_t(bits));
    wav_tag(b, "data"); wav_u32(b, uint32_t(data.size()));
    b.insert(b.end(), data.begin(), data.end());
    if (data.size() % 2) b.push_back(0);
    const uint32_t size = uint32_t(b.size() - 8);
    for (int i = 0; i < 4; ++i) b[4 + i] = uint8_t(size >> (i * 8));
    return b;
}

// tests/cel_test.cpp: 176-byte CCB with PLUT + PDAT.
Bytes cel_bytes() {
    Bytes b(176);
    word(b, 0, 0x43434220); word(b, 4, 80); word(b, 72, 0x80000002); word(b, 76, 2);
    word(b, 80, 0x504c5554); word(b, 84, 16); word(b, 88, 2);
    b[92] = 0x34; b[93] = 0x12; b[94] = 0x78; b[95] = 0x56;
    word(b, 96, 0x50444154); word(b, 100, 80); word(b, 104, 0xbc614e);
    word(b, 108, 0xfffeffff); word(b, 112, 0x00010003);
    word(b, 136, 4); word(b, 140, 2); word(b, 144, 1); word(b, 160, 36);
    for (int i = 0; i < 8; ++i) b[168 + i] = uint8_t(i);
    return b;
}

// tests/family_test.cpp: 384-byte cel group + 16-byte packed wrapper.
Bytes family_group() {
    Bytes b(384);
    word(b, 0, 0x434c4750); word(b, 4, 36); word(b, 8, 52); word(b, 12, 120); word(b, 16, 128);
    b[20] = 0x81; b[21] = 7; b[22] = 10; b[24] = 2;
    for (unsigned i = 0; i < 3; ++i) {
        b[40 + 2 * i] = uint8_t(4u << i); b[41 + 2 * i] = uint8_t(2u << i);
        const size_t p = 160 + 76 * i;
        word(b, 28 + 4 * i, uint32_t(p - 8)); word(b, p, 0xbc614e);
        word(b, p + 8, 0x00020004); word(b, p + 32, 4); word(b, p + 36, 2);
        word(b, p + 40, 1); word(b, p + 56, 36);
        for (unsigned j = 0; j < 8; ++j) b[p + 64 + j] = uint8_t(i * 10 + j);
    }
    return b;
}
Bytes family_packed() {
    const Bytes b = family_group();
    Bytes result(16);
    word(result, 0, uint32_t(b.size())); word(result, 4, uint32_t(b.size()));
    result.insert(result.end(), b.begin(), b.end());
    return result;
}

// tests/terrain_test.cpp kind 0 (DLBR) fixture.
Bytes terrain_bytes() {
    Bytes b(144);
    for (int i = 0; i < 4; ++i) b[i] = "DLBR"[i];
    word(b, 4, uint32_t(144)); word(b, 8, 64);
    for (int i = 0; i < 64; ++i) { b[16 + 2 * i] = uint8_t(i); b[16 + 2 * i + 1] = uint8_t(63 - i); }
    return b;
}

// tests/animation_test.cpp: 220-byte synthetic RRAN.
Bytes animation_bytes() {
    Bytes s(220);
    word(s, 0, 0x5252414e); word(s, 4, 20); word(s, 8, 56); word(s, 12, 104);
    word(s, 20, 0x52524644); word(s, 24, 36); word(s, 28, 116); word(s, 32, 184);
    word(s, 36, 196); word(s, 40, 204); word(s, 44, 7); word(s, 48, 3); word(s, 52, 4);
    word(s, 56, 0x414e494d); word(s, 60, 48); word(s, 72, 1); word(s, 104, 0x43434220);
    word(s, 184, 0x52525044); word(s, 188, 12); word(s, 192, 0x17001e00);
    word(s, 196, uint32_t(-2)); word(s, 200, 3);
    return s;
}

// tests/avi_test.cpp minimal RIFF (one video stream, two frames).
void avi_u32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (i * 8))); }
Bytes avi_tag(const char* s) { return Bytes(s, s + 4); }
Bytes avi_chunk(const char* id, Bytes data) {
    Bytes b = avi_tag(id);
    avi_u32(b, uint32_t(data.size()));
    b.insert(b.end(), data.begin(), data.end());
    if (data.size() & 1) b.push_back(0);
    return b;
}
void avi_append(Bytes& a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); }
Bytes avi_list(const char* kind, Bytes data) {
    Bytes b = avi_tag(kind);
    avi_append(b, data);
    return avi_chunk("LIST", b);
}
Bytes avi_bytes() {
    Bytes header;
    avi_u32(header, rr::fourcc('v', 'i', 'd', 's')); avi_u32(header, rr::fourcc('c', 'v', 'i', 'd'));
    for (int i = 0; i < 3; ++i) avi_u32(header, 0);
    avi_u32(header, 1); avi_u32(header, 15);
    while (header.size() < 56) avi_u32(header, 0);
    Bytes format;
    avi_u32(format, 40); avi_u32(format, 4); avi_u32(format, 4);
    format.insert(format.end(), {1, 0, 24, 0});
    avi_u32(format, rr::fourcc('c', 'v', 'i', 'd'));
    while (format.size() < 40) avi_u32(format, 0);
    Bytes stream = avi_chunk("strh", header);
    avi_append(stream, avi_chunk("strf", format));
    Bytes root = avi_tag("AVI ");
    avi_append(root, avi_list("hdrl", avi_list("strl", stream)));
    Bytes movie = avi_chunk("00dc", {1, 2, 3});
    avi_append(movie, avi_chunk("00dc", {4, 5}));
    avi_append(root, avi_list("movi", movie));
    return avi_chunk("RIFF", root);
}

// tests/pe_strings_test.cpp make_pe with a single RT_STRING block holding "A".
Bytes pe_bytes() {
    auto put16 = [](Bytes& v, size_t p, uint32_t x) {
        if (v.size() < p + 2) v.resize(p + 2);
        v[p] = uint8_t(x); v[p + 1] = uint8_t(x >> 8);
    };
    auto put32 = [&](Bytes& v, size_t p, uint32_t x) { put16(v, p, x & 0xffff); put16(v, p + 2, x >> 16); };
    const uint32_t section_rva = 0x1000;
    std::array<std::u16string, 16> strings{};
    strings[0] = u"A";
    Bytes rsrc;
    const size_t type_dir = 24, lang_dirs = type_dir + 16 + 8, data_entries = lang_dirs + 24, data = data_entries + 16;
    rsrc.resize(data);
    put16(rsrc, 14, 1); put32(rsrc, 16, 6); put32(rsrc, 20, 0x80000000u | uint32_t(type_dir));
    put16(rsrc, type_dir + 14, 1);
    put32(rsrc, type_dir + 16, 1); put32(rsrc, type_dir + 20, 0x80000000u | uint32_t(lang_dirs));
    put16(rsrc, lang_dirs + 14, 1);
    put32(rsrc, lang_dirs + 16, 0x409); put32(rsrc, lang_dirs + 20, uint32_t(data_entries));
    size_t pos = data;
    for (const auto& s : strings) {
        put16(rsrc, pos, uint32_t(s.size())); pos += 2;
        for (char16_t c : s) { put16(rsrc, pos, c); pos += 2; }
    }
    put32(rsrc, data_entries, section_rva + uint32_t(data)); put32(rsrc, data_entries + 4, uint32_t(pos - data));
    Bytes pe(0x200);
    pe[0] = 'M'; pe[1] = 'Z'; put32(pe, 60, 0x40);
    put32(pe, 0x40, 0x4550); put16(pe, 0x44, 0x14c); put16(pe, 0x46, 1); put16(pe, 0x54, 0xe0);
    const size_t opt = 0x58;
    put16(pe, opt, 0x10b); put32(pe, opt + 92, 16);
    put32(pe, opt + 96 + 16, section_rva); put32(pe, opt + 96 + 20, uint32_t(rsrc.size()));
    const size_t sec = opt + 0xe0;
    put32(pe, sec + 8, uint32_t(rsrc.size())); put32(pe, sec + 12, section_rva);
    put32(pe, sec + 16, uint32_t(rsrc.size())); put32(pe, sec + 20, 0x200);
    pe.insert(pe.end(), rsrc.begin(), rsrc.end());
    return pe;
}

// Synthetic 1x1 mid-grey JPEG (generated locally with Pillow; no game data).
const uint8_t kGrayJpeg[] = {
    255,216,255,224,0,16,74,70,73,70,0,1,1,0,0,1,0,1,0,0,255,219,0,67,0,8,6,6,7,6,5,8,7,7,7,9,9,8,
    10,12,20,13,12,11,11,12,25,18,19,15,20,29,26,31,30,29,26,28,28,32,36,46,39,32,34,44,35,28,28,40,
    55,41,44,48,49,52,52,52,31,39,57,61,56,50,60,46,51,52,50,255,192,0,11,8,0,1,0,1,1,1,17,0,255,196,
    0,31,0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,255,196,0,181,16,0,2,1,3,3,2,
    4,3,5,5,4,4,0,0,1,125,1,2,3,0,4,17,5,18,33,49,65,6,19,81,97,7,34,113,20,50,129,145,161,8,35,66,
    177,193,21,82,209,240,36,51,98,114,130,9,10,22,23,24,25,26,37,38,39,40,41,42,52,53,54,55,56,57,
    58,67,68,69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,115,116,117,
    118,119,120,121,122,131,132,133,134,135,136,137,138,146,147,148,149,150,151,152,153,154,162,163,
    164,165,166,167,168,169,170,178,179,180,181,182,183,184,185,186,194,195,196,197,198,199,200,201,
    202,210,211,212,213,214,215,216,217,218,225,226,227,228,229,230,231,232,233,234,241,242,243,244,
    245,246,247,248,249,250,255,218,0,8,1,1,0,0,63,0,43,255,217
};

// tests/career_test.cpp synthetic tables.
rr::CareerTables career_tables() {
    rr::CareerTables t;
    for (size_t i = 0; i < t.start_cash.size(); ++i) t.start_cash[i] = uint16_t(100 * i);
    for (size_t i = 0; i < t.level_bike.size(); ++i) t.level_bike[i] = uint8_t(i % 15);
    for (size_t i = 0; i < t.bike_price.size(); ++i) t.bike_price[i] = uint32_t(1000 * (i + 1));
    for (size_t i = 0; i < t.prize.size(); ++i) t.prize[i] = uint32_t(1000 - 60 * i);
    for (size_t p = 0; p < 10; ++p)
        for (size_t r = 0; r < 10; ++r) t.default_relations[p][r] = {int32_t(p), int32_t(r), int32_t(p * 10 + r)};
    return t;
}
}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path root = argc > 1 ? argv[1] : "fuzz/seeds";
    const Bytes empty;
    for (const char* target : {"fuzz_settings", "fuzz_resources", "fuzz_unpack", "fuzz_cel", "fuzz_family",
                               "fuzz_course", "fuzz_terrain", "fuzz_cars", "fuzz_animation", "fuzz_bobs",
                               "fuzz_avi", "fuzz_audio", "fuzz_iso", "fuzz_pe", "fuzz_rri", "fuzz_save",
                               "fuzz_exe_tables"})
        write_seed(root, target, "empty", empty);

    {  // settings: formatted defaults round-trip.
        rr::Settings s = rr::default_settings();
        s.alias = "Fuzzer";
        const std::string text = rr::format_settings(s);
        write_seed(root, "fuzz_settings", "seed-1.txt", text);
        const rr::Settings back = rr::parse_settings(text);
        check(back.alias == "Fuzzer" && back.master_volume == s.master_volume, "settings round trip");
    }
    {  // resources: one SPEC entry.
        const Bytes b = resource_container();
        write_seed(root, "fuzz_resources", "seed-1.rsc", b);
        const auto items = rr::read_resources(b);
        check(items.size() == 1 && items[0].tag == "SPEC" && items[0].id == 7, "resource seed");
        write_seed(root, "fuzz_course", "seed-1.crs", b);  // CRS files share the container.
        write_seed(root, "fuzz_cars", "seed-1.car", b);    // CAR files share the container.
    }
    {  // unpack: stored (uncompressed) resources, 4 bytes and empty.
        Bytes raw = {4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4};
        write_seed(root, "fuzz_unpack", "seed-1.bin", raw);
        check(rr::unpack_resource(raw) == Bytes({1, 2, 3, 4}), "unpack seed");
        Bytes blank(16, 0);
        write_seed(root, "fuzz_unpack", "seed-2.bin", blank);
        check(rr::unpack_resource(blank).empty(), "unpack empty seed");
    }
    {  // cel: 4x2 indexed with a 2-entry PLUT.
        const Bytes b = cel_bytes();
        write_seed(root, "fuzz_cel", "seed-1.cel", b);
        const auto cel = rr::read_cel(b);
        check(cel.width == 4 && cel.height == 2, "cel seed");
    }
    {  // family: packed payload for read_family, raw group for read_cel_group.
        const Bytes packed = family_packed(), group = family_group();
        write_seed(root, "fuzz_family", "seed-1.fam", packed);
        write_seed(root, "fuzz_family", "seed-2.group", group);
        (void)rr::read_family(packed);
        check(rr::read_cel_group(group, 0).cells.size() == 3, "family seeds");
    }
    {  // terrain: DLBR fixture, sample count comes from the target's first bytes.
        const Bytes b = terrain_bytes();
        write_seed(root, "fuzz_terrain", "seed-1.ter", b);
        (void)rr::read_terrain(b, 64);
    }
    {  // animation: single-frame RRAN.
        const Bytes b = animation_bytes();
        write_seed(root, "fuzz_animation", "seed-1.rran", b);
        check(rr::read_animation(b).frames.size() == 1, "animation seed");
    }
    {  // bobs: 2x1 frame plus an empty frame.
        const Bytes b = {2, 1, 7, 9, 0, 0};
        write_seed(root, "fuzz_bobs", "seed-1.bob", b);
        check(rr::read_bobs(b, 2).size() == 2, "bobs seed");
    }
    {  // avi: one video stream, two frames.
        const Bytes b = avi_bytes();
        write_seed(root, "fuzz_avi", "seed-1.avi", b);
        const auto avi = rr::read_avi(b);
        check(!avi.streams.empty() && !avi.chunks.empty(), "avi seed");
    }
    {  // audio: 8-bit mono WAV plus a raw IMA block.
        const Bytes wav = wav_bytes(1, 8, {0x80, 0xff, 0}, true);
        write_seed(root, "fuzz_audio", "seed-1.wav", wav);
        check(rr::read_wav(wav).rate == 11025, "wav seed");
        const uint8_t mono[] = {100, 0, 0, 0, 0x21, 0, 0, 0};
        write_seed(root, "fuzz_audio", "seed-2.ima", Bytes(mono, mono + sizeof mono));
        const auto samples = rr::decode_ima_adpcm(mono, sizeof mono, 1, 8);
        check(samples.size() == 9 && samples[0] == 100, "ima seed");
    }
    {  // iso: fixture with ROADRASH/A.BIN;1 holding "hello".
        const Bytes b = fixture::iso();
        write_seed(root, "fuzz_iso", "seed-1.iso", b);
        const Bytes copy = b;
        rr::Iso9660 iso([&](uint64_t at, uint8_t* out, size_t n) { return fixture::read_at(copy, at, out, n); });
        check(iso.list("ROADRASH") == std::vector<std::string>{"A.BIN"}, "iso list seed");
        check(iso.read("roadrash/a.bin") == Bytes({'h', 'e', 'l', 'l', 'o'}), "iso read seed");
    }
    {  // pe: one RT_STRING block holding "A" (string id 0).
        const Bytes b = pe_bytes();
        write_seed(root, "fuzz_pe", "seed-1.dll", b);
        check(rr::read_string_table(b).at(0) == "A", "pe table seed");
        check(rr::read_string_resources(b).at(0) == u"A", "pe resources seed");
        Bytes mz(128, 0);
        mz[0] = 'M'; mz[1] = 'Z';  // truncated MZ stub: exe-table readers must throw cleanly.
        write_seed(root, "fuzz_exe_tables", "seed-1.exe", mz);
    }
    {  // rri: 1x1 grey JPEG.
        const Bytes jpg(std::begin(kGrayJpeg), std::end(kGrayJpeg));
        write_seed(root, "fuzz_rri", "seed-1.jpg", jpg);
        check(rr::decode_rri(jpg).width == 1 && rr::decode_rri(jpg).pixels.size() == 1, "rri seed");
    }
    {  // save: Big Game start for character 1 written by the game code.
        const rr::CareerTables t = career_tables();
        rr::Career c;
        rr::select_mode(c, t, rr::GameMode::big_game);
        rr::choose_character(c, t, 1);
        const Bytes b = rr::write_save(c);
        write_seed(root, "fuzz_save", "seed-1.sav", b);
        check(rr::read_save(b, t).state.cash == 100, "save seed");
    }
    if (failures) std::cerr << failures << " seed failures\n";
    else std::cout << "seeds written under " << root.string() << '\n';
    return failures ? 1 : 0;
}
