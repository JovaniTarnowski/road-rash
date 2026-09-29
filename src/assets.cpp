#include "assets.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace rr {
Assets::Assets(std::string root) : root_(std::move(root)) {}

std::vector<uint8_t> Assets::read(const std::string& relative) const {
    const std::string path = root_ + "/" + relative;
    size_t size = 0;
    void* bytes = SDL_LoadFile(path.c_str(), &size);
    if (!bytes) throw std::runtime_error(path + ": " + SDL_GetError());
    auto start = static_cast<const uint8_t*>(bytes);
    std::vector<uint8_t> result(start, start + size);
    SDL_free(bytes);
    return result;
}

bool Assets::exists(const std::string& relative) const {
    SDL_IOStream* io = SDL_IOFromFile((root_ + "/" + relative).c_str(), "rb");
    if (!io) return false;
    SDL_CloseIO(io);
    return true;
}
}
