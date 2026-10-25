#include "hud.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
#include <utility>
static uint8_t brute_index(uint32_t c, const rr::Palette& pal) {
    uint8_t best = 0;
    int best_d = INT_MAX;
    for (int i = 1; i < 256; ++i) {
        const uint8_t* rgb = &pal.rgbx[size_t(4 * i)];
        const int dr = int(rgb[0]) - int((c >> 16) & 0xff);
        const int dg = int(rgb[1]) - int((c >> 8) & 0xff);
        const int db = int(rgb[2]) - int(c & 0xff);
        const int d = dr * dr + dg * dg + db * db;
        if (d < best_d) {
            best_d = d;
            best = uint8_t(i);
        }
    }
    return best;
}
int main() {
    try {
        // Review 2026-10-13 item 2: DashLut matches the brute-force search.
        {
            rr::Palette pal;
            for (int i = 0; i < 256; ++i) {
                pal.rgbx[4 * i] = uint8_t(i);
                pal.rgbx[4 * i + 1] = uint8_t(255 - i);
                pal.rgbx[4 * i + 2] = uint8_t((i * 5) & 255);
                pal.rgbx[4 * i + 3] = 0;
            }
            rr::DashLut lut;
            for (int i = 0; i < 256; ++i) {
                const uint8_t* c = &pal.rgbx[4 * i];
                const uint32_t px = (uint32_t(c[0]) << 16) | (uint32_t(c[1]) << 8) | uint32_t(c[2]);
                if (lut.index(px, pal) != brute_index(px, pal))
                    throw std::runtime_error("DashLut palette entry");
                const uint32_t dark =
                    (uint32_t(c[0] / 2) << 16) | (uint32_t(c[1] / 2) << 8) | uint32_t(c[2] / 2);
                if (lut.index(dark, pal) != brute_index(dark, pal))
                    throw std::runtime_error("DashLut darkened entry");
            }
            for (uint32_t k = 0; k < 32768; k += 97) {
                const uint32_t px = (uint32_t(((k >> 10) & 31) * 8 + 4) << 16) |
                                    (uint32_t(((k >> 5) & 31) * 8 + 4) << 8) | uint32_t((k & 31) * 8 + 4);
                if (lut.index(px, pal) != brute_index(px, pal))
                    throw std::runtime_error("DashLut cell sweep");
            }
            pal.rgbx[4] = 200; // palette change without reset may serve stale entries
            lut.reset();
            const uint32_t px = (uint32_t(200) << 16);
            if (lut.index(px, pal) != brute_index(px, pal)) throw std::runtime_error("DashLut after reset");
        }
        // F10 MiniDash cycle (0x004431d0): 0->1->2->0.
        if (rr::Hud::cycle_mini_dash(0) != 1 || rr::Hud::cycle_mini_dash(1) != 2 ||
            rr::Hud::cycle_mini_dash(2) != 0)
            throw std::runtime_error("MiniDash cycle");
        // Plan 16 Task 6: needle pen selection (0x00446c30 pen cache:
        // CreatePen(PS_SOLID, HiRes+2, 0x81172)). High resolution draws
        // 3-wide, low resolution 2-wide; the color is the raw COLORREF
        // 0x081172 (R 0x72, G 0x11, B 0x08), never a palette index.
        {
            const auto hi = rr::Hud::needle_pen(true);
            const auto lo = rr::Hud::needle_pen(false);
            if (hi.width != 3 || lo.width != 2) throw std::runtime_error("Needle pen width selection");
            if (hi.color != 0x721108 || lo.color != 0x721108) throw std::runtime_error("Needle pen color");
        }
        // Meter level shared by 0x00446b00/0x00446b40/0x00446b80:
        // ((current*2-2)*15)/(base-1)+1 clamped to 0..31, with the
        // divide-by-zero guards the original never needs (base > 1 live).
        if (rr::Hud::meter_level(0, 2500) != 0 || rr::Hud::meter_level(-5, 2500) != 0)
            throw std::runtime_error("Meter level clamps empty");
        if (rr::Hud::meter_level(10, 1) != 31 || rr::Hud::meter_level(10, 0) != 31 ||
            rr::Hud::meter_level(10, -3) != 31)
            throw std::runtime_error("Meter level guards degenerate base");
        if (rr::Hud::meter_level(2500, 2500) != 31) throw std::runtime_error("Meter level full stamina");
        if (rr::Hud::meter_level(5000, 2500) != 31) throw std::runtime_error("Meter level clamps overfull");
        if (rr::Hud::meter_level(1250, 2500) != 15) throw std::runtime_error("Meter level mid value");
        // 0x0044693a..0x004469a4: numeric speed uses separate integer
        // conversions from the full dial, with negative results clamped to 0.
        struct SpeedCase {
            int32_t velocity;
            int mph, kmh;
        };
        for (const auto c : {SpeedCase{0, 0, 0},
                             {127, 0, 0},
                             {128, 1, 1},
                             {4095, 38, 60},
                             {4096, 41, 65},
                             {8192, 82, 131},
                             {12345, 123, 196},
                             {-1, 0, 0},
                             {-128, 0, 0},
                             {-8192, 0, 0},
                             {INT32_MAX, 21495805, 34393288},
                             {INT32_MIN, 0, 0}}) {
            if (rr::Hud::numeric_speed(c.velocity, false) != c.mph ||
                rr::Hud::numeric_speed(c.velocity, true) != c.kmh)
                throw std::runtime_error("Mini dashboard numeric speed conversion");
        }
        // 0x00442dd0/0x00442e10/0x0044c780/0x00446c30 countdown banner:
        // 0xb4 ticks, "%d" of t/60+1, dashboard skipped on the start-value
        // frame, half palette until expiry.
        {
            const auto start = rr::countdown_banner(180, 180);
            if (start.dashboard || !start.dim || start.text != "4")
                throw std::runtime_error("Countdown start frame");
            for (const auto c :
                 {std::pair{179, "3"}, {120, "3"}, {119, "2"}, {60, "2"}, {59, "1"}, {1, "1"}}) {
                const auto b = rr::countdown_banner(c.first, 180);
                if (!b.dashboard || !b.dim || b.text != c.second) throw std::runtime_error("Countdown digit");
            }
            const auto live = rr::countdown_banner(0, 180);
            if (!live.dashboard || live.dim || !live.text.empty())
                throw std::runtime_error("Countdown expiry");
        }
        // 0x00415d90 fade-in / 0x00415e90 fade-out channel maps.
        {
            const auto half = rr::half_palette_lut();
            if (half[255] != 127 || half[1] != 0 || rr::identity_lut()[200] != 200)
                throw std::runtime_error("Palette maps");
            for (auto v : rr::fade_in_lut(0))
                if (v) throw std::runtime_error("Fade-in before the first upload");
            // First upload: counter 63, table[62] = 0.96^62 ~ 0.0795.
            if (rr::fade_in_lut(1)[255] != int(127 * std::pow(0.96, 62)))
                throw std::runtime_error("First fade-in upload");
            if (rr::fade_in_lut(63) != half || rr::fade_in_lut(99) != half)
                throw std::runtime_error("Fade-in ends on the half palette");
            for (int k = 1; k < 63; ++k)
                for (int v = 0; v < 256; ++v)
                    if (rr::fade_in_lut(k)[size_t(v)] > rr::fade_in_lut(k + 1)[size_t(v)])
                        throw std::runtime_error("Fade-in not monotone");
            if (rr::fade_out_lut(0) != rr::identity_lut())
                throw std::runtime_error("Fade-out before the first upload");
            // Cumulative truncation from 127: 127*0.96 = 121.92 -> 121, then 116.16 -> 116.
            if (rr::fade_out_lut(1)[255] != 121 || rr::fade_out_lut(2)[255] != 116)
                throw std::runtime_error("Fade-out truncation");
            if (rr::fade_out_lut(63)[255] >= rr::fade_out_lut(62)[255] && rr::fade_out_lut(62)[255] != 0)
                throw std::runtime_error("Fade-out not decreasing");
        }
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if (!assets.exists("ROADRASH.EXE")) {
            std::cout << "Meter levels pass; imported HUD corpus absent\n";
            return 0;
        }
        // Original palette uploads (local, git-ignored oracle capture
        // napa-palette: RR_ORACLE_PALETTE=1 logs every SetEntries on
        // DAT_00475424 with its call site; keys Pause/A/Escape/Escape at
        // frames 220/250/280/310 with a diagnostic game mode 1 override for
        // the pump frames). Native maps applied to native PALETTE.RAW must
        // reproduce every uploaded entry: fade-in 63, countdown expiry
        // full, pause pump half, resume full, quit prompt half, fade-out 63.
        {
            std::ifstream in(RR_SOURCE_DIR "/analysis/oracle/napa-palette-palette.raw", std::ios::binary);
            if (in) {
                const std::vector<unsigned char> d((std::istreambuf_iterator<char>(in)),
                                                   std::istreambuf_iterator<char>());
                auto word = [&](size_t at) {
                    if (at + 4 > d.size()) throw std::runtime_error("Palette log truncated");
                    return uint32_t(d[at]) | uint32_t(d[at + 1]) << 8 | uint32_t(d[at + 2]) << 16 |
                           uint32_t(d[at + 3]) << 24;
                };
                if (word(0) != 0x4c505252u || word(4) != 1) throw std::runtime_error("Palette log header");
                const auto pal = rr::Palette::from_raw(assets.read("DATA/PALETTE.RAW"));
                std::vector<std::pair<uint32_t, uint32_t>> sequence; // {site, mismatched entries}
                std::vector<uint32_t> fade_in_ms, fade_out_ms;
                int fade_in = 0, fade_out = 0;
                for (size_t at = 8; at < d.size();) {
                    const uint32_t site = word(at + 12), count = word(at + 24), countdown = word(at + 28),
                                   counter = word(at + 32), ms = word(at + 8);
                    if (word(at) != sequence.size() || count != 256 || word(at + 20) != 0)
                        throw std::runtime_error("Palette log record");
                    const unsigned char* e = &d[at + 36];
                    if (at + 36 + 1024 > d.size()) throw std::runtime_error("Palette log entries truncated");
                    at += 36 + 1024;
                    rr::ChannelLut lut;
                    if (site == 0x415e6c) {
                        lut = rr::fade_in_lut(++fade_in);
                        fade_in_ms.push_back(ms);
                        if (counter != uint32_t(64 - fade_in) || countdown != 180)
                            throw std::runtime_error("Fade-in counter or countdown");
                    } else if (site == 0x415f4c) {
                        lut = rr::fade_out_lut(++fade_out);
                        fade_out_ms.push_back(ms);
                        if (counter != uint32_t(64 - fade_out)) throw std::runtime_error("Fade-out counter");
                    } else if (site == 0x442f2f) {
                        lut = rr::identity_lut();
                        // 0x00442e10 uploads the full palette on the tick the countdown reaches 0.
                        if (countdown != 0 || rr::countdown_banner(0, 180).dim ||
                            !rr::countdown_banner(1, 180).dim)
                            throw std::runtime_error("Countdown expiry upload");
                    } else if (site == 0x443230)
                        lut = rr::half_palette_lut();
                    else if (site == 0x443350)
                        lut = rr::identity_lut();
                    else
                        throw std::runtime_error("Unexpected palette upload site");
                    uint32_t bad = 0;
                    for (size_t i = 0; i < 256; ++i)
                        for (size_t c = 0; c < 3; ++c)
                            if (e[4 * i + c] != lut[pal.rgbx[4 * i + c]]) {
                                ++bad;
                                break;
                            }
                    sequence.emplace_back(site, bad);
                }
                const std::vector<uint32_t> expected_order{0x442f2f, 0x443230, 0x443350, 0x443230};
                if (fade_in != 63 || fade_out != 63 || sequence.size() != 130)
                    throw std::runtime_error("Palette upload counts");
                for (size_t k = 0; k < expected_order.size(); ++k)
                    if (sequence[63 + k].first != expected_order[k])
                        throw std::runtime_error("Palette upload order");
                uint32_t mismatched = 0;
                for (const auto& s : sequence) mismatched += s.second;
                if (mismatched)
                    throw std::runtime_error("Native palette maps differ from the original uploads");
                // timeSetEvent(14 ms): 62 intervals; Wine's timer jitters by a few ms.
                const uint32_t in_span = fade_in_ms.back() - fade_in_ms.front(),
                               out_span = fade_out_ms.back() - fade_out_ms.front();
                if (std::abs(int(in_span) - 62 * 14) > 31 || std::abs(int(out_span) - 62 * 14) > 31)
                    throw std::runtime_error("Fade upload period");
                std::cout << "palette uploads vs original: " << sequence.size() << " uploads, 0/"
                          << sequence.size() * 256 << " entries differ; fade spans " << in_span << "/"
                          << out_span << " ms (62 x 14 = 868)\n";
            }
        }
        // Countdown digit raster: black at the rect, white at -2,-2, centred
        // across the 640-wide view starting 120 rows down; nothing above it.
        {
            rr::Hud hud(assets, assets.read("ROADRASH.EXE"), 0, false);
            rr::RgbImage image(640, 480, 0x01000000);
            hud.draw_countdown(image, "3");
            int white = 0, black = 0, minx = 640, maxx = 0, miny = 480;
            for (int y = 0; y < 480; ++y)
                for (int x = 0; x < 640; ++x) {
                    const uint32_t c = image.at(x, y);
                    if (c == 0x01000000) continue;
                    if (c == 0xffffff)
                        ++white;
                    else if (c == 0)
                        ++black;
                    else
                        throw std::runtime_error("Countdown ink colour");
                    minx = std::min(minx, x);
                    maxx = std::max(maxx, x);
                    miny = std::min(miny, y);
                }
            if (!white || !black) throw std::runtime_error("Countdown digit not drawn");
            if (miny < 118) throw std::runtime_error("Countdown digit above view top + 0x78 - 2");
            if (std::abs((minx + maxx) / 2 - 319) > 8)
                throw std::runtime_error("Countdown digit not centred");
            rr::RgbImage none(640, 480, 0x01000000);
            hud.draw_countdown(none, "");
            for (const auto c : none.pixels)
                if (c != 0x01000000) throw std::runtime_error("Empty countdown drew ink");
            // Original pixel comparison (local, git-ignored oracle capture
            // napa-dash-solo: post-0x00446c30 indexed frames of the demo
            // countdown; d2 "3", d36 "2", d70 "1"; white = index 255, shadow
            // = index 0). Region excludes clouds. Native "1" is the original
            // minus 4 white pixels; "3"/"2" differ by GDI hinting (one extra
            // top row, "3" one column right). Pins today's agreement.
            {
                const char* digit[] = {"3", "2", "1"};
                const char* frame[] = {"2", "36", "70"};
                for (int k = 0; k < 3; ++k) {
                    std::ifstream in(std::string(RR_SOURCE_DIR "/analysis/oracle/napa-dash-solo-d") +
                                         frame[k] + ".raw",
                                     std::ios::binary);
                    if (!in) break;
                    const std::vector<unsigned char> o((std::istreambuf_iterator<char>(in)),
                                                       std::istreambuf_iterator<char>());
                    if (o.size() != 640u * 480u) throw std::runtime_error("Countdown capture size");
                    rr::RgbImage n(640, 480, 0x01000000);
                    hud.draw_countdown(n, digit[k]);
                    int both = 0, ow = 0, nw = 0, shadow_both = 0, os = 0, ns = 0;
                    for (int y = 130; y < 222; ++y)
                        for (int x = 295; x < 345; ++x) {
                            const bool a = o[size_t(y) * 640 + size_t(x)] == 255, b = n.at(x, y) == 0xffffff;
                            const bool c = o[size_t(y) * 640 + size_t(x)] == 0, d = n.at(x, y) == 0;
                            both += a && b;
                            ow += a;
                            nw += b;
                            shadow_both += c && d;
                            os += c;
                            ns += d;
                        }
                    if (k == 2 && (both != nw || both * 100 < ow * 99))
                        throw std::runtime_error("Countdown 1 differs from the original capture");
                    if (both * 10 < (ow + nw - both) * 8)
                        throw std::runtime_error("Countdown digit drifted from the original capture");
                    std::cout << "countdown " << digit[k] << " vs original: white " << both << "/"
                              << (ow + nw - both) << " shadow " << shadow_both << "/"
                              << (os + ns - shadow_both) << "\n";
                }
            }
            // 0x004431d0 message: small Badloc 0x20, same rect, two lines.
            rr::RgbImage msg(640, 480, 0x01000000);
            hud.draw_message(msg, "Game paused.\nHit any key to continue.");
            int mtop = 480, mbottom = 0, mink = 0;
            for (int y = 0; y < 480; ++y)
                for (int x = 0; x < 640; ++x)
                    if (msg.at(x, y) != 0x01000000) {
                        ++mink;
                        mtop = std::min(mtop, y);
                        mbottom = std::max(mbottom, y);
                    }
            if (!mink || mtop < 118 || mbottom < mtop + 0x20 || mbottom > 120 + 2 * 0x20 + 4)
                throw std::runtime_error("Pause message placement");
            // Low resolution: 0x48 face at +60 in 320x240, shadow 1, doubled.
            hud.set_low_mode(true);
            rr::RgbImage low(640, 480, 0x01000000);
            hud.draw_countdown(low, "3");
            int ink = 0, top = 480;
            for (int y = 0; y < 480; y += 2)
                for (int x = 0; x < 640; x += 2) {
                    const uint32_t c = low.at(x, y);
                    if (low.at(x + 1, y) != c || low.at(x, y + 1) != c || low.at(x + 1, y + 1) != c)
                        throw std::runtime_error("Low countdown not pixel-doubled");
                    if (c != 0x01000000) {
                        ++ink;
                        top = std::min(top, y);
                    }
                }
            if (!ink || top < 118) throw std::runtime_error("Low countdown placement");
        }
        const auto exe = assets.read("ROADRASH.EXE");
        const auto physics = rr::read_physics_tables(exe);
        const auto course = rr::read_course(assets.read("DATA/COURSES/NAPA.CRS"));
        const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        rr::Race race(rr::make_route(course, false), rr::make_route(course, true), physics, specs[0], 90, {},
                      0);
        const std::vector<std::string> names{"Player"};
        // 0x00438550 / 0x00446b80: the dashboard names the nearest opponent.
        {
            rr::Race field(rr::make_route(course, false), rr::make_route(course, true), physics, specs[0], 90,
                           rr::original_opponents(exe, specs, 0, 3), 0);
            const std::vector<std::string> field_names{"Player", "Natasha", "Viper", "Sledge"};
            auto at = [&](size_t i, int32_t q8) { field.riders()[i].bike.i32(0x1c) = q8; };
            const int32_t me = 400 * 256;
            at(0, me);
            // Ahead 10 samples, behind 4: 4*2 < 10, the behind rider wins (gap > 0).
            at(1, me + 10 * 256);
            at(2, me - 4 * 256);
            at(3, me - 40 * 256);
            auto n = rr::nearest_opponent(field);
            if (n.rider != 2 || n.gap != 4 * 256) throw std::runtime_error("Nearest behind opponent rule");
            // Behind 6: 6*2 >= 10, the ahead rider wins (gap <= 0).
            at(2, me - 6 * 256);
            n = rr::nearest_opponent(field);
            if (n.rider != 1 || n.gap != -10 * 256) throw std::runtime_error("Nearest ahead opponent rule");
            // Nobody ahead: the nearest behind is named.
            at(1, me - 20 * 256);
            n = rr::nearest_opponent(field);
            if (n.rider != 2 || n.gap != 6 * 256)
                throw std::runtime_error("Nearest opponent without one ahead");
            field.riders()[2].stamina.current = field.riders()[2].stamina.base / 2;
            const auto s = rr::Hud(assets, exe, 0, false).state(field, "Player", field_names);
            if (s.target != "Viper" || s.target_gap != 6 * 256 ||
                s.target_stamina !=
                    rr::Hud::meter_level(field.riders()[2].stamina.current, field.riders()[2].stamina.base) ||
                s.target_stamina == 0)
                throw std::runtime_error("Dashboard target is not the nearest opponent");
            if (rr::nearest_opponent(race).rider != -1 ||
                !rr::Hud(assets, exe, 0, false).state(race, "Player", names).target.empty())
                throw std::runtime_error("Solo race names a target");
            // Plan 16 Task 6: no target means a normalized gap of 0 (the
            // original leaves a stale -0x20000000 with its null target,
            // which no draw path reads).
            if (rr::nearest_opponent(race).gap != 0)
                throw std::runtime_error("Solo race gap is not normalized");
            // Tied opponents: the first minimal slot wins, and the 0 gap
            // counts as ahead (0x00438550 takes the else-if branch for 0).
            at(1, me + 10 * 256);
            at(2, me + 10 * 256);
            at(3, me - 40 * 256);
            n = rr::nearest_opponent(field);
            if (n.rider != 1 || n.gap != -10 * 256)
                throw std::runtime_error("Tied opponents do not name the first slot ahead");
            // Strict behind rule: behind*2 == ahead still names the ahead
            // rider (0x00438550 uses strict <).
            at(2, me - 5 * 256);
            n = rr::nearest_opponent(field);
            if (n.rider != 1 || n.gap != -10 * 256)
                throw std::runtime_error("Behind-half boundary does not name the ahead rider");
            // Naming without a name row: the whole target block stays clear,
            // so an empty target always carries gap 0 and stamina 0.
            {
                const std::vector<std::string> short_names{"Player"};
                const auto s = rr::Hud(assets, exe, 0, false).state(field, "Player", short_names);
                if (!s.target.empty() || s.target_gap != 0 || s.target_stamina != 0)
                    throw std::runtime_error("Target naming without a name row");
            }
        }
        // 11a Task 2: the tachometer needle angle follows bike +0x2dc.
        race.riders()[0].bike.i32(0x2dc) = 0x3000;
        const auto needle = rr::Hud(assets, exe, 0, false).state(race, "Player", names);
        if (needle.rpm_angle != 0x3000 / 44 - 1024 + 4096)
            throw std::runtime_error("RPM needle does not follow +0x2dc");
        // Plan 16 Task 6: needle pen width by resolution. Parked-left angle
        // 3072 (q3, i=0: cos -65536, sin 0) lays each spine horizontal.
        {
            rr::HudState parked;
            parked.player = "P";
            parked.rpm_angle = 3072;
            parked.speed_angle = 3072;
            rr::Hud hud(assets, exe, 0, false);
            // High mode: tach (388,441) radii (28,20) -> spine y441, 3-wide
            // band rows 440..442; speedo (250,449) radii (44,28) -> y449.
            const auto full = hud.compose(parked, physics, rr::DashMode::full);
            if (full.at(370, 441) != 0x721108 || full.at(370, 440) != 0x721108 ||
                full.at(370, 442) != 0x721108)
                throw std::runtime_error("High-resolution tach needle is not 3-wide");
            if (full.at(230, 449) != 0x721108 || full.at(230, 448) != 0x721108 ||
                full.at(230, 450) != 0x721108)
                throw std::runtime_error("High-resolution speedo needle is not 3-wide");
            // Low mode: halved-high tach origin (194,220), rx 14 -> spine
            // y220 x180..194; the width-2 pen paints rows 219..220 only.
            hud.set_low_mode(true);
            const auto low = hud.compose(parked, physics, rr::DashMode::full);
            if (low.at(185, 220) != 0x721108 || low.at(185, 219) != 0x721108)
                throw std::runtime_error("Low-resolution needle spine is missing");
            if (low.at(185, 221) == 0x721108) throw std::runtime_error("Low-resolution needle is not 2-wide");
            hud.set_low_mode(false);
        }
        // 0x00446a40 via the 0x00438450 +0x1c -> +0x44 snapshot: tenths travelled.
        const auto grid = race.riders()[0].bike.i32(0x1c);
        race.riders()[0].bike.i32(0x1c) = (50 + 33 * 12) * 256;
        if (rr::Hud(assets, exe, 0, false).state(race, "Player", names).distance != 12)
            throw std::runtime_error("Distance readout ignores route position");
        race.riders()[0].bike.i32(0x1c) = grid;
        const auto parked_velocity = race.riders()[0].bike.i32(0xec);
        race.riders()[0].bike.i32(0xec) = 8192;
        if (rr::Hud(assets, exe, 0, false).state(race, "Player", names).speed != 82 ||
            rr::Hud(assets, exe, 0, true).state(race, "Player", names).speed != 131)
            throw std::runtime_error("Mini speed state ignores bike velocity or units");
        race.riders()[0].bike.i32(0xec) = -8192;
        if (rr::Hud(assets, exe, 0, false).state(race, "Player", names).speed != 0 ||
            rr::Hud(assets, exe, 0, true).state(race, "Player", names).speed != 0)
            throw std::runtime_error("Mini speed state does not clamp reverse");
        race.riders()[0].bike.i32(0xec) = parked_velocity;
        size_t variants = 0;
        for (bool metric : {false, true})
            for (int bike : {0, 5, 10}) {
                rr::Hud hud(assets, exe, bike, metric);
                auto state = hud.state(race, "Player", names);
                if (state.place != 1 || state.stamina != 31 || state.condition != 31)
                    throw std::runtime_error("Starting dashboard values");
                for (int value : {0, 1, 15, 31}) {
                    state.stamina = state.condition = state.target_stamina = value;
                    const auto image = hud.compose(state, physics, rr::DashMode::full);
                    if (image.width != 640 || image.height != 480 || image.at(0, 0) != 0x01000000 ||
                        image.at(250, 449) != 0x721108)
                        throw std::runtime_error("Dashboard composition bounds/needle");
                    ++variants;
                }
                // 0x00446c30 draws the opponent meter (layout 50, 456,424, 51x31)
                // with no target, so the dashboard hole never shows the road.
                state.target.clear();
                state.target_stamina = 0;
                state.stamina = 0;
                const auto empty = hud.compose(state, physics, rr::DashMode::full);
                for (int y = 424; y < 455; ++y)
                    for (int x = 456; x < 507; ++x)
                        if (empty.at(x, y) == 0x01000000)
                            throw std::runtime_error("Opponent meter hole is transparent");
                // 0x0044ca50: vertical meters empty from the top (colour, then black overlay).
                state.stamina = 31;
                const auto full = hud.compose(state, physics, rr::DashMode::full);
                size_t changed = 0;
                for (int y = 424; y < 455; ++y)
                    for (int x = 134; x < 185; ++x) changed += full.at(x, y) != empty.at(x, y);
                // MiniDash 1 (0x00446899/0x004468e1 skip tach/speedo; 0x00446c76 skips
                // name bars): numeric readout only, no 3x3 needle pixels at the dial.
                const auto mini = hud.compose(state, physics, rr::DashMode::mini);
                if (mini.width != 640 || mini.height != 480)
                    throw std::runtime_error("Mini dashboard bounds");
                if (mini.at(250, 449) == 0x721108) throw std::runtime_error("Mini mode still draws needles");
                auto moved = state;
                moved.distance = state.distance + 79;
                if (hud.compose(moved, physics, rr::DashMode::mini).pixels != mini.pixels)
                    throw std::runtime_error("Mini speed readout changes with distance");
                moved = state;
                moved.speed = 88;
                if (hud.compose(moved, physics, rr::DashMode::mini).pixels == mini.pixels)
                    throw std::runtime_error("Mini speed readout ignores speed");
                if (hud.compose(moved, physics, rr::DashMode::full).pixels != full.pixels)
                    throw std::runtime_error("Numeric speed changes full dashboard");
                // MiniDash 2: 0x00446c30 draws nothing (fully transparent frame).
                const auto off = hud.compose(state, physics, rr::DashMode::off);
                for (int y = 0; y < 480; y += 7)
                    for (int x = 0; x < 640; x += 11)
                        if (off.at(x, y) != 0x01000000) throw std::runtime_error("Off mode is not empty");
                // Low-mode assets (0x004457f0 choices 1/2): 320-wide dashboard blits
                // through the same 640x480 frame without transparency holes.
                hud.set_low_mode(true);
                const auto low = hud.compose(state, physics, rr::DashMode::full);
                if (low.width != 640 || low.height != 480) throw std::runtime_error("Low dashboard bounds");
                {
                    size_t lit = 0, total = 0;
                    for (int y = 420; y < 480; ++y)
                        for (int x = 0; x < 640; ++x) {
                            ++total;
                            lit += low.at(x, y) != 0x01000000;
                        }
                    if (lit < total / 10) throw std::runtime_error("Low dashboard mostly transparent");
                }
                hud.set_low_mode(false);
                // Plan 19 Task 5: compose at scale 2 is 1280 wide and downsamples
                // (pixel (2x+1,2y+1)) to the scale-1 image on >=98% of pixels.
                const auto hi = hud.compose(state, physics, rr::DashMode::full, 2);
                if (hi.width != 1280 || hi.height != 960) throw std::runtime_error("Scaled dashboard bounds");
                size_t same = 0, stotal = 0;
                for (int y = 0; y < 480; ++y)
                    for (int x = 0; x < 640; ++x) {
                        ++stotal;
                        same += hi.at(2 * x + 1, 2 * y + 1) == full.at(x, y);
                    }
                if (same * 100 < 98 * stotal)
                    throw std::runtime_error("Scaled dashboard downsample mismatch");
            }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
