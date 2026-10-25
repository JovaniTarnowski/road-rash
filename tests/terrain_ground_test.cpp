#include "terrain_ground.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
int main(int argc, char** argv) {
    try {
        rr::TerrainSlice t;
        t.type = 0;
        t.selectors = {0, 255};
        rr::PathSample p;
        p.left = p.right = 250;
        p.shoulder_left = p.shoulder_right = 100;
        p.height_left = 16;
        p.height_right = 8;
        if (rr::terrain_height(t, p, t, p, -400 * 256, 0) != 999 ||
            rr::terrain_height(t, p, t, p, 400 * 256, 0) != 8)
            throw std::runtime_error("Building ground selector boundaries");
        p.marking_flags = 32;
        if (rr::terrain_height(t, p, t, p, -400 * 256, 0) != 0) throw std::runtime_error("Marked ground");
        {
            // 0x00439b10 case 1 on 0x00454240 values: outer edges -500/500.
            rr::TerrainSlice m;
            m.type = 1;
            rr::PathSample q;
            q.left = q.right = 250;
            q.shoulder_left = q.shoulder_right = 250;
            q.elevation = 1000;
            m.walls[0] = {200, 2500, 600};
            m.walls[1] = {100, 400, 0};
            auto ground = [&](int32_t x) { return rr::terrain_height(m, q, m, q, x * 256, 0); };
            if (ground(700) != -1000) throw std::runtime_error("Cliff side drops to height 0 (-elevation)");
            if (ground(550) != 0 || ground(-600) != 0)
                throw std::runtime_error("Flat ground before the wall base");
            if (ground(-800) != (-350 * 600) / 2300) throw std::runtime_error("Left wall ramp formula");
            m.walls[1] = {100, 2000, 300};
            if (ground(700) != 300 * 100 / 1900) throw std::runtime_error("Right wall ramp");
            if (rr::terrain_height(m, q, m, q, 700 * 256, 0, 0, -2600) != 300)
                throw std::runtime_error("Right ramp limit uses the absolute top");
            m.walls[1] = {100, 400, 300};
            if (ground(700) != 300) throw std::runtime_error("Steep wall returns its height");
            // Case 3 tunnel: empty pair-B heights become 16.
            rr::TerrainSlice tunnel;
            tunnel.type = 3;
            if (rr::terrain_height(tunnel, q, tunnel, q, -300 * 256, 0) != 16 ||
                rr::terrain_height(tunnel, q, tunnel, q, 300 * 256, 0) != 16)
                throw std::runtime_error("Tunnel sidewalk 16");
            q.height_left = 10;
            q.height_right = 20;
            if (rr::terrain_height(tunnel, q, tunnel, q, -300 * 256, 0) != 10 ||
                rr::terrain_height(tunnel, q, tunnel, q, 300 * 256, 0) != 20)
                throw std::runtime_error("Tunnel sidewalk pair-B heights");
        }
        std::ifstream file(argc > 1 ? argv[1] : RR_SOURCE_DIR "/analysis/oracle/napa-building-fixtures.tsv");
        if (file) {
            std::map<std::string, int32_t> values;
            std::string line, key;
            int64_t v;
            while (std::getline(file, line)) {
                std::istringstream row(line);
                if (row >> key >> v) values[key] = int32_t(v);
            }
            size_t samples = 0;
            for (int c = 0; c < values.at("ground.count"); ++c) {
                const auto prefix = "ground." + std::to_string(c) + ".";
                auto get = [&](const std::string& k) { return values.at(prefix + k); };
                rr::TerrainSlice a, b;
                rr::PathSample p, q;
                a.type = uint8_t(get("type"));
                p.marking_flags = uint8_t(get("flags"));
                a.selectors = {uint8_t(get("selector_left")), uint8_t(get("selector_right"))};
                auto load = [&](rr::TerrainSlice& t, rr::PathSample& path, const std::string& name) {
                    path.left = -get(name + ".0");
                    path.right = get(name + ".1");
                    path.shoulder_left = -get(name + ".2") - path.left;
                    path.shoulder_right = get(name + ".3") - path.right;
                    path.height_left = get(name + ".4");
                    path.height_right = get(name + ".5");
                    for (int i = 0; i < 6; ++i)
                        t.points[i] = {get(name + "." + std::to_string(6 + i * 2)),
                                       get(name + "." + std::to_string(7 + i * 2))};
                };
                load(a, p, "near");
                load(b, q, "far");
                for (int i = 0; i < 64; ++i) {
                    const auto root = std::to_string(i) + ".";
                    const int32_t got =
                        rr::terrain_height(a, p, b, q, get(root + "x"), uint8_t(get(root + "fraction")));
                    if (got != get(root + "height"))
                        throw std::runtime_error(prefix + root + "height native=" + std::to_string(got) +
                                                 " original=" + std::to_string(get(root + "height")));
                    ++samples;
                }
            }
            std::cout << samples << " original terrain ground samples match\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
