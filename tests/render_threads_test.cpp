#include "render_threads.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    for (int threads : {1, 2, 4}) {
        rr::RenderThreads pool(threads);
        std::string message;
        try {
            pool.run(4, 100, [](int band, int, int) {
                if (band == 1 || band == 2) throw std::runtime_error("band " + std::to_string(band));
            });
        } catch (const std::runtime_error& e) { message = e.what(); }
        check(message == "band 1" || message == "band 2", "one band error reaches the caller");
        std::atomic<int> ran{0};
        pool.run(4, 100, [&](int, int, int) { ++ran; });
        check(ran == 4, "pool usable after a failed run");
    }
    if (failures == 0) std::cout << "render_threads OK\n";
    return failures ? 1 : 0;
}
