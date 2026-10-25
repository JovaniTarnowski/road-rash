#include "traffic.hpp"
#include "assets.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>
int main(int argc, char** argv) {
    try {
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if (!assets.exists("DATA/CARS/NAPA.CAR")) {
            std::cout << "Imported actor traffic corpus absent\n";
            return 0;
        }
        const rr::VehicleLibrary library(assets.read("DATA/CARS/NAPA.CAR"), assets.read("ROADRASH.EXE"));
        std::ifstream file(argc > 1 ? argv[1]
                                    : RR_SOURCE_DIR "/analysis/oracle/napa-actor-animation-fixtures.tsv");
        if (!file) {
            std::cout << "Original vehicle graphics fixture absent\n";
            return 0;
        }
        std::map<std::string, int64_t> values;
        std::string line, key;
        int64_t value;
        while (std::getline(file, line)) {
            std::istringstream row(line);
            if (row >> key >> value) values[key] = value;
        }
        if (library.vehicles().size() != size_t(values.at("vehicle.graphics.count")))
            throw std::runtime_error("Vehicle count mismatch");
        size_t cells = 0, descriptors = 0, selections = 0;
        for (size_t v = 0; v < library.vehicles().size(); ++v) {
            const auto& vehicle = library.vehicles()[v];
            const auto root = "vehicle.graphics." + std::to_string(v) + ".";
            if (vehicle.mask != uint32_t(values.at(root + "mask")) ||
                vehicle.views.size() != size_t(values.at(root + "groups")))
                throw std::runtime_error(root + "mask/groups mismatch");
            for (size_t g = 0; g < vehicle.views.size(); ++g) {
                const auto prefix = root + std::to_string(g) + ".";
                const auto& group = vehicle.views[g];
                if (group.size() != size_t(values.at(prefix + "count")))
                    throw std::runtime_error(prefix + "view count mismatch");
                for (size_t f = 0; f < group.size(); ++f) {
                    const auto key = prefix + std::to_string(f) + ".";
                    if (group[f].frame != values.at(key + "frame") ||
                        group[f].flags != uint32_t(values.at(key + "flags")))
                        throw std::runtime_error(key + "graphic mismatch");
                    ++cells;
                }
            }
            for (size_t code = 0; code < 3; ++code) {
                const auto prefix = root + "code." + std::to_string(code) + ".";
                const auto& sequence = vehicle.playback.at(code);
                if (bool(sequence) != bool(values.at(prefix + "available")))
                    throw std::runtime_error(prefix + "availability mismatch");
                if (!sequence) continue;
                std::array<uint8_t, 16> bytes{};
                const auto& frame = sequence->at(0);
                std::copy(frame.render.begin(), frame.render.end(), bytes.begin());
                bytes[12] = uint8_t(frame.duration);
                bytes[13] = frame.control;
                for (int b = 0; b < 16; ++b)
                    if (bytes[b] != values.at(prefix + "byte." + std::to_string(b)))
                        throw std::runtime_error(prefix + "descriptor mismatch");
                for (int sample = 0; sample < 48; ++sample) {
                    const auto key = "vehicle.selection." + std::to_string(v) + "." + std::to_string(code) +
                                     "." + std::to_string(sample) + ".";
                    const auto selected =
                        rr::select_actor_view(frame, vehicle.views, int32_t(values.at(key + "yaw")),
                                              int32_t(values.at(key + "roll")));
                    const auto& cell = vehicle.views.at(selected.group).at(selected.subframe);
                    const auto& image = vehicle.animation.frames.at(cell.frame);
                    if (cell.frame != values.at(key + "frame") ||
                        cell.flags != uint32_t(values.at(key + "flags")) ||
                        selected.roll != values.at(key + "selected_roll") ||
                        image.hotspot[0] != values.at(key + "hotspot_x") ||
                        image.hotspot[1] != values.at(key + "hotspot_y"))
                        throw std::runtime_error(key + "view selection mismatch");
                    ++selections;
                }
                ++descriptors;
            }
        }
        std::cout << cells << " original vehicle graphics cells, " << descriptors << " descriptors, "
                  << selections << " view selections and all masks match\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
