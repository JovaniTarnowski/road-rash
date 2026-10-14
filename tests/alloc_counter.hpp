// Plan 29 Task 1: global operator-new counter (include in exactly one TU per binary).
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace rr::test {
inline std::atomic<size_t> allocations{0};
inline std::atomic<bool> counting{false};
struct CountAllocs {               // RAII: counts operator new calls in scope
    CountAllocs() { allocations = 0; counting = true; }
    ~CountAllocs() { counting = false; }
    size_t count() const { return allocations.load(); }
};
}

#ifdef RR_ALLOC_COUNTER_DEFINE
void* operator new(std::size_t n) {
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    if (rr::test::counting.load()) rr::test::allocations.fetch_add(1);
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
#endif
