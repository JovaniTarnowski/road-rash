#include "importer.hpp"
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: rr-import SOURCE.iso_OR_FOLDER DESTINATION\n";
        return 2;
    }
    const auto result = rr::import_game(argv[1], argv[2], [](const rr::ImportProgress& p) {
        std::cout << p.done << '/' << p.total << ' ' << p.current << '\n';
    });
    for (const auto& path : result.missing) std::cerr << "Missing: " << path << '\n';
    for (const auto& path : result.mismatched) std::cerr << "Hash mismatch: " << path << '\n';
    if (!result.error.empty()) std::cerr << result.error << '\n';
    return result.ok ? 0 : 1;
}
