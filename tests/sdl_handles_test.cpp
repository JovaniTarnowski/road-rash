// Plan 28 Task 2: RAII handle test (failing first: header missing).
#include <iostream>
#include <stdexcept>
#include "sdl_handles.hpp"

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << '\n';
        }
    };
    int destroyed = 0;
    {
        // Counting deleter proves ownership runs on the throw path (Review Focus 4).
        struct CountDel {
            int* n;
            void operator()(SDL_Surface* s) const noexcept {
                ++*n;
                SDL_DestroySurface(s);
            }
        };
        try {
            std::unique_ptr<SDL_Surface, CountDel> p(SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_XRGB8888),
                                                     CountDel{&destroyed});
            if (!p) throw std::runtime_error("no surface");
            throw std::runtime_error("device lost");
        } catch (const std::runtime_error&) {
        }
        check(destroyed == 1, "surface destroyed on throw path");
    }
    {
        rr::TexturePtr t; // null reset is safe
        t.reset();
        check(!t, "null texture resets cleanly");
    }
    {
        rr::SurfacePtr s(SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_XRGB8888));
        check(!!s, "surface created");
        s.reset();
        check(!s, "surface resets cleanly");
    }
    if (failures == 0) std::cout << "sdl_handles OK\n";
    return failures ? 1 : 0;
}
