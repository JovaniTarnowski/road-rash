#include "actor_render.hpp"
#include "assets.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>
int main(int argc, char** argv) {
    try {
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if (!assets.exists("ROADRASH.EXE")) {
            std::cout << "Imported actor projection corpus absent\n";
            return 0;
        }
        const auto depths = rr::read_depth_table(assets.read("ROADRASH.EXE"));
        std::ifstream file(argc > 1 ? argv[1]
                                    : RR_SOURCE_DIR "/analysis/oracle/napa-actor-animation-fixtures.tsv");
        if (!file) {
            std::cout << "Original actor projection fixture absent\n";
            return 0;
        }
        std::map<std::string, int64_t> values;
        std::string line, key;
        int64_t value;
        while (std::getline(file, line)) {
            std::istringstream row(line);
            if (row >> key >> value) values[key] = value;
        }
        rr::CelTexture texture;
        texture.bounds = {0, 0, 24, 64};
        rr::AnimationFrame frame;
        frame.width = 24;
        frame.height = 64;
        size_t nodes = 0;
        for (int c = 0; c < values.at("actor.projection.count"); ++c) {
            const auto prefix = "actor.projection." + std::to_string(c) + ".";
            auto get = [&](const char* k) { return int32_t(values.at(prefix + k)); };
            frame.hotspot = {get("hotspot_x"), get("hotspot_y")};
            rr::ActorProjection input;
            input.road_x = get("road_x");
            input.far_x = get("far_x");
            input.road_y = get("road_y");
            input.far_y = get("far_y");
            input.depth = get("depth");
            input.occlusion = get("occlusion");
            input.flags = uint32_t(get("flags"));
            input.size = get("size");
            input.lateral = get("lateral");
            input.ground = get("ground");
            input.fraction = uint8_t(get("fraction"));
            input.clip_ground = get("clip") != 0;
            rr::ActorOrientation orientation;
            orientation.depth = input.depth;
            orientation.position = input.fraction;
            orientation.road_x = input.road_x;
            orientation.lateral_q8 = get("orientation_lateral");
            orientation.heading = get("orientation_heading");
            orientation.roll = get("orientation_roll");
            orientation.road_heading = get("orientation_road_heading");
            orientation.rider = get("orientation_rider") != 0;
            const auto angles = rr::actor_angles(orientation, get("camera_x"));
            if (angles.yaw != get("orientation_yaw") || angles.roll != get("orientation_roll") ||
                angles.view_angle != get("orientation_view"))
                throw std::runtime_error(prefix + "orientation mismatch");
            const auto node = rr::actor_node(frame, &texture, input, {get("camera_x"), get("camera_height")},
                                             depths, get("width"));
            if (bool(node) != bool(get("count"))) throw std::runtime_error(prefix + "visibility mismatch");
            if (node) {
                for (int p = 0; p < 4; ++p)
                    for (int axis = 0; axis < 2; ++axis) {
                        const auto key = prefix + "0.point." + std::to_string(p * 2 + axis);
                        const int32_t actual = axis ? node->view_points[p].y : node->view_points[p].x;
                        if (actual != values.at(key))
                            throw std::runtime_error(key + " native=" + std::to_string(actual) +
                                                     " original=" + std::to_string(values.at(key)));
                    }
                ++nodes;
            }
        }
        std::cout << values.at("actor.projection.count") << " original actor projection cases, " << nodes
                  << " nodes match\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
