// Natural-frame display-list check: the original's own pending display nodes for a captured race
// frame (oracle dump_scene) are converted to native DrawNodes, resources identified by their mip-0
// hash, and rendered over the original frame. Every pixel must equal the original final pixel.
//
// --native-actors mode (plan 11a Task 3 Step 2): per-node original comparison of player/opponent
// actor nodes against oracle dispatch records (actor.sel.*/actor.bb.* TSV keys, scene v4+). The
// display list streams through a 250-node buffer (0x00450470 flushes via 0x00450420 mid-projection),
// so the dump holds the post-flush tail; each surviving bb call maps to dump index nafter-1.
// Per bb call the test checks, each against its own ground truth:
//   descriptor import: sel 20B == native playback bank bytes (render/duration/control);
//   orientation: native actor_angles view vs sel view (+0xd0);
//   inputs: bb lateral == (sel f50 + sign-rounding) >> 8;
//   selection+projection: select_actor_view + actor_node with bb lateral/ground/pos/clip/occ_row
//     vs dump node frame/bank/4 view points/flip;
//   palette: traced colour rule (cop 0xde / player row byte / grid slot; shadow 0xdf iff cop)
//     vs dump node colour (+0x38).
#include "actor_nodes.hpp"
#include "course_scene.hpp"
#include "terrain_ground.hpp"
#include "course.hpp"
#include "golden.hpp"
#include "rider_sprites.hpp"
#include "road.hpp"
#include "scene_dump.hpp"
#include "texture_library.hpp"
#include "view.hpp"
#include "world_renderer.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>
#include <memory>
#include <set>
#include <tuple>

namespace {
const char* const course_files[5] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"}; // 0x0046df58
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int32_t blob_i32(const std::vector<uint8_t>& b, size_t p) {
    return int32_t(uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
                   uint32_t(b[p + 3]) << 24);
}
int64_t key(const std::map<std::string, int64_t>& f, const std::string& k, int64_t dflt = 0) {
    const auto it = f.find(k);
    return it == f.end() ? dflt : it->second;
}

// Expected palette colour for a scheduler bike object (traced 0x00446190 /
// 0x004460c0 via 0x00446380): the cop bike is 0xde; the player bike
// (DAT_004c7f70) takes the rider-table row byte (rider half +0x1e0 points at
// the 0x00463c78 row, +5); other bikes take their 1-based grid slot in
// DAT_004c7f74, or 0 when the slot equals the player row byte.
int32_t actor_colour(uint32_t bike_addr, const rr::scene::Memory& mem, uint8_t player_row_byte) {
    const uint32_t cop = mem.u32(0x004642dc);
    if (cop != 0 && bike_addr == cop) return 0xde;
    if (bike_addr == mem.u32(0x004c7f70)) return int32_t(player_row_byte);
    for (int slot = 1; slot <= 15; ++slot) {
        uint32_t entry = 0x004c7f74 + uint32_t(slot - 1) * 4;
        if (!mem.has(entry)) continue;
        if (mem.u32(entry) == bike_addr) return slot == player_row_byte ? 0 : slot;
    }
    return -1; // bike not on the grid: caller reports it
}

// Rider-table row byte (+5) for the player: the player half (+0x2fc of the
// player bike DAT_004642d8) carries the 0x00463c78 row pointer at +0x1e0.
uint8_t player_row_byte(const rr::scene::Dump& dump, const std::vector<uint8_t>& exe) {
    const uint32_t player = dump.memory.u32(0x004642d8);
    for (const auto& r : dump.riders) {
        if (r.address != player || r.half.empty()) continue;
        const uint32_t row = uint32_t(blob_i32(r.half, 0x1e0));
        if (dump.memory.has(row + 5)) return dump.memory.u8(row + 5);
        const size_t raw = 0x61600 + (row - 0x00463000) + 5; // .data file offset
        if (row >= 0x00463000 && raw < exe.size()) return exe[raw];
    }
    return 0;
}

int native_actors(const std::string& tsv, const rr::Assets& assets) {
    const auto golden = rr::golden::load(tsv);
    const std::string stem = tsv.substr(0, tsv.size() - 4);
    if (golden.exe != "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2") {
        std::cerr << "Oracle capture is from a different executable\n";
        return 1;
    }
    const int course = int(golden.frames.at(0).at("course"));
    const auto exe = assets.read("ROADRASH.EXE");
    const auto depths = rr::read_depth_table(exe);
    const auto course_data =
        rr::read_course(assets.read(std::string("DATA/COURSES/") + course_files[course] + ".CRS"));
    const auto routes = {rr::make_route(course_data, false), rr::make_route(course_data, true)};
    rr::ActorNodes actors(assets, exe, true);
    const auto& bank = actors.library().playback[0];
    int failures = 0, frames = 0, nodes = 0;
    for (size_t f = 0; f < golden.frames.size(); ++f) {
        const auto& frame = golden.frames[f];
        if (!frame.count("scene.file")) continue;
        const int64_t scene_file = frame.at("scene.file");
        const auto dump = rr::scene::load(stem + "-scene" + std::to_string(scene_file) + ".raw");
        if (dump.version < 4 || dump.riders.empty() || !frame.count("actor.sel.count")) {
            std::cout << "frame " << scene_file << ": skip (needs v4 dump + actor hooks recapture)\n";
            continue;
        }
        const uint8_t prow = player_row_byte(dump, exe);
        const uint32_t cam_pos = uint32_t(frame.at("camera.position"));
        const int32_t cam_x = int32_t(frame.at("camera.x")), cam_h = int32_t(frame.at("camera.height"));
        bool frame_ok = false;
        std::string frame_error;
        int frame_checked = 0;
        for (const auto& route : routes) {
            if ((cam_pos >> 8) >= route.size()) continue;
            rr::Camera camera{cam_x - route[cam_pos >> 8].offset, cam_h};
            const auto slices = rr::project_road(route, cam_pos, camera, depths);
            std::map<uint32_t, size_t> by_sample;
            for (size_t i = 0; i < slices.size(); ++i) by_sample[slices[i].sample] = i;
            // Half objects by scheduler address (halves carry the animation).
            std::map<uint32_t, const rr::scene::RiderObjects*> halves;
            for (const auto& r : dump.riders) halves[r.address] = &r;
            // Bike address per half (via bike +0x2fc).
            std::map<uint32_t, uint32_t> bike_of_half;
            for (const auto& r : dump.riders) {
                if (r.bike.size() < 0x300 || blob_i32(r.bike, 0xc4) != 4) continue;
                bike_of_half[uint32_t(blob_i32(r.bike, 0x2fc))] = r.address;
            }
            const size_t nsel = size_t(key(frame, "actor.sel.count")),
                         nbb = size_t(key(frame, "actor.bb.count"));
            bool usable = true;
            std::string diff;
            int checked = 0;
            // Group sel records by half; bb records pair with sels in dispatch order.
            for (size_t b = 0; b < nbb && usable; ++b) {
                const int32_t pos = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".pos"));
                const int32_t clip = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".clip"));
                const int32_t lateral = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".lateral"));
                const int32_t ground = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".ground"));
                const int64_t nafter = key(frame, "actor.bb." + std::to_string(b) + ".nafter", -1);
                if (nafter < 1 || nafter > int64_t(dump.nodes.size()))
                    continue; // flushed: in photo, not dump
                // Find the half with this position (scheduler halves mirror +0x1c).
                const rr::scene::RiderObjects* half = nullptr;
                for (const auto& r : dump.riders) {
                    if (r.bike.size() < 0x30) continue;
                    if (blob_i32(r.bike, 0x1c) == pos && blob_i32(r.bike, 0xc4) == 8) {
                        half = &r;
                        break;
                    }
                }
                if (!half) {
                    usable = false;
                    diff = "no half for bb pos " + std::to_string(pos);
                    break;
                }
                // The half's sel records (shadow then body): find by bike address.
                size_t sel = nsel;
                int seen = 0;
                for (size_t s = 0; s < nsel; ++s) {
                    if (uint32_t(key(frame, "actor.sel." + std::to_string(s) + ".bike")) != half->address)
                        continue;
                    if ((seen++ == 0) != (clip == 0)) continue;
                    sel = s;
                    break;
                }
                if (sel >= nsel) {
                    usable = false;
                    diff = "no sel for half pos " + std::to_string(pos);
                    break;
                }
                const std::string sp = "actor.sel." + std::to_string(sel) + ".";
                // (1) Descriptor import parity: sel 20B vs native bank bytes.
                // The dump's +0x80/+0x81 is post-draw and may lag dispatch by
                // a frame advance (0x0043ae90 runs for RRAN halves), so find
                // the bank entry whose bytes equal the sel record exactly
                // (unique match required); the anim still comes from the dump.
                const int8_t anim = half->bike[0x80];
                if (anim < 0 || size_t(anim) >= bank.size() || !bank[size_t(anim)]) {
                    usable = false;
                    diff = "half anim out of range pos " + std::to_string(pos);
                    break;
                }
                const int64_t d0 = key(frame, sp + "d0"), d1 = key(frame, sp + "d1");
                const int64_t d2 = key(frame, sp + "d2"), d3 = key(frame, sp + "d3");
                const auto& seq = bank[size_t(anim)];
                int found = -1, hits = 0;
                for (size_t di = 0; di < seq->size(); ++di) {
                    const uint8_t* cb = seq->at(di).render.data();
                    if (d0 == (int64_t(uint32_t(cb[0]) | uint32_t(cb[1]) << 8 | uint32_t(cb[2]) << 16 |
                                       uint32_t(cb[3]) << 24)) &&
                        d1 == (int64_t(uint32_t(cb[4]) | uint32_t(cb[5]) << 8 | uint32_t(cb[6]) << 16 |
                                       uint32_t(cb[7]) << 24)) &&
                        (d2 & 0xffff) == (int64_t(cb[8]) | (int64_t(cb[9]) << 8)) &&
                        (d3 & 0xff) == int64_t(seq->at(di).duration) &&
                        ((d3 >> 8) & 0xff) == int64_t(seq->at(di).control)) {
                        found = int(di);
                        ++hits;
                    }
                }
                if (hits != 1) {
                    usable = false;
                    diff = "descriptor import pos " + std::to_string(pos);
                    break;
                }
                const auto& descriptor = seq->at(size_t(found));
                // (2) Input consistency: bb lateral == (sel f50 + sign-rounding) >> 8.
                const int32_t f50 = int32_t(key(frame, sp + "f50"));
                const int32_t want_lat = (f50 + ((f50 >> 31) & 0xff)) >> 8;
                if (want_lat != lateral) {
                    usable = false;
                    diff = "lateral consistency pos " + std::to_string(pos);
                    break;
                }
                // (3) Orientation: native view angle vs sel view (+0xd0).
                auto sit = by_sample.find(uint32_t(pos) >> 8);
                if (sit == by_sample.end() || sit->second >= slices.size() - 1)
                    continue; // not in slice window
                const auto &near = slices[sit->second], far = slices[sit->second + 1];
                if (near.count == 2) continue; // split roadways: game path only
                const auto& sample = route.at(near.sample);
                rr::ActorOrientation orientation;
                orientation.depth = near.depth;
                orientation.position = pos;
                orientation.road_x = near.road[0].road_x + camera.x;
                orientation.lateral_q8 = blob_i32(half->bike, 0x28) - sample.offset * 256;
                orientation.road_heading = near.road[0].heading;
                orientation.heading = blob_i32(half->bike, 0x5c);
                orientation.roll = blob_i32(half->bike, 0x60);
                orientation.rider = true; // halves are type 8 (0x00413ae0 rider rule)
                const auto angles = rr::actor_angles(orientation, camera.x);
                if (angles.view_angle != int32_t(key(frame, sp + "view"))) {
                    usable = false;
                    diff = "view angle pos " + std::to_string(pos) +
                           " native=" + std::to_string(angles.view_angle) +
                           " orig=" + std::to_string(key(frame, sp + "view"));
                    break;
                }
                // (4) Full dispatch parity: ActorNodes::rider (shadow axis +
                // body with select roll, yaw clamp, 0x00450ad0 rotation) vs
                // the dump node for this bb call. Ground is the shadow
                // ground (+0x4c); rider() adds vertical>>8 for the body.
                // Colour from the traced rule, via the half's bike.
                uint32_t bike_addr = 0;
                for (const auto& r : dump.riders) {
                    if (r.bike.size() < 0x300 || blob_i32(r.bike, 0xc4) != 4) continue;
                    if (uint32_t(blob_i32(r.bike, 0x2fc)) == half->address) {
                        bike_addr = r.address;
                        break;
                    }
                }
                const int32_t colour = actor_colour(bike_addr, dump.memory, prow);
                if (colour < 0) {
                    usable = false;
                    diff = "colour rule pos " + std::to_string(pos);
                    break;
                }
                rr::ActorProjection projection;
                projection.road_x = orientation.road_x;
                projection.far_x = far.road[0].road_x + camera.x;
                projection.road_y = near.road[0].world_y;
                projection.far_y = far.road[0].world_y;
                projection.lateral = lateral;
                projection.ground = ground;
                projection.depth = near.depth;
                projection.fraction = uint8_t(uint32_t(pos));
                projection.occlusion = int32_t(key(frame, "actor.bb." + std::to_string(b) + ".occ_row"));
                // Body roll is the dispatch +0xbc (sel key), already yaw-
                // clamped by 0x004463e0 — not the +0x60-derived view roll.
                const int32_t sel_roll = int32_t(key(frame, sp + "bc"));
                const auto made = actors.rider(descriptor, 0, colour, orientation, projection, camera, depths,
                                               blob_i32(half->bike, 0x20), &sel_roll);
                const bool is_shadow = clip == 0;
                if (made.size() != 2) {
                    usable = false;
                    diff = "dispatch node count pos " + std::to_string(pos);
                    break;
                }
                const auto& node = made[is_shadow ? 0 : 1];
                const auto& dumped = dump.nodes[size_t(nafter - 1)];
                const auto& cell = actors.library()
                                       .views[0]
                                       .at(rr::select_actor_view(descriptor, actors.library().views[0],
                                                                 angles.yaw, angles.roll)
                                               .group)
                                       .at(rr::select_actor_view(descriptor, actors.library().views[0],
                                                                 angles.yaw, angles.roll)
                                               .subframe);
                if (dumped.u32(8) != 0x00466684 || int32_t(dumped.u32(0x34)) != int32_t(cell.frame)) {
                    usable = false;
                    diff = "frame/bank pos " + std::to_string(pos);
                    break;
                }
                const int32_t shadow_colour = (colour == 0xde) ? 0xdf : 0xe0;
                const int32_t want_colour = is_shadow ? shadow_colour : colour;
                if (int32_t(dumped.u32(0x38)) != want_colour) {
                    usable = false;
                    diff = "palette pos " + std::to_string(pos);
                    break;
                }
                for (int k = 0; k < 4; ++k) {
                    if (node.view_points[size_t(k)].x != int32_t(dumped.u32(0x44 + 8 * size_t(k))) ||
                        node.view_points[size_t(k)].y != int32_t(dumped.u32(0x48 + 8 * size_t(k)))) {
                        usable = false;
                        diff = "view point " + std::to_string(k) + " pos " + std::to_string(pos);
                        break;
                    }
                }
                if (!usable) break;
                if (node.flip_u != (dumped.bytes[0x64] != 0) || node.flip_v != (dumped.bytes[0x65] != 0)) {
                    usable = false;
                    diff = "flip pos " + std::to_string(pos);
                    break;
                }
                ++checked;
            }
            if (usable && checked > 0) {
                frame_ok = true;
                frame_checked = checked;
                nodes += checked;
                break;
            }
            if (frame_error.empty()) frame_error = diff + " (checked " + std::to_string(checked) + ")";
        }
        ++frames;
        if (!frame_ok) {
            ++failures;
            std::cout << "frame " << scene_file << ": MISMATCH " << frame_error << "\n";
        } else {
            std::cout << "frame " << scene_file << ": " << frame_checked << " actor nodes match\n";
        }
    }
    std::cout << frames << " frames, " << nodes << " actor nodes, " << failures << " failures\n";
    return failures ? 1 : 0;
}

// --native-terrain: native scene construction against the original's dumped
// display list. Each frame's sub-slice terrain values (drawn by the original
// from its live random stream, which CourseScene's isolated stream does not
// reproduce) are loaded into the native scene for the dumped strips. Every
// original terrain node (road GLOBAL CELs, WATER128, family categories 1, 3,
// 5 and 6) must then appear natively with identical texture, Q16 points and
// flips. The dump holds only the post-flush tail, so native nodes without an
// original counterpart are expected. Scenery and actors are reported only.
// Riders' ground (+0x24) is checked against 0x00439b10 on the same values.
int native_terrain(const std::string& tsv, const rr::Assets& assets, bool check_ground) {
    const auto golden = rr::golden::load(tsv);
    const std::string stem = tsv.substr(0, tsv.size() - 4);
    const int course = int(golden.frames.at(0).at("course"));
    const auto exe = assets.read("ROADRASH.EXE");
    const auto depths = rr::read_depth_table(exe);
    const auto data =
        rr::read_course(assets.read(std::string("DATA/COURSES/") + course_files[course] + ".CRS"));
    rr::CourseScene scene(assets, exe, data, course_files[course]);
    const auto lib = rr::testlib::load_texture_library(assets, course_files[course]);
    const std::array<std::vector<rr::RouteSample>, 2> routes = {rr::make_route(data, false),
                                                                rr::make_route(data, true)};
    std::set<std::string> terrain = {"PLAIN",    "WHITE",    "YELLOW",   "START160", "START162",
                                     "SIDCRSS0", "SIDLANE0", "SIDLANE1", "FINISH",   "WATER128"};
    for (const auto& [id, family] : scene.families()) {
        std::function<void(uint32_t, int)> walk = [&](uint32_t offset, int depth) {
            if (const auto g = family.groups.find(offset); g != family.groups.end())
                for (const auto& cell : g->second.cells) terrain.insert(cell.texture.name);
            const auto n = family.nodes.find(offset);
            if (n == family.nodes.end() || n->second.tag || depth > 3) return;
            for (const auto child : n->second.children) walk(child, depth + 1);
        };
        if (const auto root = family.nodes.find(0); root != family.nodes.end())
            for (const uint32_t category : {1u, 3u, 5u, 6u})
                if (category < root->second.children.size()) walk(root->second.children[category], 1);
    }
    // Native nodes keyed by points and flips; textures compare by the dump's
    // mip-0 hash (several disc resources share pixels under other names).
    using Key = std::tuple<std::array<int32_t, 8>, bool, bool>;
    size_t frames = 0, matched = 0, missing = 0, others = 0, grounds = 0, ground_failures = 0;
    for (const auto& f : golden.frames) {
        if (!f.count("scene.file")) continue;
        const auto dump = rr::scene::load(stem + "-scene" + std::to_string(f.at("scene.file")) + ".raw");
        const auto& route = routes[f.at("camera.half") ? 1 : 0];
        // Sub-slice values of a dumped ring entry, as native TerrainSlice
        // inputs (0x00454240 inverted for the wall offsets).
        auto load = [&](uint32_t sub, rr::TerrainSlice& t, size_t& sample) {
            if (!dump.memory.has(sub - 0x30 + 8) || !dump.memory.has(sub + 0x160)) return false;
            sample = dump.memory.u32(sub - 0x30 + 8) >> 8;
            if (sample >= route.size()) return false;
            const auto& rs = route[sample];
            auto I = [&](uint32_t o) { return dump.memory.i32(sub + o); };
            const int32_t cc = I(0xcc);
            const int type = dump.memory.u8(sub + 0x3c);
            if (cc != rs.offset) return false;
            t = scene.terrain(rs.section, rs.local);
            if (type != t.type)
                throw std::runtime_error("Sub-slice type differs at sample " + std::to_string(sample));
            if (type == 4)
                for (int i = 0; i < 6; ++i) {
                    t.points[size_t(i)] = {I(0xf0 + 8 * uint32_t(i)) - cc, I(0xf4 + 8 * uint32_t(i))};
                    t.shades[size_t(i)] = I(0x120 + 4 * uint32_t(i));
                }
            if (type == 1 || type == 3) {
                const int32_t l = -rs.path.left, r = rs.path.right;
                int32_t el = l - rs.path.shoulder_left, er = r + rs.path.shoulder_right;
                if (type == 3) {
                    if (I(0x160) < 1)
                        er = 2 * er - r;
                    else
                        el = 2 * el - l;
                }
                t.walls[0] = {el - (I(0x14c) - cc), el - (I(0x150) - cc), I(0x154) >= 1 ? I(0x154) : 0};
                t.walls[1] = {(I(0x158) - cc) - er, (I(0x15c) - cc) - er, I(0x160) >= 1 ? I(0x160) : 0};
            }
            return true;
        };
        const uint32_t base = 0x004c0a5c - 31 * 0x224;
        for (uint32_t k = 0; k < 33; ++k) {
            const uint32_t strip = base + k * 0x224;
            if (!dump.memory.has(strip + 0x14)) continue;
            const uint32_t record = dump.memory.u32(strip + 0x14);
            if (!dump.memory.has(record + 0xc)) continue;
            rr::TerrainSlice t;
            size_t sample = 0;
            if (load(dump.memory.u32(record + 0xc), t, sample))
                scene.set_terrain(route[sample].section, route[sample].local, t);
        }
        const uint32_t position = uint32_t(f.at("camera.position"));
        const rr::Camera camera{int32_t(f.at("camera.x")) - route[position >> 8].offset,
                                int32_t(f.at("camera.height"))};
        std::multimap<Key, rr::DrawNode> native;
        for (const auto& n :
             scene.draw_nodes(route, rr::project_road(route, position, camera, depths), camera, depths)) {
            std::array<int32_t, 8> v{};
            for (size_t k = 0; k < 4; ++k) {
                v[2 * k] = n.view_points[k].x;
                v[2 * k + 1] = n.view_points[k].y;
            }
            native.insert({{v, n.flip_u, n.flip_v}, n});
        }
        size_t frame_missing = 0;
        for (const auto& s : dump.nodes) {
            if (s.u32(8) == 0x00466684) continue; // dynamic rider resource
            std::string name = s.bytes[0x66] ? "flat" + std::to_string(s.bytes[0x67]) : "";
            if (name.empty()) {
                const auto* t = lib.find(s.hash, s.size);
                name = t ? t->name : "?";
            }
            if (!terrain.count(name) && name != "flat253") {
                ++others;
                continue;
            }
            std::array<int32_t, 8> v{};
            for (size_t k = 0; k < 4; ++k) {
                v[2 * k] = int32_t(s.u32(0x44 + 8 * k));
                v[2 * k + 1] = int32_t(s.u32(0x48 + 8 * k));
            }
            const auto range = native.equal_range({v, s.bytes[0x64] != 0, s.bytes[0x65] != 0});
            auto it = range.first;
            for (; it != range.second; ++it) {
                const auto& n = it->second;
                if (s.bytes[0x66] ? (!n.texture && n.flat_index && *n.flat_index == s.bytes[0x67])
                                  : (n.texture && !n.texture->mips.empty() && s.size &&
                                     n.texture->mips[0].pixels.size() >= s.size &&
                                     rr::testlib::fnv(n.texture->mips[0].pixels.data(), s.size) == s.hash))
                    break;
            }
            if (it == range.second) {
                ++frame_missing;
                if (std::getenv("RR_TERRAIN_VERBOSE")) {
                    std::cout << "  missing " << name << " hash " << s.hash << " size " << s.size << " at";
                    for (auto value : v) std::cout << ' ' << value;
                    for (auto j = range.first; j != range.second; ++j)
                        std::cout << " | native " << (j->second.texture ? j->second.texture->name : "flat");
                    std::cout << '\n';
                }
                continue;
            }
            native.erase(it);
            ++matched;
        }
        // Riders' ground: 0x0043a190 -> 0x00439b10(+0x140 sub-slice, +0x28, +0x138).
        for (const auto& r : dump.riders) {
            if (r.bike.size() < 0x340) continue;
            const uint32_t sub = uint32_t(r.bike_i32(0x140));
            rr::TerrainSlice t, tn;
            size_t sample = 0, next_sample = 0;
            if (!load(sub, t, sample)) continue;
            if (!dump.memory.has(sub + 8) || !load(dump.memory.u32(sub + 8), tn, next_sample)) {
                tn = t;
                next_sample = sample;
            }
            const auto& rs = route[sample];
            const auto& rn = route[next_sample];
            const int32_t got =
                rr::terrain_height(t, rs.path, tn, rn.path, r.bike_i32(0x28) - rs.offset * 256,
                                   uint8_t(r.bike_i32(0x138)), rn.offset - rs.offset, rs.offset);
            ++grounds;
            if (got != r.bike_i32(0x24)) ++ground_failures;
        }
        ++frames;
        missing += frame_missing;
        if (frame_missing)
            std::cout << "frame " << f.at("scene.file") << ": " << frame_missing
                      << " original terrain nodes without a native match\n";
    }
    if (!frames) {
        std::cout << "skip: capture has no scene frames\n";
        return rr::golden::skip;
    }
    std::cout << frames << " frames: " << matched << " terrain nodes match, " << missing << " missing, "
              << others << " scenery/actor nodes not compared; " << grounds << " rider grounds, "
              << ground_failures << " differ\n";
    return missing || (check_ground && ground_failures) ? 1 : 0;
}
} // namespace

int run(int argc, char** argv) {
    bool actors_mode = false, terrain_mode = false, check_ground = true;
    const std::string dir = rr::golden::oracle_dir();
    std::string tsv = dir + "/city-scene.tsv";
    std::string assets_root = RR_SOURCE_DIR "/../cdrom/ROADRASH";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--native-actors")
            actors_mode = true;
        else if (a == "--native-terrain")
            terrain_mode = true;
        else if (a == "--nodes-only")
            check_ground = false; // report rider grounds without gating
        else if (a == "--assets" && i + 1 < argc)
            assets_root = argv[++i];
        else if (a[0] != '-')
            tsv = a;
    }
    rr::Assets assets(assets_root);
    std::ifstream probe(tsv);
    if (!probe || !assets.exists("DATA/GLOBAL.RSC")) {
        std::cout << "skip: no scene capture or disc\n";
        return rr::golden::skip;
    }
    if (actors_mode) return native_actors(tsv, assets);
    if (terrain_mode) return native_terrain(tsv, assets, check_ground);
    const auto golden = rr::golden::load(tsv);
    const std::string stem = tsv.substr(0, tsv.size() - 4);
    const int course = int(golden.frames.at(0).at("course"));
    const auto lib = rr::testlib::load_texture_library(assets, course_files[course]);
    const auto exe = assets.read("ROADRASH.EXE");
    const auto sprites = rr::load_rider_sprites(assets, exe, std::getenv("RR_SCENE_LOBIKES") == nullptr);
    std::cout << lib.textures.size() << " textures\n";
    int failures = 0, frames = 0;
    for (const auto& f : golden.frames) {
        if (!f.count("scene.file") || !f.count("pixels.file")) continue;
        const auto dump = rr::scene::load(stem + "-scene" + std::to_string(f.at("scene.file")) + ".raw");
        const auto pixels = read_file(stem + "-f" + std::to_string(f.at("pixels.file")) + ".raw");
        const int width = int(f.at("pixels.width")), height = int(f.at("pixels.height"));
        if (pixels.size() != size_t(width) * size_t(height)) {
            ++failures;
            continue;
        }
        rr::Framebuffer fb(width, height);
        for (int y = 0; y < height; ++y)
            std::memcpy(fb.row(y), pixels.data() + size_t(y) * size_t(width), size_t(width));
        std::vector<rr::DrawNode> nodes;
        std::vector<std::unique_ptr<rr::CelTexture>> dynamic;
        int unmatched = 0;
        for (const auto& s : dump.nodes) {
            rr::DrawNode n;
            for (int k = 0; k < 4; ++k)
                n.view_points[size_t(k)] = {int32_t(s.u32(0x44 + 8 * size_t(k))),
                                            int32_t(s.u32(0x48 + 8 * size_t(k)))};
            n.flip_u = s.bytes[0x64] != 0;
            n.flip_v = s.bytes[0x65] != 0;
            if (s.u32(8) == 0x00466684) { // dynamic "Bob" resource (0x00466688 holds its address)
                auto t = rr::rider_texture(sprites, int32_t(s.u32(0x38)), int32_t(s.u32(0x34)));
                if (!t) continue;
                dynamic.push_back(std::make_unique<rr::CelTexture>(std::move(*t)));
                n.texture = dynamic.back().get();
            } else if (s.bytes[0x66])
                n.flat_index = s.bytes[0x67];
            else if (!(n.texture = lib.find(s.hash, s.size))) {
                ++unmatched;
                continue;
            }
            nodes.push_back(n);
        }
        try {
            for (size_t i = 0; i < nodes.size(); ++i) {
                try {
                    rr::render_nodes(fb, {nodes[i]}, rr::view_mode(f.at("view.high") != 0),
                                     uint32_t(f.count("view.quality_mask") ? f.at("view.quality_mask") : 16));
                } catch (const std::exception& e) {
                    const auto* t = nodes[i].texture;
                    if (t)
                        std::cerr << "texture " << t->name << " bounds " << t->bounds[0] << ','
                                  << t->bounds[1] << ',' << t->bounds[2] << ',' << t->bounds[3] << " mip0 "
                                  << t->mips[0].width << 'x' << t->mips[0].height << '\n';
                    throw std::runtime_error("node " + std::to_string(i) + ": " + e.what());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "frame " << f.at("scene.file") << ": " << e.what() << "\n";
            ++failures;
            continue;
        }
        size_t differ = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                differ += fb.row(y)[x] != pixels[size_t(y) * size_t(width) + size_t(x)];
        std::cout << "frame " << f.at("scene.file") << ": " << dump.nodes.size() << " nodes, " << nodes.size()
                  << " rendered, " << unmatched << " unmatched, " << differ << " differing pixels\n";
        if (differ || unmatched) {
            ++failures;
            std::ofstream out(stem + "-scene" + std::to_string(f.at("scene.file")) + "-native.raw",
                              std::ios::binary);
            for (int y = 0; y < height; ++y) out.write(reinterpret_cast<const char*>(fb.row(y)), width);
        }
        ++frames;
    }
    if (!frames) {
        std::cout << "skip: capture has no scene frames\n";
        return rr::golden::skip;
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "oracle scene: " << e.what() << '\n';
        return 1;
    }
}
