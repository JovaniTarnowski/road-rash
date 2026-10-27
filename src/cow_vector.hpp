// Plan 34 Task 3: a fixed-size vector stored in shared chunks. Copies share
// every chunk; the first write to a shared chunk clones only that chunk, so
// race snapshots copy a few pointers instead of the whole table.
#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace rr {
template <class T, size_t N = 128> class CowVector {
  public:
    void assign(size_t n, const T& value) {
        size_ = n;
        chunks_.clear();
        for (size_t i = 0; i < (n + N - 1) / N; ++i) {
            auto c = std::make_shared<std::array<T, N>>();
            c->fill(value);
            chunks_.push_back(std::move(c));
        }
    }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    const T& operator[](size_t i) const { return (*chunks_[i / N])[i % N]; }
    T& mut(size_t i) {
        auto& c = chunks_[i / N];
        if (c.use_count() > 1) c = std::make_shared<std::array<T, N>>(*c);
        return (*c)[i % N];
    }

  private:
    std::vector<std::shared_ptr<std::array<T, N>>> chunks_;
    size_t size_ = 0;
};
} // namespace rr
