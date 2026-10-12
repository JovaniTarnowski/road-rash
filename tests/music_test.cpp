// Race music: RIFF MIDS + SoundFont 1 parsers, corpus checks, player hooks.
#include <cmath>
#include <cstdio>
#include <iostream>

#include "assets.hpp"
#include "mixer.hpp"
#include "race_music.hpp"

namespace {
int failures = 0;
void check(bool v, const char* what) {
    if (!v) {
        ++failures;
        std::cerr << "FAIL " << what << "\n";
    }
}
void expect_throw(const std::vector<uint8_t>& b, bool mids, const char* what) {
    bool threw = false;
    try {
        if (mids)
            (void)rr::read_mids(b);
        else
            (void)rr::read_sf1(b, true);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, what);
}

// Minimal valid MIDS: one block, one note-on + note-off on channel 1.
std::vector<uint8_t> minimal_mids() {
    std::vector<uint8_t> b;
    auto put = [&](const void* p, size_t n) {
        const uint8_t* q = static_cast<const uint8_t*>(p);
        b.insert(b.end(), q, q + n);
    };
    auto u32 = [&](uint32_t v) {
        uint8_t q[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        put(q, 4);
    };
    // events: note-on (delta 0) + note-off (delta 480), stride 12
    // (delta, middle word, event word).
    const uint32_t ev_bytes = 24;
    const uint32_t data_bytes = 4 + 8 + ev_bytes;
    const uint32_t riff = 4 + 8 + 12 + 8 + data_bytes;
    put("RIFF", 4);
    u32(riff);
    put("MIDS", 4);
    put("fmt ", 4);
    u32(12);
    u32(480);
    u32(4096);
    u32(0);  // flags: 12-byte stride
    put("data", 4);
    u32(data_bytes);
    u32(1);  // one block
    u32(0);
    u32(ev_bytes);  // start tick, size
    u32(0);
    u32(0);
    u32(0x00402190u);  // delta 0, kind 0, vel 0x40 key 0x21 status 0x90
    u32(480);
    u32(0);
    u32(0x00002180u);  // delta 480, kind 0, vel 0 key 0x21 status 0x80
    return b;
}

// Minimal valid SF1: one melodic preset (program 30, bank 0), one zone, one
// 16-sample sine-ish loop.
std::vector<uint8_t> minimal_sf1(int bank_id = 0);
std::vector<uint8_t> minimal_sf1(int bank_id) {
    std::vector<uint8_t> b;
    auto put = [&](const void* p, size_t n) {
        const uint8_t* q = static_cast<const uint8_t*>(p);
        b.insert(b.end(), q, q + n);
    };
    auto u16 = [&](uint16_t v) {
        uint8_t q[2] = {uint8_t(v), uint8_t(v >> 8)};
        put(q, 2);
    };
    auto u32 = [&](uint32_t v) {
        uint8_t q[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        put(q, 4);
    };
    auto chunk = [&](const char* tag, const std::vector<uint8_t>& payload) {
        put(tag, 4);
        u32(uint32_t(payload.size()));
        put(payload.data(), payload.size());
        if (payload.size() & 1) {
            uint8_t z = 0;
            put(&z, 1);
        }
    };
    auto list = [&](const char* kind, const std::vector<uint8_t>& inner) {
        std::vector<uint8_t> payload;
        payload.insert(payload.end(), kind, kind + 4);
        payload.insert(payload.end(), inner.begin(), inner.end());
        put("LIST", 4);
        u32(uint32_t(payload.size()));
        put(payload.data(), payload.size());
        if (payload.size() & 1) {
            uint8_t z = 0;
            put(&z, 1);
        }
    };
    std::vector<uint8_t> info, sdta, pdta;
    auto info_chunk = [&](const char* tag, const std::vector<uint8_t>& p) {
        size_t o = info.size();
        info.resize(o + 8 + p.size() + (p.size() & 1));
        memcpy(info.data() + o, tag, 4);
        info[o + 4] = uint8_t(p.size());
        info[o + 5] = uint8_t(p.size() >> 8);
        info[o + 6] = uint8_t(p.size() >> 16);
        info[o + 7] = uint8_t(p.size() >> 24);
        memcpy(info.data() + o + 8, p.data(), p.size());
    };
    info_chunk("ifil", {1, 0, 0, 0});
    std::vector<int16_t> sine(16);
    for (int i = 0; i < 16; ++i) sine[i] = int16_t(lrint(20000.0 * sin(i * 6.2831853 / 16)));
    std::vector<uint8_t> smpl(32);
    memcpy(smpl.data(), sine.data(), 32);
    size_t o = sdta.size();
    sdta.resize(o + 8 + 32);
    memcpy(sdta.data() + o, "smpl", 4);
    sdta[o + 4] = 32;
    memcpy(sdta.data() + o + 8, smpl.data(), 32);
    auto pdta_chunk = [&](const char* tag, const std::vector<uint8_t>& p) {
        size_t q = pdta.size();
        pdta.resize(q + 8 + p.size() + (p.size() & 1));
        memcpy(pdta.data() + q, tag, 4);
        pdta[q + 4] = uint8_t(p.size());
        pdta[q + 5] = uint8_t(p.size() >> 8);
        pdta[q + 6] = uint8_t(p.size() >> 16);
        pdta[q + 7] = uint8_t(p.size() >> 24);
        memcpy(pdta.data() + q + 8, p.data(), p.size());
    };
    // phdr: preset + EOP terminal.
    std::vector<uint8_t> phdr(76, 0);
    memcpy(phdr.data(), "TestPreset", 10);
    phdr[20] = 30;
    phdr[22] = uint8_t(bank_id);
    phdr[38 + 24] = 1;
    pdta_chunk("phdr", phdr);
    // pbag: zone 0 (gens 0..1) + terminal.
    pdta_chunk("pbag", {0, 0, 0, 0, 1, 0, 0, 0});
    // pgen: instrument 0.
    pdta_chunk("pgen", {41, 0, 0, 0});
    // inst: instrument + EOI terminal.
    std::vector<uint8_t> inst(44, 0);
    inst[20] = 0;
    inst[22 + 20] = 1;
    pdta_chunk("inst", inst);
    // ibag: zone 0 (gens 0..3) + terminal.
    pdta_chunk("ibag", {0, 0, 0, 0, 3, 0, 0, 0});
    // igen: keyRange low=0 high=127 (high byte first), rootKey 60, sampleID 0.
    pdta_chunk("igen", {43, 0, 0, 127, 58, 0, 60, 0, 53, 0, 0, 0});
    // shdr (compact 16 bytes): start 0, end 16, loop 0..16.
    pdta_chunk("shdr", {0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0});

    std::vector<uint8_t> body;
    {
        std::vector<uint8_t> inner;
        inner.insert(inner.end(), info.begin(), info.end());
        size_t q = body.size();
        body.resize(q + 12 + inner.size());
        memcpy(body.data() + q, "LIST", 4);
        uint32_t sz = uint32_t(4 + inner.size());
        memcpy(body.data() + q + 4, &sz, 4);
        memcpy(body.data() + q + 8, "INFO", 4);
        memcpy(body.data() + q + 12, inner.data(), inner.size());
    }
    {
        size_t q = body.size();
        body.resize(q + 12 + sdta.size());
        memcpy(body.data() + q, "LIST", 4);
        uint32_t sz = uint32_t(4 + sdta.size());
        memcpy(body.data() + q + 4, &sz, 4);
        memcpy(body.data() + q + 8, "sdta", 4);
        memcpy(body.data() + q + 12, sdta.data(), sdta.size());
    }
    {
        size_t q = body.size();
        body.resize(q + 12 + pdta.size());
        memcpy(body.data() + q, "LIST", 4);
        uint32_t sz = uint32_t(4 + pdta.size());
        memcpy(body.data() + q + 4, &sz, 4);
        memcpy(body.data() + q + 8, "pdta", 4);
        memcpy(body.data() + q + 12, pdta.data(), pdta.size());
    }
    (void)u16;
    (void)chunk;
    (void)list;
    std::vector<uint8_t> out;
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    uint32_t riff = uint32_t(4 + body.size());
    out.push_back(uint8_t(riff));
    out.push_back(uint8_t(riff >> 8));
    out.push_back(uint8_t(riff >> 16));
    out.push_back(uint8_t(riff >> 24));
    out.insert(out.end(), {'s', 'f', 'b', 'k'});
    out.insert(out.end(), body.begin(), body.end());
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    // Selection rule (0x00433bb0).
    check(rr::race_song_name(1, "") == "SBKGRNG1", "course 1 -> SBKGRNG1");
    check(rr::race_song_name(4, "") == "SBKGRNG4", "course 4 (NAPA cli) name maps");
    check(rr::race_song_name(3, "krusty") == "KLOWN", "krusty -> KLOWN");
    check(rr::race_song_name(2, "Bozo the clown") == "KLOWN", "bozo prefix -> KLOWN");
    check(rr::race_song_name(5, "KRUSTY") == "KLOWN", "alias match is case-insensitive");
    check(rr::race_song_name(5, "krust") == "SBKGRNG5", "short prefix does not match");
    bool threw = false;
    try {
        (void)rr::race_song_name(0, "");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "course 0 rejected");

    // Synthetic MIDS.
    const auto mids = minimal_mids();
    const rr::MidsSong song = rr::read_mids(mids);
    check(song.division == 480, "synthetic division");
    check(song.events.size() == 2, "synthetic event count");
    check(song.events[0].tick == 0 && song.events[0].status == 0x90 &&
              song.events[0].data0 == 0x21 && song.events[0].data1 == 0x40,
          "synthetic note-on");
    check(song.events[1].tick == 480 && song.events[1].status == 0x80, "synthetic note-off");
    for (size_t i = 0; i < mids.size(); ++i) {
        std::vector<uint8_t> cut(mids.begin(), mids.begin() + i);
        expect_throw(cut, true, "every MIDS truncation throws");
        if (failures) break;
    }
    check(!failures, "MIDS truncation sweep");
    expect_throw({0, 1, 2, 3}, true, "garbage MIDS throws");
    std::vector<uint8_t> bad_tag = mids;
    memcpy(bad_tag.data(), "RIFX", 4);
    expect_throw(bad_tag, true, "wrong RIFF tag throws");
    std::vector<uint8_t> bad_kind = mids;
    memcpy(bad_kind.data() + 8, "XXXX", 4);
    expect_throw(bad_kind, true, "non-MIDS form throws");

    // Synthetic SF1.
    const auto sf1 = minimal_sf1();
    {
        auto bank = rr::read_sf1(sf1, true);
        check(bank->zones.size() == 1, "synthetic zone count");
        check(bank->zones[0].program == 30, "synthetic program");
        check(bank->samples.size() == 16, "synthetic sample count");
        // Offline render: note 33 on channel 1 with program 30 -> audible.
        rr::MidsSong s;
        s.division = 480;
        s.tempo.emplace_back(0, 500000);
        s.events = {{0, 0xC1, 30, 0}, {0, 0x91, 33, 100}, {480, 0x81, 33, 0}};
        auto drums = rr::read_sf1(sf1, false);
        check(drums->zones.empty(), "drum view of melodic bank is empty");
        // Drum-kit bank built directly (preset bank byte 128).
        auto kit = rr::read_sf1(minimal_sf1(128), false);
        std::vector<int16_t> mono;
        check(rr::render_race_song(s, *bank, *kit, mono), "synthetic song renders");
        double energy = 0;
        for (int16_t v : mono) energy += double(v) * v;
        check(energy > 0, "synthetic render is audible");
    }
    for (size_t i = 0; i < sf1.size(); ++i) {
        std::vector<uint8_t> cut(sf1.begin(), sf1.begin() + i);
        bool ok = false;
        try {
            (void)rr::read_sf1(cut, true);
        } catch (const std::runtime_error&) {
            ok = true;
        }
        if (!ok) {
            check(false, "every SF1 truncation throws");
            break;
        }
    }
    expect_throw({1, 2, 3, 4, 5}, false, "garbage SF1 throws");

    // Player with no assets: silent no-op, never throws.
    {
        rr::Mixer mixer(8);
        // Nonexistent root: every asset read fails.
        rr::Assets empty("/nonexistent-rr-disc-root");
        rr::RaceMusicPlayer player(empty, mixer);
        player.play_race(4, "");
        check(!player.active() && !player.loaded(), "missing assets: silent no-op");
        player.update();
        player.pause(true);
        player.stop();
    }

    // Corpus (skip 77 without the disc).
    rr::Assets assets(argc > 1 ? argv[1] : RR_SOURCE_DIR "/../cdrom/ROADRASH");
    if (!assets.exists("ROADRASH.EXE")) {
        std::cout << "skip: no disc\n";
        return failures ? 1 : 77;
    }
    const char* songs[] = {"SBKGRNG1", "SBKGRNG2", "SBKGRNG3",
                           "SBKGRNG4", "SBKGRNG5", "KLOWN"};
    for (const char* name : songs) {
        const std::string path = std::string("AUDIO/MUSIC/") + name + ".MID";
        if (!assets.exists(path)) {
            check(false, "corpus song present");
            continue;
        }
        rr::MidsSong s = rr::read_mids(assets.read(path));
        check(!s.events.empty(), "corpus song has events");
        std::cout << name << ": " << s.events.size() << " events, division " << s.division
                  << ", tempos " << s.tempo.size() << "\n";
    }
    for (int n = 1; n <= 5; ++n) {
        auto b = rr::read_sf1(
            assets.read("AUDIO/MUSIC/GRUNGE" + std::to_string(n) + ".SBK"), true);
        check(!b->zones.empty() && !b->samples.empty(), "corpus melodic bank loads");
        std::cout << "GRUNGE" << n << ": " << b->zones.size() << " zones, " << b->samples.size()
                  << " samples\n";
    }
    {
        auto kit = rr::read_sf1(assets.read("AUDIO/MUSIC/RRDRUMS.SBK"), false);
        check(kit->zones.size() >= 12, "drum kit zone count");
        // Full corpus render of the Napa song: must be audible.
        rr::MidsSong s =
            rr::read_mids(assets.read(std::string("AUDIO/MUSIC/") + "SBKGRNG4.MID"));
        auto mel = rr::read_sf1(assets.read("AUDIO/MUSIC/GRUNGE4.SBK"), true);
        std::vector<int16_t> mono;
        check(rr::render_race_song(s, *mel, *kit, mono), "corpus Napa song renders");
        double energy = 0;
        for (size_t i = 0; i < mono.size(); i += 7) energy += double(mono[i]) * mono[i];
        std::cout << "SBKGRNG4 render: " << mono.size() << " frames\n";
        check(energy > 0, "corpus render is audible");
        // Player loads the race song behind the same interface.
        rr::Mixer mixer(8);
        rr::RaceMusicPlayer player(assets, mixer);
        player.play_race(4, "");
        check(player.active() && player.loaded() && player.song() == "SBKGRNG4",
              "play_race(4) loads SBKGRNG4");
        check(player.event_count() == s.events.size(), "player event count matches parser");
        player.stop();
        check(!player.active(), "stop silences");
        player.play_race(1, "krusty");
        check(player.active() && player.song() == "KLOWN", "krusty loads KLOWN");
        player.stop();
    }

    if (failures) std::cerr << failures << " failures\n";
    return failures ? 1 : 0;
}
