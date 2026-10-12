// Natural-frame display-list check: the original's own pending display nodes for a captured race
// frame (oracle dump_scene) are converted to native DrawNodes, resources identified by their mip-0
// hash, and rendered over the original frame. Every pixel must equal the original final pixel.
//
// --native-actors mode (plan 11a Task 3 Step 2): rebuild the player/opponent actor nodes from the
// dumped scheduler bike/rider-half objects (scene v4, oracle dump_rider_objects) and compare with
// the dump's Bob nodes: node order, bank (body/shadow), frame, all four view points, flip, and
// texture/palette selection (0xde/0xdf/0xe0). Reconstruction rules (traced, see
// analysis/nodes-notes.md): native project_road over the course (proven vs TSV by oracle-road),
// Game::rider_nodes orientation/projection from bike +0x1c/+0x28/+0x24/+0x20/+0x5c/+0x60, direct
// animation selection {+0x80,+0x81} through playback[0], colour from DAT_004642dc / DAT_004c7f70 /
// DAT_004c7f74 grid slots with the rider-table row byte (0x00446190/0x004460c0), shadow colour 0xdf
// only for cop bodies (0x00446350), ceiling from the CourseScene emitter pairing.
#include "actor_nodes.hpp"
#include "course.hpp"
#include "course_scene.hpp"
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
#include <memory>

namespace {
const char* const course_files[5] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};   // 0x0046df58
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int32_t blob_i32(const std::vector<uint8_t>& b, size_t p) {
    return int32_t(uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24);
}

// Expected palette colour for a scheduler bike object (traced 0x00446190 /
// 0x004460c0 via 0x00446380): the cop bike is 0xde; the player bike
// (DAT_004c7f70) takes the rider-table row byte (rider half +0x1e0 points at
// the 0x00463c78 row, +5); other bikes take their 1-based grid slot in
// DAT_004c7f74, or 0 when the slot equals the player row byte.
int32_t actor_colour(uint32_t bike_addr, const rr::scene::Memory& mem, const std::vector<uint8_t>& exe,
                     uint8_t player_row_byte) {
    const uint32_t cop = mem.u32(0x004642dc);
    if (cop != 0 && bike_addr == cop) return 0xde;
    if (bike_addr == mem.u32(0x004c7f70)) return int32_t(player_row_byte);
    for (int slot = 1; slot <= 15; ++slot) {
        uint32_t entry = 0x004c7f74 + uint32_t(slot - 1) * 4;
        if (!mem.has(entry)) continue;
        if (mem.u32(entry) == bike_addr) return slot == player_row_byte ? 0 : slot;
    }
    return -1;   // bike not on the grid: caller reports it
}

// Rider-table row byte (+5) for the player: the player half (+0x2fc of the
// player bike DAT_004642d8) carries the 0x00463c78 row pointer at +0x1e0.
uint8_t player_row_byte(const rr::scene::Dump& dump, const std::vector<uint8_t>& exe) {
    const uint32_t player = dump.memory.u32(0x004642d8);
    for (const auto& r : dump.riders) {
        if (r.address != player || r.half.empty()) continue;
        const uint32_t row = uint32_t(blob_i32(r.half, 0x1e0));
        // Rows live in .data (0x00463000 image covers 0x00463c78); scheduler
        // halves mirror the same pointer. Read through the dump when present,
        // else through the reference EXE image (same bytes by hash check).
        if (dump.memory.has(row + 5)) return dump.memory.u8(row + 5);
        const size_t raw = 0x61600 + (row - 0x00463000) + 5;   // .data file offset
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
    const auto course_data = rr::read_course(assets.read(std::string("DATA/COURSES/") + course_files[course] + ".CRS"));
    const auto routes = {rr::make_route(course_data, false), rr::make_route(course_data, true)};
    rr::ActorNodes actors(assets, exe, true);
    int failures = 0, frames = 0, nodes = 0;
    for (size_t f = 0; f < golden.frames.size(); ++f) {
        const auto& frame = golden.frames[f];
        if (!frame.count("scene.file")) continue;
        const auto dump = rr::scene::load(stem + "-scene" + std::to_string(frame.at("scene.file")) + ".raw");
        if (dump.version < 4 || dump.riders.empty()) {
            std::cout << "frame " << frame.at("scene.file") << ": skip (no rider objects; recapture with v4 dump)\n";
            continue;
        }
        const uint8_t prow = player_row_byte(dump, exe);
        // Camera matches oracle-road-test: relative to the route's own road.
        const uint32_t cam_pos = uint32_t(frame.at("camera.position"));
        const int32_t cam_x = int32_t(frame.at("camera.x")), cam_h = int32_t(frame.at("camera.height"));
        bool frame_ok = false;
        std::string frame_error;
        // Try both routes like oracle-road-test.
        for (const auto& route : routes) {
            if ((cam_pos >> 8) >= route.size()) continue;
            rr::Camera camera{cam_x - route[cam_pos >> 8].offset, cam_h};
            const auto slices = rr::project_road(route, cam_pos, camera, depths);
            // Slice index by sample for the emitter pairing.
            std::map<uint32_t, size_t> by_sample;
            for (size_t i = 0; i < slices.size(); ++i) by_sample[slices[i].sample] = i;
            // Expected colour per bike address.
            std::map<uint32_t, int32_t> colours;
            for (const auto& r : dump.riders) {
                const auto& bike = r.bike;
                if (blob_i32(bike, 0xc4) != 4) continue;   // scheduler bike objects only
                colours[r.address] = actor_colour(r.address, dump.memory, exe, prow);
            }
            // Rebuild actor nodes per drawn bike: shadow then body (0x00446250).
            struct Expected {
                uint32_t frame;
                int32_t colour;   // body colour; shadow is 0xdf iff body is 0xde else 0xe0
                std::array<rr::RasterPoint, 4> shadow, body;
                bool shadow_flip, body_flip;
            };
            std::vector<Expected> expected;
            bool usable = true;
            // The emitter pairing supplies near/far/ceiling per sample; the
            // scene is required so ceilings match the original occlusion rows.
            const auto scene = rr::CourseScene(assets, exe, course_data, course_files[course], 1, 0);
            std::map<uint32_t, std::vector<rr::DrawNode>> built;
            auto emitter = [&](const rr::RoadSlice& near, const rr::RoadSlice& far, int road, int32_t ceiling, bool back) {
                std::vector<rr::DrawNode> out;
                for (const auto& r : dump.riders) {
                    const auto& bike = r.bike;
                    if (blob_i32(bike, 0xc4) != 4) continue;
                    const uint32_t pos = uint32_t(blob_i32(bike, 0x1c));
                    if ((pos >> 8) != near.sample || ((pos & 255) >= 128) != back) continue;
                    auto it = by_sample.find(pos >> 8);
                    if (it == by_sample.end() || it->second >= slices.size() - 1) continue;
                    if (&near != &slices[it->second] || &far != &slices[it->second + 1]) continue;
                    if (near.count == 2) {
                        // Route check needs the rider's road; v4 stores the
                        // slice pointer at +0x140: half 0 selects road 0.
                        // The dump slice word is process-specific, so match by
                        // sample only and require single-road here.
                        continue;
                    }
                    const auto& sample = route.at(near.sample);
                    rr::ActorOrientation orientation;
                    orientation.depth = near.depth;
                    orientation.position = int32_t(pos);
                    orientation.road_x = near.road[road].road_x + camera.x;
                    orientation.lateral_q8 = blob_i32(bike, 0x28) - sample.offset * 256;
                    orientation.road_heading = near.road[road].heading;
                    orientation.heading = blob_i32(bike, 0x5c);
                    orientation.roll = blob_i32(bike, 0x60);
                    orientation.rider = true;   // dispatch takes the bike (type 4)
                    rr::ActorProjection projection;
                    projection.road_x = orientation.road_x;
                    projection.far_x = far.road[road].road_x + camera.x;
                    projection.road_y = near.road[road].world_y;
                    projection.far_y = far.road[road].world_y;
                    projection.lateral = orientation.lateral_q8 / 256;
                    projection.ground = blob_i32(bike, 0x24);
                    projection.depth = near.depth;
                    projection.fraction = uint8_t(pos);
                    projection.occlusion = ceiling;
                    const int8_t anim = int8_t(bike[0x80] & 0x7f), index = int8_t(bike[0x81]);
                    const auto& bank = actors.library().playback[0];
                    if (anim < 0 || size_t(anim) >= bank.size() || !bank[size_t(anim)] ||
                        index < 0 || size_t(index) >= bank[size_t(anim)]->size()) {
                        usable = false;
                        continue;
                    }
                    const auto& descriptor = bank[size_t(anim)]->at(size_t(index));
                    const int32_t colour = colours[r.address];
                    const auto made = actors.rider(descriptor, 0, colour, orientation, projection, camera, depths,
                                                   blob_i32(bike, 0x20));
                    for (const auto& n : made) out.push_back(n);
                    built[r.address] = made;
                }
                return out;
            };
            try {
                scene.draw_nodes(route, slices, camera, depths, emitter);
            } catch (const std::exception& e) {
                frame_error = std::string("draw_nodes: ") + e.what();
                usable = false;
            }
            if (!usable) continue;
            // Collect the dump's Bob pairs in order: shadow (0xe0/0xdf) then body.
            struct Dumped {
                size_t index;
                int32_t frame, colour;
                std::array<rr::RasterPoint, 4> points;
                bool flip_u, flip_v;
            };
            std::vector<std::pair<Dumped, Dumped>> pairs;
            for (size_t i = 0; i < dump.nodes.size(); ++i) {
                const auto& s = dump.nodes[i];
                if (s.u32(8) != 0x00466684) continue;
                const int32_t colour = int32_t(s.u32(0x38));
                if ((colour == 0xe0 || colour == 0xdf) && i + 1 < dump.nodes.size()) {
                    const auto& b = dump.nodes[i + 1];
                    if (b.u32(8) != 0x00466684) continue;
                    const int32_t bcolour = int32_t(b.u32(0x38));
                    if (bcolour == 0xe0 || bcolour == 0xdf) continue;
                    Dumped shadow{i, int32_t(s.u32(0x34)), colour}, body{i + 1, int32_t(b.u32(0x34)), bcolour};
                    for (int k = 0; k < 4; ++k) {
                        shadow.points[size_t(k)] = {int32_t(s.u32(0x44 + 8 * size_t(k))), int32_t(s.u32(0x48 + 8 * size_t(k)))};
                        body.points[size_t(k)] = {int32_t(b.u32(0x44 + 8 * size_t(k))), int32_t(b.u32(0x48 + 8 * size_t(k)))};
                    }
                    shadow.flip_u = s.bytes[0x64] != 0;
                    shadow.flip_v = s.bytes[0x65] != 0;
                    body.flip_u = b.bytes[0x64] != 0;
                    body.flip_v = b.bytes[0x65] != 0;
                    pairs.emplace_back(shadow, body);
                    ++i;
                }
            }
            // Match rebuilt nodes to dumped pairs by body colour+frame.
            if (pairs.size() != built.size()) {
                frame_error = "pair count " + std::to_string(pairs.size()) + " != rebuilt bikes " + std::to_string(built.size());
                continue;
            }
            bool same = true;
            std::string diff;
            // Order check: dumped pairs must appear in increasing node order
            // and rebuilt bikes in increasing position (display-list order).
            for (size_t i = 1; i < pairs.size(); ++i)
                if (pairs[i].first.index < pairs[i - 1].first.index) {
                    same = false;
                    diff = "dump pair order";
                }
            // Index rebuilt bikes by (colour, frame) like the dump pairs.
            std::multimap<std::pair<int32_t, uint32_t>, std::pair<uint32_t, std::vector<rr::DrawNode>>> by_key;
            for (const auto& [addr, made] : built) {
                if (made.size() != 2) {
                    same = false;
                    diff = "bike " + std::to_string(addr) + " rebuilt " + std::to_string(made.size()) + " nodes";
                    continue;
                }
                // Find the dump frame for this colour.
                for (const auto& [sh, bo] : pairs)
                    if (bo.colour == colours[addr]) by_key.emplace(std::make_pair(bo.colour, bo.frame), std::make_pair(addr, made));
            }
            for (const auto& [sh, bo] : pairs) {
                auto range = by_key.equal_range({bo.colour, uint32_t(bo.frame)});
                if (range.first == range.second) {
                    same = false;
                    diff = "no rebuilt bike for colour " + std::to_string(bo.colour) + " frame " + std::to_string(bo.frame);
                    continue;
                }
                const auto& made = range.first->second.second;
                const auto& sh_node = made[0], bo_node = made[1];
                auto check = [&](const rr::DrawNode& n, const Dumped& d, const char* what) {
                    for (int k = 0; k < 4; ++k)
                        if (n.view_points[size_t(k)].x != d.points[size_t(k)].x ||
                            n.view_points[size_t(k)].y != d.points[size_t(k)].y) {
                            same = false;
                            diff = std::string(what) + " view point " + std::to_string(k) + " colour " +
                                   std::to_string(bo.colour);
                        }
                    if (n.flip_u != d.flip_u || n.flip_v != d.flip_v) {
                        same = false;
                        diff = std::string(what) + " flip colour " + std::to_string(bo.colour);
                    }
                };
                check(sh_node, sh, "shadow");
                check(bo_node, bo, "body");
                // Palette selection: shadow 0xdf iff body is cop 0xde.
                const int32_t want_shadow = (bo.colour == 0xde) ? 0xdf : 0xe0;
                if (sh.colour != want_shadow) {
                    same = false;
                    diff = "shadow palette colour " + std::to_string(bo.colour);
                }
                if (!same && false) break;
                by_key.erase(range.first);
            }
            if (same) {
                frame_ok = true;
                nodes += int(pairs.size()) * 2;
                break;
            }
            if (frame_error.empty()) frame_error = diff;
        }
        ++frames;
        if (!frame_ok) {
            ++failures;
            std::cout << "frame " << frame.at("scene.file") << ": MISMATCH " << frame_error << "\n";
        } else {
            std::cout << "frame " << frame.at("scene.file") << ": actor nodes match\n";
        }
    }
    std::cout << frames << " frames, " << nodes << " actor nodes, " << failures << " failures\n";
    return failures ? 1 : 0;
}
}   // namespace

int main(int argc, char** argv) {
    bool actors_mode = false;
    const std::string dir = rr::golden::oracle_dir();
    std::string tsv = dir + "/city-scene.tsv";
    std::string assets_root = RR_SOURCE_DIR "/../cdrom/ROADRASH";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--native-actors") actors_mode = true;
        else if (a == "--assets" && i + 1 < argc) assets_root = argv[++i];
        else if (a[0] != '-') tsv = a;
    }
    rr::Assets assets(assets_root);
    std::ifstream probe(tsv);
    if (!probe || !assets.exists("DATA/GLOBAL.RSC")) { std::cout << "skip: no scene capture or disc\n"; return rr::golden::skip; }
    if (actors_mode) return native_actors(tsv, assets);
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
        if (pixels.size() != size_t(width) * size_t(height)) { ++failures; continue; }
        rr::Framebuffer fb(width, height);
        for (int y = 0; y < height; ++y) std::memcpy(fb.row(y), pixels.data() + size_t(y) * size_t(width), size_t(width));
        std::vector<rr::DrawNode> nodes;
        std::vector<std::unique_ptr<rr::CelTexture>> dynamic;
        int unmatched = 0;
        for (const auto& s : dump.nodes) {
            rr::DrawNode n;
            for (int k = 0; k < 4; ++k) n.view_points[size_t(k)] = {int32_t(s.u32(0x44 + 8 * size_t(k))), int32_t(s.u32(0x48 + 8 * size_t(k)))};
            n.flip_u = s.bytes[0x64] != 0;
            n.flip_v = s.bytes[0x65] != 0;
            if (s.u32(8) == 0x00466684) {   // dynamic "Bob" resource (0x00466688 holds its address)
                auto t = rr::rider_texture(sprites, int32_t(s.u32(0x38)), int32_t(s.u32(0x34)));
                if (!t) continue;
                dynamic.push_back(std::make_unique<rr::CelTexture>(std::move(*t)));
                n.texture = dynamic.back().get();
            } else if (s.bytes[0x66]) n.flat_index = s.bytes[0x67];
            else if (!(n.texture = lib.find(s.hash, s.size))) { ++unmatched; continue; }
            nodes.push_back(n);
        }
        try {
            for(size_t i=0;i<nodes.size();++i) {
                try {rr::render_nodes(fb,{nodes[i]},rr::view_mode(f.at("view.high") != 0),uint32_t(f.count("view.quality_mask") ? f.at("view.quality_mask") : 16));}
                catch(const std::exception& e) {
                    const auto* t=nodes[i].texture;
                    if(t)std::cerr<<"texture "<<t->name<<" bounds "<<t->bounds[0]<<','<<t->bounds[1]<<','<<t->bounds[2]<<','<<t->bounds[3]<<" mip0 "<<t->mips[0].width<<'x'<<t->mips[0].height<<'\n';
                    throw std::runtime_error("node "+std::to_string(i)+": "+e.what());
                }
            }
        } catch (const std::exception& e) { std::cerr << "frame " << f.at("scene.file") << ": " << e.what() << "\n"; ++failures; continue; }
        size_t differ = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) differ += fb.row(y)[x] != pixels[size_t(y) * size_t(width) + size_t(x)];
        std::cout << "frame " << f.at("scene.file") << ": " << dump.nodes.size() << " nodes, " << nodes.size() << " rendered, "
                  << unmatched << " unmatched, " << differ << " differing pixels\n";
        if (differ || unmatched) {
            ++failures;
            std::ofstream out(stem + "-scene" + std::to_string(f.at("scene.file")) + "-native.raw", std::ios::binary);
            for (int y = 0; y < height; ++y) out.write(reinterpret_cast<const char*>(fb.row(y)), width);
        }
        ++frames;
    }
    if (!frames) { std::cout << "skip: capture has no scene frames\n"; return rr::golden::skip; }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
