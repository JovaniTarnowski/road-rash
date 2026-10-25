#pragma once
#include <functional>
#include <string>
#include <vector>
namespace rr {
struct ReferenceHash {
    std::string path, hash;
};
struct ImportProgress {
    size_t done, total;
    std::string current;
};
struct ImportResult {
    bool ok = false;
    std::vector<std::string> mismatched, missing;
    std::string error;
};
const std::vector<ReferenceHash>& reference_hashes();
// dest becomes the ROADRASH Assets root only after every reference file passes.
// Existing nonempty destinations are preserved; this API never replaces them.
ImportResult import_game(const std::string& source, const std::string& dest,
                         const std::function<void(const ImportProgress&)>& progress = {});
ImportResult import_game(const std::string& source, const std::string& dest,
                         const std::vector<ReferenceHash>& references,
                         const std::function<void(const ImportProgress&)>& progress = {});
std::string data_root();
} // namespace rr
