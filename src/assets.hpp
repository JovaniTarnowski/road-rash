#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rr {
// Original game files addressed by path relative to the ROADRASH directory, e.g. "DATA/GLOBAL.RSC".
class Assets {
public:
    explicit Assets(std::string root);
    std::vector<uint8_t> read(const std::string& relative) const;
    bool exists(const std::string& relative) const;
    const std::string& root() const { return root_; }
private:
    std::string root_;
};
}
