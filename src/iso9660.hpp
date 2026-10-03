#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace rr {
class Iso9660 {
public:
    using ReadAt = std::function<size_t(uint64_t, uint8_t*, size_t)>;
    explicit Iso9660(ReadAt);
    std::vector<std::string> list(const std::string& directory) const;
    std::vector<uint8_t> read(const std::string& path) const;
private:
    struct Entry { std::string name; uint32_t sector, size; bool directory; };
    ReadAt read_at_;
    uint64_t volume_size_ = 0;
    Entry root_;
    std::vector<uint8_t> load(const Entry&) const;
    std::vector<Entry> entries(const Entry&) const;
    Entry locate(const std::string&) const;
    Entry record(const uint8_t*, size_t) const;
};
}
