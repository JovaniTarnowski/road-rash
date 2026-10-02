#pragma once
// Reader for oracle golden files (docs/plans/01-foundation-and-oracle.md, Task 3).
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rr::golden {
constexpr int skip = 77;
struct File {
    std::string exe, scenario;
    std::vector<std::map<std::string, int64_t>> frames;
};

inline File parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line != "# rr-oracle 1") throw std::runtime_error("Golden file: bad magic");
    File f;
    bool ended = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (ended) throw std::runtime_error("Golden file: data after end");
        if (line == "end") { ended = true; continue; }
        const auto tab = line.find('\t');
        if (tab == std::string::npos) throw std::runtime_error("Golden file: missing tab: " + line);
        const std::string key = line.substr(0, tab), value = line.substr(tab + 1);
        if (key == "exe") { f.exe = value; continue; }
        if (key == "scenario") { f.scenario = value; continue; }
        if (key == "frame") { f.frames.emplace_back(); continue; }
        if (f.frames.empty()) throw std::runtime_error("Golden file: key before frame: " + key);
        char* end = nullptr;
        const long long v = std::strtoll(value.c_str(), &end, 10);
        if (value.empty() || *end) throw std::runtime_error("Golden file: not an integer: " + line);
        f.frames.back()[key] = v;
    }
    if (!ended) throw std::runtime_error("Golden file: truncated (no end marker)");
    return f;
}

inline File load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open golden file " + path);
    std::ostringstream s;
    s << in.rdbuf();
    return parse(s.str());
}

inline std::string oracle_dir() {
    if (const char* d = std::getenv("RR_ORACLE_DIR")) return d;
    return std::string(RR_SOURCE_DIR) + "/analysis/oracle";
}
}
