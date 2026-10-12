#include "race_music.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "assets.hpp"
#include "mixer.hpp"

namespace rr {
namespace {

uint16_t u16le(const std::vector<uint8_t>& b, size_t p) {
    if (p + 2 > b.size()) throw std::runtime_error("MIDS/SF1: truncated u16");
    return uint16_t(b[p]) | (uint16_t(b[p + 1]) << 8);
}

uint32_t u32le(const std::vector<uint8_t>& b, size_t p) {
    if (p + 4 > b.size()) throw std::runtime_error("MIDS/SF1: truncated u32");
    return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) |
           (uint32_t(b[p + 3]) << 24);
}

// Bounds-checked RIFF chunk walk.
struct Chunk {
    std::string tag;
    size_t payload = 0;
    uint32_t size = 0;
};

std::vector<Chunk> riff_chunks(const std::vector<uint8_t>& b, size_t first, size_t end) {
    std::vector<Chunk> out;
    while (first < end) {
        if (first + 8 > end) throw std::runtime_error("MIDS/SF1: truncated chunk header");
        char tag[5] = {};
        memcpy(tag, b.data() + first, 4);
        uint32_t size = u32le(b, first + 4);
        size_t last = first + 8 + size;
        if (last > end) throw std::runtime_error("MIDS/SF1: chunk overruns parent");
        out.push_back({tag, first + 8, size});
        first = last + (size & 1);  // word-aligned
        if (first > end) throw std::runtime_error("MIDS/SF1: truncated chunk pad");
    }
    if (first != end) throw std::runtime_error("MIDS/SF1: chunk list overrun");
    return out;
}

bool ieq_prefix(const std::string& s, const char* prefix, size_t n) {
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i])) return false;
    return true;
}

}  // namespace

bool race_song_is_klown(const std::string& alias) {
    // 0x00433bb0: strnicmp(alias, "krusty", 6) / strnicmp(alias, "bozo", 4).
    return ieq_prefix(alias, "krusty", 6) || ieq_prefix(alias, "bozo", 4);
}

std::string race_song_name(int original_course_index, const std::string& alias) {
    if (race_song_is_klown(alias)) return "KLOWN";
    // music_generic table order: 1 CITY .. 5 CANYN -> Grunge1..5.
    if (original_course_index < 1 || original_course_index > 5)
        throw std::runtime_error("race song: course index out of range");
    return "SBKGRNG" + std::to_string(original_course_index);
}

MidsSong read_mids(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 12) throw std::runtime_error("MIDS: truncated RIFF header");
    if (memcmp(bytes.data(), "RIFF", 4) != 0) throw std::runtime_error("MIDS: missing RIFF tag");
    uint32_t riff_size = u32le(bytes, 4);
    if (riff_size + 8 > bytes.size()) throw std::runtime_error("MIDS: truncated RIFF");
    if (memcmp(bytes.data() + 8, "MIDS", 4) != 0) throw std::runtime_error("MIDS: not a MIDS form");
    const size_t form_end = size_t(8) + riff_size;
    if (form_end > bytes.size()) throw std::runtime_error("MIDS: truncated RIFF");

    MidsSong song;
    size_t fmt_off = 0, fmt_sz = 0, data_off = 0, data_sz = 0;
    bool have_fmt = false, have_data = false;
    for (const Chunk& c : riff_chunks(bytes, 12, form_end)) {
        if (c.tag == "fmt " && !have_fmt) {
            fmt_off = c.payload;
            fmt_sz = c.size;
            have_fmt = true;
        } else if (c.tag == "data" && !have_data) {
            data_off = c.payload;
            data_sz = c.size;
            have_data = true;
        }
    }
    if (!have_fmt) throw std::runtime_error("MIDS: missing fmt chunk");
    if (!have_data) throw std::runtime_error("MIDS: missing data chunk");
    if (fmt_sz != 12) throw std::runtime_error("MIDS: bad fmt size");
    song.division = u32le(bytes, fmt_off);
    const uint32_t max_buffer = u32le(bytes, fmt_off + 4);
    const uint32_t flags = u32le(bytes, fmt_off + 8);
    if (song.division == 0 || song.division > 32768) throw std::runtime_error("MIDS: bad division");
    if (max_buffer == 0 || max_buffer > (1 << 24))
        throw std::runtime_error("MIDS: bad max_buffer");
    if (flags & ~1u) throw std::runtime_error("MIDS: unknown flags");

    size_t p = data_off;
    const size_t data_end = data_off + data_sz;
    if (data_end > bytes.size()) throw std::runtime_error("MIDS: truncated data");
    if (p + 4 > data_end) throw std::runtime_error("MIDS: truncated block count");
    const uint32_t blocks = u32le(bytes, p);
    p += 4;
    if (blocks > (1 << 20)) throw std::runtime_error("MIDS: absurd block count");
    const size_t stride = (flags & 1) ? 8 : 12;
    uint32_t tick = 0;
    for (uint32_t i = 0; i < blocks; ++i) {
        if (p + 8 > data_end) throw std::runtime_error("MIDS: truncated block header");
        const uint32_t size = u32le(bytes, p + 4);  // p[0..4): start tick (informational)
        p += 8;
        if (size > max_buffer) throw std::runtime_error("MIDS: block exceeds max_buffer");
        if (p + size > data_end) throw std::runtime_error("MIDS: truncated block");
        const size_t end = p + size;
        while (p < end) {
            if (p + stride > end) throw std::runtime_error("MIDS: truncated event");
            const uint32_t delta = u32le(bytes, p);
            const uint32_t event = u32le(bytes, p + stride - 4);
            p += stride;
            // 32-bit tick wraparound matches the original's DWORD accumulation.
            tick += delta;
            const uint32_t kind = (event >> 24) & 191;
            if (event & 0x80000000u) {
                const uint32_t length = event & 0xffffffu;
                const size_t padded = (size_t(length) + 3) & ~size_t(3);
                if (p + padded > end) throw std::runtime_error("MIDS: truncated long event");
                p += padded;  // SysEx/meta payload: counted, not sequenced
            } else if (kind == 1) {
                song.tempo.emplace_back(tick, event & 0xffffffu);
            } else if (kind == 0) {
                const uint8_t status = event & 0xff, a = (event >> 8) & 0xff, v = (event >> 16) & 0xff;
                if (status < 0x80 || status >= 0xF0 || a >= 0x80 || v >= 0x80)
                    throw std::runtime_error("MIDS: bad short event");
                song.events.push_back({tick, status, a, v});
            } else {
                throw std::runtime_error("MIDS: unknown short event kind");
            }
        }
        if (p != end) throw std::runtime_error("MIDS: block overrun");
    }
    if (p != data_end) throw std::runtime_error("MIDS: trailing data");
    song.total_ticks = tick;
    return song;
}

std::shared_ptr<const Sf1Bank> read_sf1(const std::vector<uint8_t>& bytes, bool melodic) {
    if (bytes.size() < 12) throw std::runtime_error("SF1: truncated RIFF header");
    if (memcmp(bytes.data(), "RIFF", 4) != 0) throw std::runtime_error("SF1: missing RIFF tag");
    uint32_t riff_size = u32le(bytes, 4);
    if (riff_size + 8 > bytes.size()) throw std::runtime_error("SF1: truncated RIFF");
    if (memcmp(bytes.data() + 8, "sfbk", 4) != 0) throw std::runtime_error("SF1: not an sfbk form");
    const size_t form_end = size_t(8) + riff_size;

    // Flatten the two-level LIST tree into path-keyed payloads.
    struct Entry {
        std::string path;
        size_t off;
        uint32_t size;
    };
    std::vector<Entry> flat;
    for (const Chunk& top : riff_chunks(bytes, 12, form_end)) {
        if (top.tag != "LIST") throw std::runtime_error("SF1: expected LIST");
        if (top.size < 4) throw std::runtime_error("SF1: truncated LIST");
        char kind[5] = {};
        memcpy(kind, bytes.data() + top.payload, 4);
        std::string prefix = std::string(kind) + "/";
        for (const Chunk& c : riff_chunks(bytes, top.payload + 4, top.payload + top.size)) {
            if (c.tag == "LIST") throw std::runtime_error("SF1: nested LIST");
            flat.push_back({prefix + c.tag, c.payload, c.size});
        }
    }
    auto find = [&](const char* path) -> const Entry* {
        for (const auto& e : flat)
            if (e.path == path) return &e;
        return nullptr;
    };
    const Entry* ifil = find("INFO/ifil");
    const Entry* smpl = find("sdta/smpl");
    const Entry* phdr = find("pdta/phdr");
    const Entry* pbag = find("pdta/pbag");
    const Entry* pgen = find("pdta/pgen");
    const Entry* inst = find("pdta/inst");
    const Entry* ibag = find("pdta/ibag");
    const Entry* igen = find("pdta/igen");
    const Entry* shdr = find("pdta/shdr");
    if (!ifil || !smpl || !phdr || !pbag || !pgen || !inst || !ibag || !igen || !shdr)
        throw std::runtime_error("SF1: missing chunk");
    if (ifil->size != 4) throw std::runtime_error("SF1: bad ifil");
    if (u16le(bytes, ifil->off) != 1) throw std::runtime_error("SF1: not SoundFont 1");
    if (smpl->size % 2 != 0) throw std::runtime_error("SF1: odd smpl size");
    if (phdr->size % 38 != 0 || pbag->size % 4 != 0 || pgen->size % 4 != 0 ||
        inst->size % 22 != 0 || ibag->size % 4 != 0 || igen->size % 4 != 0)
        throw std::runtime_error("SF1: bad pdta alignment");

    auto bank = std::make_shared<Sf1Bank>();
    bank->samples.resize(smpl->size / 2);
    for (size_t i = 0; i < bank->samples.size(); ++i) {
        const size_t o = smpl->off + 2 * i;
        bank->samples[i] = int16_t(bytes[o] | (bytes[o + 1] << 8));
    }
    const size_t sample_count = bank->samples.size();

    struct Gen {
        uint16_t op = 0, amount = 0;
    };
    auto gen_list = [&](const Entry* e) {
        std::vector<Gen> v;
        for (uint32_t i = 0; i < e->size; i += 4)
            v.push_back({u16le(bytes, e->off + i), u16le(bytes, e->off + i + 2)});
        return v;
    };
    const std::vector<Gen> pgens = gen_list(pgen);
    const std::vector<Gen> igens = gen_list(igen);
    auto bag_list = [&](const Entry* e) {
        std::vector<std::pair<uint16_t, uint16_t>> v;
        for (uint32_t i = 0; i < e->size; i += 4)
            v.push_back({u16le(bytes, e->off + i), u16le(bytes, e->off + i + 2)});
        return v;
    };
    const auto pbags = bag_list(pbag);
    const auto ibags = bag_list(ibag);

    const uint32_t n_presets = phdr->size / 38;
    const uint32_t n_inst = inst->size / 22;
    // Bag lists hold one entry per zone plus a terminal entry; their counts
    // are independent of the preset/instrument counts.
    if (n_presets < 1 || pbags.empty()) throw std::runtime_error("SF1: bad preset bags");
    if (n_inst < 1 || ibags.empty()) throw std::runtime_error("SF1: bad instrument bags");
    // Sample headers in these banks are a compact 16 bytes: start, end,
    // loopStart, loopEnd (no name/rate/pitch fields). Sample rate is 22050 Hz
    // (the AWE32 engine default) and the root pitch comes from generator 58
    // (overridingRootKey), defaulting to 60.
    if (shdr->size % 16 != 0) throw std::runtime_error("SF1: bad shdr alignment");
    const uint32_t n_samples = shdr->size / 16;

    auto sample_range = [&](uint32_t s) {
        if (s >= n_samples) throw std::runtime_error("SF1: sample id out of range");
        const size_t o = shdr->off + 16 * size_t(s);
        const uint32_t start = u32le(bytes, o);
        const uint32_t end = u32le(bytes, o + 4);
        const uint32_t loop_start = u32le(bytes, o + 8);
        const uint32_t loop_end = u32le(bytes, o + 12);
        if (end > sample_count || loop_end > sample_count)
            throw std::runtime_error("SF1: sample range out of range");
        if (start > end) throw std::runtime_error("SF1: inverted sample range");
        struct R {
            uint32_t s, e, ls, le;
        };
        return R{start, end, loop_start, loop_end};
    };

    // Global + per-zone generators for one bag range: last occurrence wins.
    auto collect = [&](const std::vector<Gen>& gens, uint32_t lo, uint32_t hi) {
        std::map<uint16_t, uint16_t> m;
        for (uint32_t i = lo; i < hi; ++i) {
            if (i >= gens.size()) throw std::runtime_error("SF1: generator index out of range");
            if (gens[i].op != 0) m[gens[i].op] = gens[i].amount;
        }
        return m;
    };

    for (uint32_t pi = 0; pi + 1 < n_presets; ++pi) {
        const size_t o = phdr->off + 38 * size_t(pi);
        const uint16_t program = u16le(bytes, o + 20);
        const uint16_t bank_id = u16le(bytes, o + 22);
        const uint16_t bag_lo = u16le(bytes, o + 24);
        const uint16_t bag_hi = u16le(bytes, o + 38 + 24);
        const bool want = melodic ? (bank_id == 0) : (bank_id == 128);
        if (!want) continue;
        if (bag_lo > bag_hi || bag_hi > pbags.size())
            throw std::runtime_error("SF1: preset bag out of range");
        for (uint32_t bi = bag_lo; bi < bag_hi; ++bi) {
            const uint32_t g_lo = (bi < pbags.size()) ? pbags[bi].first : 0;
            const uint32_t g_hi = (bi + 1 < pbags.size()) ? pbags[bi + 1].first : uint32_t(pgens.size());
            if (g_lo > g_hi || g_hi > pgens.size())
                throw std::runtime_error("SF1: pgen out of range");
            const auto pzone = collect(pgens, g_lo, g_hi);
            auto it_inst = pzone.find(41);  // instrument
            if (it_inst == pzone.end()) continue;  // global preset zone
            const uint32_t inst_id = it_inst->second;
            if (inst_id >= n_inst) throw std::runtime_error("SF1: instrument out of range");
            const uint16_t ib_lo = u16le(bytes, inst->off + 22 * size_t(inst_id) + 20);
            const uint16_t ib_hi = (inst_id + 1 < n_inst)
                                       ? u16le(bytes, inst->off + 22 * size_t(inst_id + 1) + 20)
                                       : uint16_t(ibags.size() - 1);
            if (ib_lo > ib_hi || ib_hi > ibags.size())
                throw std::runtime_error("SF1: instrument bag out of range");
            const int16_t preset_coarse = pzone.count(51) ? int16_t(pzone.at(51)) : int16_t(0);
            const int16_t preset_fine = pzone.count(52) ? int16_t(pzone.at(52)) : int16_t(0);
            const int16_t preset_att = pzone.count(48) ? int16_t(pzone.at(48)) : int16_t(0);
            for (uint32_t bj = ib_lo; bj < ib_hi; ++bj) {
                const uint32_t h_lo = (bj < ibags.size()) ? ibags[bj].first : 0;
                const uint32_t h_hi =
                    (bj + 1 < ibags.size()) ? ibags[bj + 1].first : uint32_t(igens.size());
                if (h_lo > h_hi || h_hi > igens.size())
                    throw std::runtime_error("SF1: igen out of range");
                const auto izone = collect(igens, h_lo, h_hi);
                auto it_s = izone.find(53);  // sampleID
                if (it_s == izone.end()) continue;  // global instrument zone
                const auto r = sample_range(it_s->second);
                Sf1Zone z;
                z.program = uint8_t(program & 127);
                z.bank = uint8_t(bank_id == 128 ? 128 : 0);
                // Generators 43/44 (keyRange/velRange) pack low+(high<<8).
                z.low_key = izone.count(43) ? uint8_t(izone.at(43) & 0xff) : uint8_t(0);
                z.high_key = izone.count(43) ? uint8_t(izone.at(43) >> 8) : uint8_t(127);
                if (z.low_key > z.high_key) throw std::runtime_error("SF1: inverted key range");
                z.root_key = izone.count(58) ? uint8_t(izone.at(58) & 127) : uint8_t(60);
                const int16_t coarse =
                    int16_t(izone.count(51) ? int16_t(izone.at(51)) : 0) + preset_coarse;
                const int16_t fine =
                    int16_t(izone.count(52) ? int16_t(izone.at(52)) : 0) + preset_fine;
                z.coarse_tune = int8_t(std::clamp(coarse, int16_t(-127), int16_t(127)));
                z.fine_tune = int8_t(std::clamp(fine, int16_t(-127), int16_t(127)));
                z.volume_db_cb =
                    int16_t(izone.count(48) ? int16_t(izone.at(48)) : 0) + preset_att;
                z.sample_start = r.s;
                z.sample_end = r.e;
                z.loop_start = (r.le > r.ls && r.ls >= r.s && r.le <= r.e) ? r.ls : r.s;
                z.loop_end = (r.le > r.ls) ? r.le : r.s;  // <= start: no loop
                z.sample_rate = 22050;
                bank->zones.push_back(z);
            }
        }
    }
    return bank;
}

namespace {
// Offline song renderer used by the test hook: maps channel events to zones
// (channel 9 -> drum kit, others -> melodic preset by last program change)
// and mixes pitched sample spans with a 5 ms release ramp.
struct Note {
    uint32_t start_frame = 0, frames = 0;
    uint8_t velocity = 0;
    uint8_t key = 0;
    const Sf1Zone* zone = nullptr;
};

const Sf1Zone* pick_zone(const Sf1Bank& melodic, const Sf1Bank& drums, const uint8_t* program,
                         uint8_t ch, uint8_t key) {
    const Sf1Bank& b = (ch == 9) ? drums : melodic;
    const uint8_t prog = (ch == 9) ? 0 : program[ch];
    for (const Sf1Zone& z : b.zones) {
        if (ch != 9 && z.program != prog) continue;
        if (key >= z.low_key && key <= z.high_key) return &z;
    }
    return nullptr;  // unmapped (program,key): silent, as on hardware without
                     // the factory presets.
}

float note_gain(uint8_t velocity, int16_t cb) {
    const float vel = velocity / 127.0f;
    return vel * float(pow(10.0, -double(cb) / 200.0));  // centibels -> amplitude
}

void render_song(const MidsSong& song, const Sf1Bank& melodic, const Sf1Bank& drums,
                 std::vector<int16_t>& mono) {
    constexpr double kRate = 22050.0;
    std::vector<std::pair<uint32_t, uint32_t>> tempo = song.tempo;
    if (tempo.empty() || tempo[0].first != 0) tempo.insert(tempo.begin(), {0, 500000});
    auto tick_to_frames = [&](uint32_t tick) {
        double frames = 0.0;
        uint32_t last_tick = 0, usec = tempo[0].second;
        for (size_t i = 1; i <= tempo.size(); ++i) {
            uint32_t edge = (i < tempo.size()) ? tempo[i].first : tick;
            if (edge > tick) edge = tick;
            frames += double(edge - last_tick) * usec / 1e6 / double(song.division) * kRate;
            if (edge == tick) break;
            last_tick = edge;
            usec = tempo[i].second;
        }
        return uint32_t(frames);
    };
    uint8_t program[16] = {};
    struct On {
        uint32_t tick;
        uint8_t vel, key;
        const Sf1Zone* zone;
    };
    std::map<std::pair<uint8_t, uint8_t>, On> active;
    std::vector<Note> notes;
    for (const MidsEvent& e : song.events) {
        const uint8_t op = e.status & 0xf0, ch = e.status & 0x0f;
        if (op == 0xC0) {
            program[ch] = e.data0 & 127;
        } else if (op == 0x90 && e.data1 != 0u) {
            const Sf1Zone* z = pick_zone(melodic, drums, program, ch, e.data0);
            if (z) active[{ch, e.data0}] = {e.tick, e.data1, e.data0, z};
            // Unmapped notes stay silent.
        } else if (op == 0x90 || op == 0x80) {  // note off (incl. vel-0 on)
            auto it = active.find({ch, e.data0});
            if (it == active.end()) continue;
            const uint32_t s = tick_to_frames(it->second.tick);
            const uint32_t en = tick_to_frames(e.tick);
            if (en > s && en - s < uint32_t(kRate * 60))
                notes.push_back({s, en - s, it->second.vel, it->second.key, it->second.zone});
            active.erase(it);
        }
    }
    uint32_t total = 0;
    for (const Note& n : notes) total = std::max(total, n.start_frame + n.frames);
    if (!total || total > uint32_t(kRate * 1200)) return;  // cap 20 minutes
    std::vector<double> mix(total, 0.0);
    for (const Note& n : notes) {
        const Sf1Zone* z = n.zone;
        const std::vector<int16_t>& smp = (z->bank == 128) ? drums.samples : melodic.samples;
        if (smp.empty() || z->sample_end > smp.size()) continue;
        const double semis =
            double(int(n.key) - int(z->root_key) + z->coarse_tune) + double(z->fine_tune) / 100.0;
        const double step = double(z->sample_rate) / kRate * pow(2.0, semis / 12.0);
        const bool loop = z->loop_end > z->loop_start;
        const float gain = note_gain(n.velocity, z->volume_db_cb);
        const uint32_t rel = uint32_t(kRate * 0.005);  // 5 ms release ramp
        double pos = 0.0;
        for (uint32_t i = 0; i < n.frames; ++i) {
            uint32_t idx = z->sample_start + uint32_t(pos);
            const double frac = pos - floor(pos);
            auto at = [&](uint32_t k) -> double {
                if (k >= z->sample_end) {
                    if (!loop) return 0.0;
                    k = z->loop_start + (k - z->loop_start) % (z->loop_end - z->loop_start);
                }
                return double(smp[k]) / 32768.0;
            };
            const double v = at(idx) * (1.0 - frac) + at(idx + 1) * frac;
            float g = gain;
            if (rel > 1 && i + rel >= n.frames) g *= float(n.frames - 1 - i) / float(rel);
            mix[n.start_frame + i] += v * g;
            pos += step;
            if (!loop && z->sample_start + uint32_t(pos) >= z->sample_end) break;
        }
    }
    mono.resize(total);
    for (uint32_t i = 0; i < total; ++i)
        mono[i] = int16_t(lrint(std::clamp(mix[i], -1.0, 1.0) * 32767.0));
}
}  // namespace

// Offline render entry point shared with the test (no audio hardware).
bool render_race_song(const MidsSong& song, const Sf1Bank& melodic, const Sf1Bank& drums,
                      std::vector<int16_t>& mono) {
    render_song(song, melodic, drums, mono);
    return !mono.empty();
}

RaceMusicPlayer::RaceMusicPlayer(const Assets& assets, Mixer& mixer)
    : assets_(assets), mixer_(mixer) {}

RaceMusicPlayer::~RaceMusicPlayer() { stop(); }

void RaceMusicPlayer::play_race(int original_course_index, const std::string& alias) {
    stop();
    // 0x00433bb0: the AWE `SbkGrng` mix is used when an AWE32 is present
    // (0x00434500); there is no GM fallback path worth emulating on modern
    // hardware, so the AWE mix always plays. Missing/unreadable assets are a
    // silent no-op and the race continues.
    std::string song;
    try {
        song = race_song_name(original_course_index, alias);
    } catch (const std::runtime_error&) {
        return;
    }
    try {
        song_data_ = read_mids(assets_.read("AUDIO/MUSIC/" + song + ".MID"));
        if (song == "KLOWN") {
            // Klown is a song, not a bank (no KLOWN.SBK on disc): it was
            // written against the AWE factory ROM presets. Merge every course
            // bank for best effort; notes outside those programs stay silent.
            auto merged = std::make_shared<Sf1Bank>();
            for (int n = 1; n <= 5; ++n) {
                auto b = read_sf1(
                    assets_.read("AUDIO/MUSIC/GRUNGE" + std::to_string(n) + ".SBK"), true);
                merged->samples.insert(merged->samples.end(), b->samples.begin(),
                                       b->samples.end());
                const uint32_t off = uint32_t(merged->samples.size() - b->samples.size());
                for (const Sf1Zone& z : b->zones) {
                    Sf1Zone shifted = z;
                    shifted.sample_start += off;
                    shifted.sample_end += off;
                    shifted.loop_start += off;
                    shifted.loop_end += off;
                    merged->zones.push_back(shifted);
                }
            }
            melodic_ = std::move(merged);
        } else {
            melodic_ = read_sf1(assets_.read("AUDIO/MUSIC/GRUNGE" +
                                            std::to_string(original_course_index) + ".SBK"),
                                true);
        }
        drums_ = read_sf1(assets_.read("AUDIO/MUSIC/RRDRUMS.SBK"), false);
    } catch (const std::runtime_error&) {
        stop();
        return;
    }
    song_ = song;
    loaded_ = true;
    playing_ = true;
}

void RaceMusicPlayer::stop() {
    playing_ = false;
    paused_ = false;
    loaded_ = false;
    song_.clear();
    song_data_ = MidsSong{};
    melodic_.reset();
    drums_.reset();
}

void RaceMusicPlayer::pause(bool value) { paused_ = value; }

void RaceMusicPlayer::update() {}

}  // namespace rr
