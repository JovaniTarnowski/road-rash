#include "road.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

static std::vector<uint8_t> load(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error(std::string("Cannot open ") + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: rr-course [--exe ROADRASH.EXE] FILE [FILE...]\n";
        return 2;
    }
    try {
        int i = 1;
        rr::DepthTable table;
        if (std::string(argv[1]) == "--exe" && argc > 3) {
            table = rr::read_depth_table(load(argv[2]));
            i = 3;
        }
        for (; i < argc; ++i) {
            auto c = rr::read_course(load(argv[i]));
            auto a = rr::make_route(c, false), b = rr::make_route(c, true);
            std::cout << argv[i] << ": " << c.nodes.size() << " nodes, " << c.sections.size() << " sections, "
                      << a.size() << " / " << b.size() << " route samples";
            for (const auto* route : {&a, &b}) {
                // Dual windows; separation must close to zero at each merge.
                size_t dual = 0, splits = 0, windows = 0, merges = 0, open = 0;
                for (size_t k = 0; k < route->size(); ++k) {
                    const auto& s = (*route)[k];
                    dual += s.dual;
                    splits += s.event == rr::RoadEvent::split;
                    windows += s.event == rr::RoadEvent::merge_window;
                    if (s.event == rr::RoadEvent::merged) {
                        ++merges;
                        if (k && (*route)[k - 1].dual && (*route)[k - 1].x_sep != 0) ++open;
                    }
                }
                std::cout << "; " << (route == &a ? "A" : "B") << ": " << splits << " splits, " << merges
                          << " merges (" << windows << " restored), " << dual << " dual samples [";
                for (size_t k = 0; k < route->size(); ++k)
                    if ((*route)[k].event == rr::RoadEvent::split)
                        std::cout << " split@" << k;
                    else if ((*route)[k].event == rr::RoadEvent::merged)
                        std::cout << " merge@" << k;
                std::cout << " ]";
                if (open) throw std::runtime_error("Road separation not closed at merge");
            }
            if (!table.scale.empty()) {
                // Project every Q8 camera position of both routes and report the screen extent.
                int64_t low = INT64_MAX, high = INT64_MIN;
                size_t views = 0;
                for (const auto* route : {&a, &b})
                    for (uint32_t p = 0; p < (route->size() << 8); p += 16, ++views)
                        for (const auto& s : rr::project_road(*route, p, {}, table))
                            for (int r = 0; r < s.count; ++r) {
                                low = std::min<int64_t>(low, s.road[r].screen_y >> 16);
                                high = std::max<int64_t>(high, s.road[r].screen_y >> 16);
                            }
                std::cout << ", " << views << " projected views, road rows " << low << ".." << high;
            }
            std::cout << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
