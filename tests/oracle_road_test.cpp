#include "golden.hpp"
#include "assets.hpp"
#include "road.hpp"
#include <array>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    const std::string file = argc > 1 ? argv[1] : rr::golden::oracle_dir() + "/napa-attract.tsv";
    if (!std::filesystem::exists(file)) {
        std::cerr << "SKIP missing oracle capture " << file << '\n';
        return rr::golden::skip;
    }
    try {
        const auto golden = rr::golden::load(file);
        if (golden.exe != "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2")
            throw std::runtime_error("Oracle capture is from a different executable");
        rr::Assets assets(argc > 2 ? argv[2] : std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH");
        const auto table = rr::read_depth_table(assets.read("ROADRASH.EXE"));
        // Original course selector indexes the pointer table at 0x0046df58
        // (0x00441940), unlike rr-game's CLI course ordering.
        const char* names[] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};
        std::map<int, std::array<std::vector<rr::RouteSample>, 2>> routes;
        size_t compared = 0, mismatches = 0;
        for (size_t f = 0; f < golden.frames.size(); ++f) {
            const auto& original = golden.frames[f];
            if (original.at("slice.0.kind") != 1 && original.at("slice.0.kind") != 2) continue;
            const int course = int(original.at("course"));
            if (course < 0 || course >= 5) throw std::runtime_error("Unknown oracle course");
            if (!routes.count(course)) {
                const auto data =
                    rr::read_course(assets.read(std::string("DATA/COURSES/") + names[course] + ".CRS"));
                routes[course] = {rr::make_route(data, false), rr::make_route(data, true)};
            }
            const int64_t recorded_position = original.at("camera.position");
            if (recorded_position < 0 || recorded_position > UINT32_MAX)
                throw std::runtime_error("Invalid oracle camera position");
            const uint32_t position = uint32_t(recorded_position);
            bool matched = false;
            std::string first_difference;
            for (const auto& route : routes.at(course)) {
                if ((position >> 8) >= route.size()) continue;
                rr::Camera camera{int32_t(original.at("camera.x")) - route[position >> 8].offset,
                                  int32_t(original.at("camera.height"))};
                const auto native = rr::project_road(route, position, camera, table);
                bool same = true;
                auto compare = [&](const std::string& key, int64_t value) {
                    const auto it = original.find(key);
                    if (it == original.end())
                        throw std::runtime_error("Oracle is missing required field " + key);
                    if (it->second == value) return;
                    if (first_difference.empty())
                        first_difference = key + " original=" + std::to_string(it->second) +
                                           " native=" + std::to_string(value);
                    same = false;
                };
                for (size_t s = 0; s < native.size(); ++s) {
                    const auto p = "slice." + std::to_string(s) + ".";
                    compare(p + "kind", native[s].count);
                    compare(p + "sample", native[s].sample);
                    for (int r = 0; r < native[s].count; ++r) {
                        const auto q = p + "road." + std::to_string(r) + ".";
                        const auto& road = native[s].road[r];
                        compare(q + "depth", native[s].depth);
                        compare(q + "scale", native[s].scale);
                        compare(q + "centre", native[s].screen_x(r, 0));
                        compare(q + "screen_y", road.screen_y);
                        compare(q + "left", native[s].screen_x(r, road.left));
                        compare(q + "right", native[s].screen_x(r, road.right));
                        compare(q + "shoulder_left", native[s].screen_x(r, road.shoulder_left));
                        compare(q + "shoulder_right", native[s].screen_x(r, road.shoulder_right));
                    }
                }
                if (same) {
                    matched = true;
                    break;
                }
            }
            ++compared;
            if (!matched) {
                if (mismatches < 20) std::cerr << "frame " << f << " " << first_difference << '\n';
                ++mismatches;
            }
        }
        std::cout << compared << " frames compared; " << mismatches << " mismatches\n";
        if (!compared) throw std::runtime_error("Oracle capture has no race frames");
        return mismatches ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
