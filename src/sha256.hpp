#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace rr {
std::string sha256(const std::vector<uint8_t>&);
// Incremental SHA-256 (plan 23 Task 4: the import job hashes ≤1 MiB ranges
// as they arrive instead of buffering whole files twice). finish() appends
// padding and returns the hex digest; call it once.
class Sha256Stream {
  public:
    Sha256Stream();
    void write(const uint8_t* data, size_t size);
    std::string finish();

  private:
    std::array<uint32_t, 8> h_;
    uint64_t total_ = 0;
    uint8_t buf_[64]{};
    size_t buffered_ = 0;
    bool finished_ = false;
    void compress(const uint8_t block[64]);
};
} // namespace rr
