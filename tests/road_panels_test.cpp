#include "road_panels.hpp"
#include "road_materials.hpp"
#include "assets.hpp"
#include "golden.hpp"
#include <filesystem>
#include <iostream>
int main() {
    rr::RoadPanelInput in;
    in.first_left = in.second_left = 10 * 65536;
    in.first_y = 100 * 65536;
    in.second_y = 80 * 65536;
    in.first_lane_width = in.second_lane_width = 20 * 65536;
    in.left_lane_count = in.right_lane_count = 1;
    in.left_lanes_q8 = in.right_lanes_q8 = 256;
    const auto panels = rr::road_panels(in);
    if (panels.size() != 2 || panels[0].points[0].x != 6 * 65536 || panels[1].points[1].x != 54 * 65536 ||
        !panels[0].flip_v)
        return 1;
    in.visible = false;
    if (!rr::road_panels(in).empty()) return 1;
    const auto path = rr::golden::oracle_dir() + "/raster-fixtures.tsv";
    if (!std::filesystem::exists(path)) {
        std::cout << "Synthetic panels pass; SKIP missing original panels oracle\n";
        return 0;
    }
    try {
        const auto golden = rr::golden::load(path);
        if (golden.exe != "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" ||
            golden.frames.empty())
            throw std::runtime_error("Wrong panel golden");
        const auto& fields = golden.frames.front();
        size_t checked = 0, pixel_count = 0;
        std::optional<rr::RoadMaterials> materials;
        const auto root = std::filesystem::path(RR_SOURCE_DIR) / "../cdrom/ROADRASH";
        if (std::filesystem::exists(root)) {
            rr::Assets assets(root.string());
            materials = rr::read_road_materials(assets.read("ROADRASH.EXE"), assets.read("DATA/GLOBAL.RSC"));
        }
        for (int c = 0; c < 128; ++c) {
            const auto prefix = "panels." + std::to_string(c) + ".";
            auto value = [&](const char* key) { return int32_t(fields.at(prefix + key)); };
            rr::RoadPanelInput input;
            input.first_left = value("first_left");
            input.second_left = value("second_left");
            input.first_y = value("first_y");
            input.second_y = value("second_y");
            input.first_lane_width = value("first_step");
            input.second_lane_width = value("second_step");
            input.left_lanes_q8 = value("left_q8");
            input.right_lanes_q8 = value("right_q8");
            input.left_lane_count = uint8_t(value("left_count"));
            input.right_lane_count = uint8_t(value("right_count"));
            input.surface_flags = uint8_t(value("surface"));
            input.marking_flags = uint8_t(value("marking"));
            input.terrain_type = uint8_t(value("type"));
            input.next_terrain_type = uint8_t(value("next_type"));
            input.visible = value("visible") != 0;
            const auto result = rr::road_panels(input, value("width"));
            if (result.size() != size_t(value("count"))) throw std::runtime_error(prefix + "count mismatch");
            for (size_t n = 0; n < result.size(); ++n) {
                const auto key = prefix + std::to_string(n) + ".";
                const auto& panel = result[n];
                if (int(panel.material) != fields.at(key + "material") ||
                    panel.pre0 != fields.at(key + "pre0") ||
                    panel.flip_u != bool(fields.at(key + "flip_u")) ||
                    panel.flip_v != bool(fields.at(key + "flip_v")))
                    throw std::runtime_error(key + "metadata mismatch");
                for (int i = 0; i < 4; ++i)
                    if (panel.points[i].x != fields.at(key + "point." + std::to_string(i * 2)) ||
                        panel.points[i].y != fields.at(key + "point." + std::to_string(i * 2 + 1)))
                        throw std::runtime_error(key + "geometry mismatch");
                ++checked;
            }
            if (materials) {
                const auto mode = rr::view_mode(value("draw_high") != 0);
                rr::Framebuffer fb(mode.surface_width, mode.surface_height);
                fb.clear(125);
                rr::render_nodes(fb, rr::road_draw_nodes(result, *materials), mode,
                                 uint32_t(value("draw_mask")));
                const auto raw = std::filesystem::path(path).parent_path() /
                                 (golden.scenario + "-panels" + std::to_string(c) + ".raw");
                std::ifstream reference(raw, std::ios::binary);
                if (!reference) throw std::runtime_error("Missing original road-panel pixels");
                const std::vector<uint8_t> expected((std::istreambuf_iterator<char>(reference)), {});
                if (expected.size() != size_t(fb.width()) * fb.height())
                    throw std::runtime_error("Wrong road-panel frame size");
                for (int y = 0; y < fb.height(); ++y)
                    for (int x = 0; x < fb.width(); ++x)
                        if (fb.row(y)[x] != expected[size_t(y) * fb.width() + x])
                            throw std::runtime_error(prefix + "pixel mismatch at " + std::to_string(x) + "," +
                                                     std::to_string(y));
                pixel_count += expected.size();
            }
        }
        std::cout << pixel_count << " original road-material pixels match\n";
        std::cout << "128 original road-panel cases, " << checked << " panels match\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
