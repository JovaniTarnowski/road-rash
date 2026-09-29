#include "resource.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: rr-inspect FILE [FILE...]\n"; return 2; }
    try {
        for (int i = 1; i < argc; ++i) {
            std::ifstream file(argv[i], std::ios::binary);
            if (!file) throw std::runtime_error(std::string("Cannot open ")+argv[i]);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
            auto entries = rr::read_resources(data);
            std::cout << argv[i] << ": " << entries.size() << " resources\n";
            for (const auto& e : entries)
                std::cout << e.tag << '\t' << e.id << '\t' << e.offset << '\t' << e.size << '\n';
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
